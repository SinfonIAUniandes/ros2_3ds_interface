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
#define DEFAULT_PORT 17650
#define DEFAULT_SEND_INTERVAL_MS 1000
#define MAX_PACKET_SIZE 512
#define STATUS_REFRESH_MS 500
#define GRAPH_REFRESH_MS 5000
#define RTPS_LOG_INTERVAL_MS 5000
#define APP_BUILD_ID "20260816-subnet-discovery"

typedef struct {
    char peer_ip[INET_ADDRSTRLEN];
    u16 port;
    u32 send_interval_ms;
    u32 domain_id;
    bool dds_enabled;
    bool peer_ip_invalid;
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

static void load_config_defaults(probe_config *config) {
    memset(config, 0, sizeof(*config));
    config->port = DEFAULT_PORT;
    config->send_interval_ms = DEFAULT_SEND_INTERVAL_MS;
    config->domain_id = 0;
    config->dds_enabled = true;
}

static bool load_config_file(probe_config *config, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
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
            struct in_addr peer_address;
            if (*value != '\0' && inet_pton(AF_INET, value, &peer_address) == 1) {
                strncpy(config->peer_ip, value, sizeof(config->peer_ip) - 1);
                config->peer_ip_invalid = false;
            } else if (*value != '\0') {
                config->peer_ip[0] = '\0';
                config->peer_ip_invalid = true;
            } else {
                config->peer_ip[0] = '\0';
                config->peer_ip_invalid = false;
            }
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
    return true;
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

    static void chatter_receive_callback(void *context, const char *data) {
        (void)context;
        app_log_write(APP_LOG_INFO, "ROS REMOTE RX %s", data);
    }

    static void publish_chatter(dds_runtime *dds, u32 *sequence) {
        char payload[64];
        snprintf(payload, sizeof(payload), "Hello from 3DS: %lu", (unsigned long)(*sequence)++);
        if (dds_runtime_publish_chatter(dds, payload)) {
            app_log_write(APP_LOG_INFO, "ROS TX %s", payload);
        } else {
            app_log_write(APP_LOG_ERROR, "ROS TX failed %s", dds_runtime_error_text(dds));
        }
    }

    static void draw_static_status(PrintConsole *console, const probe_config *config,
                       const char *local_ip, bool network_ready, int soc_result) {
        consoleSelect(console);
        printf("\x1b[0;0HROS 2 / DDS native runtime        \n");
        printf("\x1b[1;0HIP %-15s  soc %-4s  net %-4s  \n", local_ip,
            soc_result == 0 ? "OK" : "FAIL", network_ready ? "OK" : "FAIL");
        printf("\x1b[10;0HLog: /3ds/ros2_3ds_interface/logs/\n");
        printf("\x1b[11;0HYYYYMMDD/session-HHMMSS-mmm.log     \n");
        if (config->peer_ip[0] != '\0') {
            printf("\x1b[16;0HDISC: PEER %s\n", config->peer_ip);
        } else {
            printf("\x1b[16;0HDISC: MCAST\n");
        }
        printf("\x1b[17;0H%.39s\n", APP_BUILD_ID);
    }

    static void draw_dynamic_status(PrintConsole *console, const probe_config *config,
                        const probe_state *state, dds_runtime *dds, bool chatter_publishing,
                        bool chatter_listening, int32_t graph_matches, int32_t writer_matches,
                        int32_t reader_matches, int32_t writer_qos_rejections,
                        int32_t reader_qos_rejections, const ddsrt_3ds_socket_stats_t *socket_stats) {
        consoleSelect(console);
        printf("\x1b[2;0HDDS %-7s domain %-3lu rc %-8ld\n", dds_runtime_status(dds),
            (unsigned long)config->domain_id, (long)dds->last_result);
        printf("\x1b[3;0H%-38.38s\n", dds_runtime_error_text(dds));
        printf("\x1b[4;0HUDP %-4s bind %-4s join %-4s loop %-4s\n",
            state->socket_fd >= 0 ? "OK" : "FAIL", state->bind_error == 0 ? "OK" : "FAIL",
            state->membership_joined ? "OK" : "FAIL", state->loopback_error == 0 ? "OK" : "FAIL");
        printf("\x1b[5;0HROS TX:%-8llu RX:%-8llu\n",
            (unsigned long long)dds_runtime_chatter_transmitted(dds),
            (unsigned long long)dds_runtime_chatter_received(dds));
        printf("\x1b[6;0HGRAPH P:%-8llu M:%-14ld\n", (unsigned long long)dds_runtime_graph_published(dds),
            (long)graph_matches);
        printf("\x1b[7;0HMATCH W:%ld R:%ld QW:%ld QR:%ld\n", (long)writer_matches,
            (long)reader_matches, (long)writer_qos_rejections, (long)reader_qos_rejections);
        printf("\x1b[8;0HA send B 1Hz:%-3s Y listen:%-3s X UDP\n",
            chatter_publishing ? "ON" : "OFF", chatter_listening ? "ON" : "OFF");
        printf("\x1b[9;0HUDP M:%-6lu U:%-6lu RX:%-6lu\n", (unsigned long)state->multicast_sent,
            (unsigned long)state->unicast_sent, (unsigned long)state->received);
        printf("\x1b[12;0HRTPS TXM:%lu TXU:%lu\n",
            (unsigned long)socket_stats->tx_multicast, (unsigned long)socket_stats->tx_unicast);
        printf("\x1b[13;0H     RX:%lu REM:%lu\n",
            (unsigned long)socket_stats->rx_total, (unsigned long)socket_stats->rx_remote);
        printf("\x1b[14;0HERR TX:%ld RX:%ld MIF:%ld\n", (long)socket_stats->last_send_errno,
            (long)socket_stats->last_recv_errno, (long)socket_stats->multicast_if_errno);
        printf("\x1b[15;0HLast %-32.32s\n", state->last_sender);
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
    load_config_defaults(&config);
    load_config_file(&config, "romfs:/config.ini");
    bool external_config_loaded = load_config_file(&config, "sdmc:/3ds/ros2_3ds_interface/config.ini");
    app_log_write(APP_LOG_INFO, external_config_loaded ? "External SD config loaded"
                  : "Using built-in ROMFS config/defaults");
    if (config.peer_ip_invalid) {
        app_log_write(APP_LOG_WARN, "Invalid peer_ip ignored; using multicast discovery");
    }
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
    char broadcast_ip_text[INET_ADDRSTRLEN] = "";
    if (ip_text) {
        strncpy(local_ip_text, ip_text, sizeof(local_ip_text) - 1);
    }
    if (network_ready) {
        inet_ntop(AF_INET, &broadcast, broadcast_ip_text, sizeof(broadcast_ip_text));
    }
    draw_static_status(&status_console, &config, local_ip_text, network_ready, soc_result);

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
    int32_t last_graph_matches = -2;
    int32_t last_writer_matches = -2;
    int32_t last_reader_matches = -2;
    int32_t last_writer_qos_rejections = -2;
    int32_t last_reader_qos_rejections = -2;
    int32_t last_send_errno = 0;
    int32_t last_recv_errno = 0;
    int32_t last_multicast_if_errno = 0;
    if (network_ready && config.dds_enabled) {
        if (config.peer_ip[0] != '\0') {
            app_log_write(APP_LOG_INFO, "DDS discovery: multicast + static peer %s", config.peer_ip);
        } else {
            app_log_write(APP_LOG_INFO, "DDS discovery: multicast + subnet broadcast %s",
                          broadcast_ip_text);
        }
        app_log_write(APP_LOG_INFO, "DDS compatibility: RTPS 2.1");
        bool started = dds_runtime_start(&dds, config.domain_id, config.peer_ip,
                                         broadcast_ip_text);
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

    bool chatter_publishing = false;
    bool chatter_listening = true;
    u32 chatter_sequence = 0;
    u64 next_chatter_at = 0;
    u64 next_graph_at = osGetTime() + GRAPH_REFRESH_MS;
    u64 next_status_at = 0;
    u64 next_rtps_log_at = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 keys_down = hidKeysDown();
        if (keys_down & KEY_START) {
            app_log_write(APP_LOG_INFO, "Exit requested");
            break;
        }

        u64 now = osGetTime();
        if (keys_down & KEY_A) {
            publish_chatter(&dds, &chatter_sequence);
        }
        if (keys_down & KEY_B) {
            chatter_publishing = !chatter_publishing;
            next_chatter_at = now;
            app_log_write(APP_LOG_INFO, "ROS 1Hz chatter %s", chatter_publishing ? "enabled" : "disabled");
        }
        if (keys_down & KEY_Y) {
            chatter_listening = !chatter_listening;
            app_log_write(APP_LOG_INFO, "ROS listener %s", chatter_listening ? "enabled" : "disabled");
        }
        if (chatter_publishing && now >= next_chatter_at) {
            publish_chatter(&dds, &chatter_sequence);
            next_chatter_at = now + config.send_interval_ms;
        }
        if (dds.running && now >= next_graph_at) {
            if (dds_runtime_refresh_graph(&dds)) {
                app_log_write(APP_LOG_DEBUG, "ROS graph refreshed");
            } else {
                app_log_write(APP_LOG_ERROR, "ROS graph failed %s", dds_runtime_error_text(&dds));
            }
            next_graph_at = now + GRAPH_REFRESH_MS;
        }
        int32_t graph_matches = dds_runtime_graph_writer_matches(&dds);
        if (graph_matches != last_graph_matches) {
            if (graph_matches >= 0) {
                app_log_write(APP_LOG_INFO, "ROS graph match count=%ld", (long)graph_matches);
            } else {
                app_log_write(APP_LOG_ERROR, "ROS graph match count=%ld %s", (long)graph_matches,
                              dds_runtime_error_text(&dds));
            }
            last_graph_matches = graph_matches;
        }
        int32_t writer_matches = dds_runtime_chatter_writer_matches(&dds);
        if (writer_matches != last_writer_matches) {
            if (writer_matches >= 0) {
                app_log_write(APP_LOG_INFO, "ROS writer match count=%ld", (long)writer_matches);
            } else {
                app_log_write(APP_LOG_ERROR, "ROS writer match count=%ld %s", (long)writer_matches,
                              dds_runtime_error_text(&dds));
            }
            last_writer_matches = writer_matches;
        }
        int32_t reader_matches = dds_runtime_chatter_reader_matches(&dds);
        if (reader_matches != last_reader_matches) {
            if (reader_matches >= 0) {
                app_log_write(APP_LOG_INFO, "ROS reader match count=%ld", (long)reader_matches);
            } else {
                app_log_write(APP_LOG_ERROR, "ROS reader match count=%ld %s", (long)reader_matches,
                              dds_runtime_error_text(&dds));
            }
            last_reader_matches = reader_matches;
        }
        uint32_t writer_qos_policy = 0;
        int32_t writer_qos_rejections = dds_runtime_chatter_writer_incompatible_qos(
            &dds, &writer_qos_policy);
        if (writer_qos_rejections != last_writer_qos_rejections) {
            if (writer_qos_rejections > 0) {
                app_log_write(APP_LOG_WARN, "ROS writer incompatible QoS count=%ld policy=%lu",
                              (long)writer_qos_rejections, (unsigned long)writer_qos_policy);
            }
            last_writer_qos_rejections = writer_qos_rejections;
        }
        uint32_t reader_qos_policy = 0;
        int32_t reader_qos_rejections = dds_runtime_chatter_reader_incompatible_qos(
            &dds, &reader_qos_policy);
        if (reader_qos_rejections != last_reader_qos_rejections) {
            if (reader_qos_rejections > 0) {
                app_log_write(APP_LOG_WARN, "ROS reader incompatible QoS count=%ld policy=%lu",
                              (long)reader_qos_rejections, (unsigned long)reader_qos_policy);
            }
            last_reader_qos_rejections = reader_qos_rejections;
        }
        if (state.socket_fd >= 0 && (keys_down & KEY_X)) {
            send_probe(&state, &multicast_destination, true);
            if (peer_valid) {
                send_probe(&state, &unicast_destination, false);
            }
        }
        if (state.socket_fd >= 0) {
            receive_probes(&state);
        }
        if (chatter_listening) {
            dds_runtime_poll_chatter(&dds, chatter_receive_callback, NULL);
        }

        if (now >= next_status_at) {
            ddsrt_3ds_socket_stats_t socket_stats;
            dds_runtime_socket_stats(&socket_stats);
            if (socket_stats.last_send_errno != last_send_errno ||
                socket_stats.last_recv_errno != last_recv_errno) {
                app_log_write(APP_LOG_WARN, "RTPS socket errno tx=%ld rx=%ld",
                              (long)socket_stats.last_send_errno,
                              (long)socket_stats.last_recv_errno);
                last_send_errno = socket_stats.last_send_errno;
                last_recv_errno = socket_stats.last_recv_errno;
            }
            if (socket_stats.multicast_if_errno != last_multicast_if_errno) {
                app_log_write(APP_LOG_WARN,
                              "RTPS IP_MULTICAST_IF errno=%ld; using active Wi-Fi route",
                              (long)socket_stats.multicast_if_errno);
                last_multicast_if_errno = socket_stats.multicast_if_errno;
            }
            if (now >= next_rtps_log_at) {
                app_log_write(APP_LOG_INFO, "RTPS tx_m=%lu tx_u=%lu rx=%lu remote=%lu",
                              (unsigned long)socket_stats.tx_multicast,
                              (unsigned long)socket_stats.tx_unicast,
                              (unsigned long)socket_stats.rx_total,
                              (unsigned long)socket_stats.rx_remote);
                next_rtps_log_at = now + RTPS_LOG_INTERVAL_MS;
            }
            draw_dynamic_status(&status_console, &config, &state, &dds, chatter_publishing,
                                chatter_listening, graph_matches, writer_matches, reader_matches,
                                writer_qos_rejections, reader_qos_rejections, &socket_stats);
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
