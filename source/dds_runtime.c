#include "dds_runtime.h"

#include <dds/dds.h>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static dds_runtime_log_fn g_log_callback;
static void *g_log_context;

static void dds_log_sink(void *context, const dds_log_data_t *data) {
    (void)context;
    if (g_log_callback == NULL || data == NULL || data->message == NULL) {
        return;
    }

    char message[160];
    size_t length = data->size < sizeof(message) - 1 ? data->size : sizeof(message) - 1;
    memcpy(message, data->message, length);
    while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r')) {
        length--;
    }
    message[length] = '\0';
    int level = (data->priority & DDS_LC_ERROR) || (data->priority & DDS_LC_FATAL)
        ? 3 : (data->priority & DDS_LC_WARNING) ? 2 : 4;
    g_log_callback(g_log_context, level, message);
}

void dds_runtime_init(dds_runtime *runtime) {
    runtime->domain = DDS_ENTITY_NIL;
    runtime->participant = DDS_ENTITY_NIL;
    runtime->last_result = DDS_RETCODE_OK;
    runtime->running = false;
    ros2_chatter_init(&runtime->chatter);
    ros2_graph_init(&runtime->graph);
}

void dds_runtime_set_log_sink(dds_runtime_log_fn callback, void *context) {
    g_log_callback = callback;
    g_log_context = context;
    dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_INFO);
    dds_set_log_sink(dds_log_sink, NULL);
    dds_set_trace_sink(NULL, NULL);
}

bool dds_runtime_start(dds_runtime *runtime, uint32_t domain_id, const char *peer_ip,
                       const char *broadcast_ip) {
    if (runtime->running) {
        return true;
    }

    const char *domain_config =
        "<CycloneDDS><Domain Id=\"any\"><Compatibility><ProtocolVersion>2.1</ProtocolVersion>"
        "</Compatibility></Domain></CycloneDDS>";
    char peer_domain_config[448];
    if ((peer_ip != NULL && peer_ip[0] != '\0') ||
        (broadcast_ip != NULL && broadcast_ip[0] != '\0')) {
        const bool explicit_peer = peer_ip != NULL && peer_ip[0] != '\0';
        const char *address = explicit_peer ? peer_ip : broadcast_ip;
        struct in_addr parsed_address;
        if (inet_pton(AF_INET, address, &parsed_address) != 1) {
            runtime->last_result = DDS_RETCODE_BAD_PARAMETER;
            return false;
        }
        char peer_locator[INET_ADDRSTRLEN + 8];
        int locator_length;
        if (explicit_peer) {
            locator_length = snprintf(peer_locator, sizeof(peer_locator), "%s", address);
        } else {
            uint32_t discovery_port = 7400u + 250u * domain_id;
            if (discovery_port > 65535u) {
                runtime->last_result = DDS_RETCODE_BAD_PARAMETER;
                return false;
            }
            locator_length = snprintf(peer_locator, sizeof(peer_locator), "%s:%lu", address,
                                      (unsigned long)discovery_port);
        }
        if (locator_length < 0 || (size_t)locator_length >= sizeof(peer_locator)) {
            runtime->last_result = DDS_RETCODE_BAD_PARAMETER;
            return false;
        }
        int config_length = snprintf(peer_domain_config, sizeof(peer_domain_config),
            "<CycloneDDS><Domain Id=\"any\"><Compatibility><ProtocolVersion>2.1</ProtocolVersion>"
            "</Compatibility><Discovery><ParticipantIndex>auto</ParticipantIndex>"
            "<MaxAutoParticipantIndex>9</MaxAutoParticipantIndex>"
            "<Peers><Peer Address=\"%s\" PruneDelay=\"inf\"/></Peers></Discovery>"
            "</Domain></CycloneDDS>", peer_locator);
        if (config_length < 0 || (size_t)config_length >= sizeof(peer_domain_config)) {
            runtime->last_result = DDS_RETCODE_BAD_PARAMETER;
            return false;
        }
        domain_config = peer_domain_config;
    }

    runtime->domain = dds_create_domain(domain_id, domain_config);
    if (runtime->domain < 0) {
        runtime->last_result = runtime->domain;
        runtime->domain = DDS_ENTITY_NIL;
        return false;
    }

    dds_entity_t participant = dds_create_participant(domain_id, NULL, NULL);
    if (participant < 0) {
        runtime->last_result = participant;
        goto fail;
    }

    runtime->participant = participant;
    if (!ros2_chatter_start(&runtime->chatter, participant)) {
        runtime->last_result = runtime->chatter.last_result;
        goto fail;
    }

    if (!ros2_graph_start(&runtime->graph, participant)) {
        runtime->last_result = runtime->graph.last_result;
        goto fail;
    }

    if (!ros2_graph_publish(&runtime->graph, participant,
                            ros2_chatter_writer_entity(&runtime->chatter),
                            ros2_chatter_reader_entity(&runtime->chatter))) {
        runtime->last_result = runtime->graph.last_result;
        goto fail;
    }

    runtime->last_result = DDS_RETCODE_OK;
    runtime->running = true;
    return true;

fail:
    ros2_graph_stop(&runtime->graph);
    ros2_chatter_stop(&runtime->chatter);
    if (participant >= 0) {
        dds_delete(participant);
    }
    if (runtime->domain >= 0) {
        dds_delete(runtime->domain);
    }
    runtime->participant = DDS_ENTITY_NIL;
    runtime->domain = DDS_ENTITY_NIL;
    runtime->running = false;
    return false;
}

