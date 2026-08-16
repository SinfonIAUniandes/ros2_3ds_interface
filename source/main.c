#include <3ds.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logging/app_log.h"
#include "dds_runtime.h"

#define SOC_BUFFER_SIZE 0x100000
#define MULTICAST_GROUP "239.255.0.1"
#define DEFAULT_PORT 7410
#define DEFAULT_SEND_INTERVAL_MS 1000
#define MAX_PACKET_SIZE 512
#define STATUS_REFRESH_MS 500
#define APP_BUILD_ID "20260816-iovec-sendmsg"

typedef struct {
    char peer_ip[INET_ADDRSTRLEN];
    u16 port;
    u32 send_interval_ms;
    u32 domain_id;
    bool dds_enabled;
} probe_config;

typedef struct {
    int socket_fd;
    int socket_error;
    int reuse_error;
    int bind_error;
    int nonblocking_error;
    int membership_error;
    int loopback_error;
    int last_send_error;
    int last_receive_error;
    bool membership_joined;
    u32 multicast_sent;
    u32 unicast_sent;
    u32 received;
    char last_sender[INET_ADDRSTRLEN];
    char last_payload[65];
} probe_state;

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }

    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return text;
}

static void load_config(probe_config *config) {
    memset(config, 0, sizeof(*config));
    config->port = DEFAULT_PORT;
    config->send_interval_ms = DEFAULT_SEND_INTERVAL_MS;
    config->domain_id = 0;
    config->dds_enabled = true;

    FILE *file = fopen("romfs:/config.ini", "r");
    if (!file) {
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#') {
            continue;
        }

        char *separator = strchr(entry, '=');
        if (!separator) {
            continue;
        }

        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        if (strcmp(key, "peer_ip") == 0) {
            strncpy(config->peer_ip, value, sizeof(config->peer_ip) - 1);
        } else if (strcmp(key, "port") == 0) {
            long port = strtol(value, NULL, 10);
            if (port > 0 && port <= 65535) {
                config->port = (u16)port;
            }
        } else if (strcmp(key, "send_interval_ms") == 0) {
            long interval = strtol(value, NULL, 10);
            if (interval >= 100 && interval <= 60000) {
                config->send_interval_ms = (u32)interval;
            }
        } else if (strcmp(key, "domain_id") == 0) {
            long domain_id = strtol(value, NULL, 10);
            if (domain_id >= 0 && domain_id <= 232) {
                config->domain_id = (u32)domain_id;
            }
        } else if (strcmp(key, "dds_enabled") == 0) {
            config->dds_enabled = strtol(value, NULL, 10) != 0;
        }
    }
    fclose(file);
}

static int set_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

static bool get_local_network(struct in_addr *ip, struct in_addr *netmask, struct in_addr *broadcast) {
    return SOCU_GetIPInfo(ip, netmask, broadcast) == 0 && ip->s_addr != 0;
}