void dds_runtime_stop(dds_runtime *runtime) {
    if (runtime->running) {
        ros2_graph_stop(&runtime->graph);
        ros2_chatter_stop(&runtime->chatter);
    }

    int32_t participant_result = DDS_RETCODE_OK;
    int32_t domain_result = DDS_RETCODE_OK;
    bool participant_deleted = false;
    bool domain_deleted = false;
    if (runtime->participant >= 0) {
        participant_result = dds_delete(runtime->participant);
        participant_deleted = true;
        runtime->participant = DDS_ENTITY_NIL;
    }
    if (runtime->domain >= 0) {
        domain_result = dds_delete(runtime->domain);
        domain_deleted = true;
        runtime->domain = DDS_ENTITY_NIL;
    }
    if (participant_deleted || domain_deleted) {
        runtime->last_result = participant_result != DDS_RETCODE_OK ? participant_result : domain_result;
    }
    runtime->running = false;

    dds_set_log_sink(NULL, NULL);
    dds_set_trace_sink(NULL, NULL);
    g_log_callback = NULL;
    g_log_context = NULL;
}

bool dds_runtime_publish_chatter(dds_runtime *runtime, const char *data) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }

    bool published = ros2_chatter_publish(&runtime->chatter, data);
    runtime->last_result = runtime->chatter.last_result;
    return published;
}

bool dds_runtime_refresh_graph(dds_runtime *runtime) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }

    bool published = ros2_graph_publish(&runtime->graph, runtime->participant,
                                        ros2_chatter_writer_entity(&runtime->chatter),
                                        ros2_chatter_reader_entity(&runtime->chatter));
    runtime->last_result = runtime->graph.last_result;
    return published;
}

int32_t dds_runtime_poll_chatter(dds_runtime *runtime, ros2_chatter_receive_fn callback, void *context) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return runtime->last_result;
    }

    int32_t result = ros2_chatter_take(&runtime->chatter, callback, context);
    runtime->last_result = runtime->chatter.last_result;
    return result;
}

uint64_t dds_runtime_chatter_transmitted(const dds_runtime *runtime) {
    return runtime->chatter.transmitted;
}

uint64_t dds_runtime_chatter_received(const dds_runtime *runtime) {
    return runtime->chatter.received;
}

uint64_t dds_runtime_graph_published(const dds_runtime *runtime) {
    return runtime->graph.published;
}

void dds_runtime_socket_stats(ddsrt_3ds_socket_stats_t *stats) {
    ddsrt_3ds_socket_stats(stats);
}

int32_t dds_runtime_chatter_writer_matches(dds_runtime *runtime) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return -1;
    }

    int32_t matches = ros2_chatter_writer_matches(&runtime->chatter);
    if (matches < 0) {
        runtime->last_result = runtime->chatter.last_result;
    }
    return matches;
}

int32_t dds_runtime_chatter_reader_matches(dds_runtime *runtime) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return -1;
    }

    int32_t matches = ros2_chatter_reader_matches(&runtime->chatter);
    if (matches < 0) {
        runtime->last_result = runtime->chatter.last_result;
    }
    return matches;
}

int32_t dds_runtime_chatter_writer_incompatible_qos(dds_runtime *runtime, uint32_t *last_policy_id) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return -1;
    }

    int32_t count = ros2_chatter_writer_incompatible_qos(&runtime->chatter, last_policy_id);
    if (count < 0) {
        runtime->last_result = runtime->chatter.last_result;
    }
    return count;
}

int32_t dds_runtime_chatter_reader_incompatible_qos(dds_runtime *runtime, uint32_t *last_policy_id) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return -1;
    }

    int32_t count = ros2_chatter_reader_incompatible_qos(&runtime->chatter, last_policy_id);
    if (count < 0) {
        runtime->last_result = runtime->chatter.last_result;
    }
    return count;
}

int32_t dds_runtime_graph_writer_matches(dds_runtime *runtime) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return -1;
    }

    int32_t matches = ros2_graph_writer_matches(&runtime->graph);
    if (matches < 0) {
        runtime->last_result = runtime->graph.last_result;
    }
    return matches;
}

const char *dds_runtime_status(const dds_runtime *runtime) {
    return runtime->running ? "RUNNING" : "STOPPED";
}

const char *dds_runtime_error_text(const dds_runtime *runtime) {
    return dds_strretcode(runtime->last_result);
}