static void initialize_probe(probe_state *state, const probe_config *config, struct in_addr local_ip) {
    memset(state, 0, sizeof(*state));
    state->socket_fd = -1;
    strcpy(state->last_sender, "-");
    strcpy(state->last_payload, "-");

    state->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (state->socket_fd < 0) {
        state->socket_error = errno;
        app_log_write(APP_LOG_ERROR, "UDP socket failed errno=%d", state->socket_error);
        return;
    }

    int enabled = 1;
    if (setsockopt(state->socket_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        state->reuse_error = errno;
    }

    struct sockaddr_in local_address;
    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(config->port);
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(state->socket_fd, (struct sockaddr *)&local_address, sizeof(local_address)) < 0) {
        state->bind_error = errno;
        app_log_write(APP_LOG_ERROR, "UDP bind %u failed errno=%d", config->port, state->bind_error);
    }

    if (set_nonblocking(state->socket_fd) < 0) {
        state->nonblocking_error = errno;
    }

    struct ip_mreq membership;
    memset(&membership, 0, sizeof(membership));
    inet_pton(AF_INET, MULTICAST_GROUP, &membership.imr_multiaddr);
    membership.imr_interface = local_ip;
    if (setsockopt(state->socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) < 0) {
        state->membership_error = errno;
        app_log_write(APP_LOG_WARN, "Multicast join failed errno=%d", state->membership_error);
    } else {
        state->membership_joined = true;
        app_log_write(APP_LOG_INFO, "Joined multicast %s", MULTICAST_GROUP);
    }

    unsigned char loopback = 1;
    if (setsockopt(state->socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0) {
        state->loopback_error = errno;
    }
}

static void send_probe(probe_state *state, const struct sockaddr_in *destination, bool multicast) {
    char packet[128];
    int packet_size = snprintf(packet, sizeof(packet), "ROS2_3DS_PROBE seq=%lu time=%llu",
                               (unsigned long)(state->multicast_sent + 1),
                               (unsigned long long)osGetTime());
    if (packet_size < 0) {
        return;
    }

    ssize_t sent = sendto(state->socket_fd, packet, (size_t)packet_size, 0,
                          (const struct sockaddr *)destination, sizeof(*destination));
    if (sent < 0) {
        state->last_send_error = errno;
        app_log_write(APP_LOG_ERROR, "Probe TX failed errno=%d", state->last_send_error);
    } else if (multicast) {
        state->multicast_sent++;
    } else {
        state->unicast_sent++;
    }
}

static void receive_probes(probe_state *state) {
    for (int attempt = 0; attempt < 16; attempt++) {
        char packet[MAX_PACKET_SIZE];
        struct sockaddr_in sender;
        socklen_t sender_size = sizeof(sender);
        ssize_t received = recvfrom(state->socket_fd, packet, sizeof(packet) - 1, 0,
                                    (struct sockaddr *)&sender, &sender_size);
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                state->last_receive_error = errno;
            }
            break;
        }

        packet[received] = '\0';
        state->received++;
        const char *sender_ip = inet_ntoa(sender.sin_addr);
        if (sender_ip) {
            strncpy(state->last_sender, sender_ip, sizeof(state->last_sender) - 1);
            state->last_sender[sizeof(state->last_sender) - 1] = '\0';
        }
        strncpy(state->last_payload, packet, sizeof(state->last_payload) - 1);
        state->last_payload[sizeof(state->last_payload) - 1] = '\0';
         app_log_write(APP_LOG_INFO, "RX %.44s", state->last_payload);
    }
}

    static void dds_log_callback(void *context, int level, const char *message) {
        (void)context;
        app_log_level app_level = level == APP_LOG_ERROR ? APP_LOG_ERROR
            : level == APP_LOG_WARN ? APP_LOG_WARN : APP_LOG_DDS;
        app_log_write(app_level, "DDS %s", message);
    }

    static void draw_static_status(PrintConsole *console, const char *local_ip,
                       bool network_ready, int soc_result) {
        consoleSelect(console);
        printf("\x1b[0;0HROS 2 / DDS native runtime        \n");
        printf("\x1b[1;0HIP %-15s  soc %-4s  net %-4s  \n", local_ip,
            soc_result == 0 ? "OK" : "FAIL", network_ready ? "OK" : "FAIL");
        printf("\x1b[10;0HLog: /3ds/ros2_3ds_interface/logs\n");
        printf("\x1b[11;0Hruntime-YYYYMMDD.log on SD         \n");
    }

    static void draw_dynamic_status(PrintConsole *console, const probe_config *config,
                        const probe_state *state, const dds_runtime *dds) {
        consoleSelect(console);
        printf("\x1b[2;0HDDS %-7s domain %-3lu rc %-8ld\n", dds_runtime_status(dds),
            (unsigned long)config->domain_id, (long)dds->last_result);
        printf("\x1b[3;0H%-38.38s\n", dds_runtime_error_text(dds));
        printf("\x1b[4;0HUDP %-4s bind %-4s join %-4s loop %-4s\n",
            state->socket_fd >= 0 ? "OK" : "FAIL", state->bind_error == 0 ? "OK" : "FAIL",
            state->membership_joined ? "OK" : "FAIL", state->loopback_error == 0 ? "OK" : "FAIL");
        printf("\x1b[5;0HProbe %u every %lums peer %.15s\n", config->port,
            (unsigned long)config->send_interval_ms, config->peer_ip[0] ? config->peer_ip : "disabled");
        printf("\x1b[6;0HTX M:%-6lu U:%-6lu RX:%-6lu     \n", (unsigned long)state->multicast_sent,
            (unsigned long)state->unicast_sent, (unsigned long)state->received);
        printf("\x1b[7;0HLast %-32.32s\n", state->last_sender);
        printf("\x1b[8;0HData %-32.32s\n", state->last_payload);
        printf("\x1b[9;0HLog %-5s A send START exit           \n", app_log_has_error() ? "ERROR" : "OK");
}

int main(void) {
    gfxInitDefault();
    PrintConsole status_console;
    PrintConsole log_console;
    consoleInit(GFX_TOP, &status_console);
    consoleInit(GFX_BOTTOM, &log_console);
    romfsInit();
    app_log_init(&log_console);
    app_log_write(APP_LOG_INFO, "Application started build=%s", APP_BUILD_ID);

    probe_config config;
    load_config(&config);

    void *soc_buffer = memalign(0x1000, SOC_BUFFER_SIZE);
    int soc_result = soc_buffer ? (int)socInit((u32 *)soc_buffer, SOC_BUFFER_SIZE) : -1;
    app_log_write(soc_result == 0 ? APP_LOG_INFO : APP_LOG_ERROR, "socInit result=0x%08X", (unsigned int)soc_result);

    struct in_addr local_ip;
    struct in_addr netmask;
    struct in_addr broadcast;
    memset(&local_ip, 0, sizeof(local_ip));
    bool network_ready = soc_result == 0 && get_local_network(&local_ip, &netmask, &broadcast);
    const char *ip_text = network_ready ? inet_ntoa(local_ip) : NULL;
    char local_ip_text[INET_ADDRSTRLEN] = "unavailable";
    if (ip_text) {
        strncpy(local_ip_text, ip_text, sizeof(local_ip_text) - 1);
    }
    draw_static_status(&status_console, local_ip_text, network_ready, soc_result);

    probe_state state;
    memset(&state, 0, sizeof(state));
    state.socket_fd = -1;
    if (network_ready) {
        app_log_write(APP_LOG_INFO, "Network ready: %s", local_ip_text);
        initialize_probe(&state, &config, local_ip);
    } else {
        app_log_write(APP_LOG_ERROR, "No active IPv4 network");
    }

    dds_runtime dds;
    dds_runtime_init(&dds);
    dds_runtime_set_log_sink(dds_log_callback, NULL);
    if (network_ready && config.dds_enabled) {
        bool started = dds_runtime_start(&dds, config.domain_id);
        app_log_write(started ? APP_LOG_INFO : APP_LOG_ERROR, "DDS participant %s rc=%ld %s",
                      started ? "started" : "failed", (long)dds.last_result,
                      dds_runtime_error_text(&dds));
    }

    struct sockaddr_in multicast_destination;
    memset(&multicast_destination, 0, sizeof(multicast_destination));
    multicast_destination.sin_family = AF_INET;
    multicast_destination.sin_port = htons(config.port);
    inet_pton(AF_INET, MULTICAST_GROUP, &multicast_destination.sin_addr);

    struct sockaddr_in unicast_destination;
    memset(&unicast_destination, 0, sizeof(unicast_destination));
    unicast_destination.sin_family = AF_INET;
    unicast_destination.sin_port = htons(config.port);
    bool peer_valid = config.peer_ip[0] && inet_pton(AF_INET, config.peer_ip, &unicast_destination.sin_addr) == 1;

    u64 next_send_at = 0;
    u64 next_status_at = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 keys_down = hidKeysDown();
        if (keys_down & KEY_START) {
            app_log_write(APP_LOG_INFO, "Exit requested");
            break;
        }

        u64 now = osGetTime();
        if (state.socket_fd >= 0 && (now >= next_send_at || (keys_down & KEY_A))) {
            send_probe(&state, &multicast_destination, true);
            if (peer_valid) {
                send_probe(&state, &unicast_destination, false);
            }
            next_send_at = now + config.send_interval_ms;
        }
        if (state.socket_fd >= 0) {
            receive_probes(&state);
        }

        if (now >= next_status_at) {
            draw_dynamic_status(&status_console, &config, &state, &dds);
            next_status_at = now + STATUS_REFRESH_MS;
        }
        app_log_render();
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    dds_runtime_stop(&dds);
    app_log_write(APP_LOG_INFO, "DDS participant stopped rc=%ld", (long)dds.last_result);
    if (state.socket_fd >= 0) {
        if (state.membership_joined) {
            struct ip_mreq membership;
            memset(&membership, 0, sizeof(membership));
            inet_pton(AF_INET, MULTICAST_GROUP, &membership.imr_multiaddr);
            membership.imr_interface = local_ip;
            setsockopt(state.socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &membership, sizeof(membership));
        }
        close(state.socket_fd);
    }
    if (soc_result == 0) {
        socExit();
    }
    free(soc_buffer);
    app_log_write(APP_LOG_INFO, "Application stopped");
    app_log_render();
    app_log_close();
    romfsExit();
    gfxExit();
    return 0;
}
