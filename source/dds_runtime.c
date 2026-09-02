#include "dds_runtime.h"

#include <dds/dds.h>
#include "logging/app_log.h"
#include "ros2_types.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static dds_runtime_log_fn g_log_callback;
static void *g_log_context;

#define DDS_3DS_GENERAL_CONFIG \
    "<General><MaxMessageSize>1456 B</MaxMessageSize>" \
    "<MaxRexmitMessageSize>1456 B</MaxRexmitMessageSize>" \
    "<FragmentSize>1344 B</FragmentSize></General>"

static void dds_log_sink(void *context, const dds_log_data_t *data) {
    (void)context;
    if (g_log_callback == NULL || data == NULL || data->message == NULL) {
        return;
    }

    char message[384];
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
    snprintf(runtime->ros_namespace, sizeof(runtime->ros_namespace), "/");
    ros2_chatter_init(&runtime->chatter);
    ros2_graph_init(&runtime->graph);
    ros2_imu_init(&runtime->imu);
    ros2_add_two_ints_init(&runtime->add_two_ints);
    ros2_camera_init(&runtime->camera);

    runtime->common_topic.name = "rt/chatter";
    runtime->common_topic.type = &std_msgs_msg_dds__String__desc;
    runtime->common_topic.topic = DDS_ENTITY_NIL;
    runtime->common_topic.writer = DDS_ENTITY_NIL;
    runtime->common_topic.reader = DDS_ENTITY_NIL;
    runtime->common_topic.last_result = DDS_RETCODE_OK;
    runtime->common_topic.writer_enabled = true;
    runtime->common_topic.reader_enabled = true;

    runtime->common_service.name = "/add_two_ints";
    runtime->common_service.request_name = ROS2_ADD_TWO_INTS_REQUEST_TOPIC;
    runtime->common_service.response_name = ROS2_ADD_TWO_INTS_RESPONSE_TOPIC;
    runtime->common_service.request_type = &example_interfaces_srv_dds__AddTwoInts_Request__desc;
    runtime->common_service.response_type = &example_interfaces_srv_dds__AddTwoInts_Response__desc;
    runtime->common_service.request_topic = DDS_ENTITY_NIL;
    runtime->common_service.response_topic = DDS_ENTITY_NIL;
    runtime->common_service.request_reader = DDS_ENTITY_NIL;
    runtime->common_service.response_writer = DDS_ENTITY_NIL;
    runtime->common_service.last_result = DDS_RETCODE_OK;
    runtime->common_service.service_id = NULL;
    runtime->common_service.running = false;
}

void dds_runtime_set_log_sink(dds_runtime_log_fn callback, void *context) {
    g_log_callback = callback;
    g_log_context = context;
    dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_INFO);
    dds_set_log_sink(dds_log_sink, NULL);
    dds_set_trace_sink(NULL, NULL);
}

bool dds_runtime_start(dds_runtime *runtime, uint32_t domain_id, const char *peer_ip,
                       const char *broadcast_ip, bool imu_enabled,
                       double imu_acceleration_scale, bool camera_enabled,
                       const ros2_camera_config *camera_config,
                       const char *ros_namespace) {
    if (runtime->running) {
        return true;
    }

    const char *domain_config = "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                                "</Domain></CycloneDDS>";
    char peer_domain_config[640];
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
        int config_length;
        if (explicit_peer) {
            config_length = snprintf(peer_domain_config, sizeof(peer_domain_config),
                "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                "<Discovery><ParticipantIndex>auto</ParticipantIndex>"
                "<MaxAutoParticipantIndex>9</MaxAutoParticipantIndex>"
                "<Peers><Peer Address=\"%s\" PruneDelay=\"inf\"/></Peers></Discovery>"
                "</Domain></CycloneDDS>", peer_locator);
        } else {
            config_length = snprintf(peer_domain_config, sizeof(peer_domain_config),
                "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                "<Discovery><ParticipantIndex>auto</ParticipantIndex>"
                "<MaxAutoParticipantIndex>9</MaxAutoParticipantIndex>"
                "<Peers><Peer Address=\"%s\" PruneDelay=\"inf\"/>"
                "<Peer Address=\"%s\" PruneDelay=\"inf\"/></Peers></Discovery>"
                "</Domain></CycloneDDS>", peer_locator, address);
        }
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
    snprintf(runtime->ros_namespace, sizeof(runtime->ros_namespace), "%s",
             ros_namespace != NULL && ros_namespace[0] != '\0' ? ros_namespace : "/");
    ros2_graph_set_namespace(&runtime->graph, ros_namespace);
    if (!ros2_chatter_start(&runtime->chatter, participant, ros_namespace)) {
        runtime->last_result = runtime->chatter.last_result;
        goto fail;
    }

    if (imu_enabled) {
        (void)ros2_imu_start(&runtime->imu, participant, imu_acceleration_scale, ros_namespace);
    }
    if (camera_enabled) {
        (void)ros2_camera_start(&runtime->camera, participant, camera_config, ros_namespace);
    }
    (void)ros2_add_two_ints_start(&runtime->add_two_ints, participant, ros_namespace);

    if (!ros2_graph_start(&runtime->graph, participant)) {
        runtime->last_result = runtime->graph.last_result;
        goto fail;
    }

    if (!ros2_graph_publish(&runtime->graph, participant,
                            ros2_chatter_writer_entity(&runtime->chatter),
                            ros2_chatter_reader_entity(&runtime->chatter),
                            ros2_imu_writer_entity(&runtime->imu),
                            ros2_camera_writer_entity(&runtime->camera),
                            runtime->add_two_ints.request_reader,
                            runtime->add_two_ints.response_writer)) {
        runtime->last_result = runtime->graph.last_result;
        goto fail;
    }

    runtime->last_result = DDS_RETCODE_OK;
    runtime->running = true;
    return true;

fail:
    ros2_graph_stop(&runtime->graph);
    ros2_add_two_ints_stop(&runtime->add_two_ints);
    ros2_camera_stop(&runtime->camera);
    ros2_imu_stop(&runtime->imu);
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
        ros2_add_two_ints_stop(&runtime->add_two_ints);
        ros2_camera_stop(&runtime->camera);
        ros2_imu_stop(&runtime->imu);
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
                                        ros2_chatter_reader_entity(&runtime->chatter),
                                        ros2_imu_writer_entity(&runtime->imu),
                                        ros2_camera_writer_entity(&runtime->camera),
                                        runtime->add_two_ints.request_reader,
                                        runtime->add_two_ints.response_writer);
    runtime->last_result = runtime->graph.last_result;
    return published;
}

bool dds_runtime_publish_imu(dds_runtime *runtime, uint64_t timestamp_ms) {
    if (!runtime->running) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }
    bool published = ros2_imu_publish(&runtime->imu, timestamp_ms);
    runtime->last_result = runtime->imu.last_result;
    return published;
}

bool dds_runtime_poll_camera(dds_runtime *runtime, uint64_t timestamp_ms, bool publish) {
    if (!runtime->running || runtime->camera.writer <= DDS_ENTITY_NIL) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }
    bool processed = ros2_camera_poll(&runtime->camera, timestamp_ms, publish);
    runtime->last_result = runtime->camera.last_result;
    return processed;
}

bool dds_runtime_set_camera_enabled(dds_runtime *runtime, bool enabled,
                                    const ros2_camera_config *camera_config) {
    app_log_write(APP_LOG_INFO, "dds_runtime_set_camera_enabled: enabled=%d", enabled);
    if (!runtime->running || runtime->participant <= DDS_ENTITY_NIL) {
        runtime->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }
    if (enabled) {
        app_log_write(APP_LOG_INFO, "CAM starting: enabling camera");
        if (runtime->camera.writer > DDS_ENTITY_NIL) return true;
        app_log_write(APP_LOG_INFO, "CAM calling ros2_camera_start");
        if (!ros2_camera_start(&runtime->camera, runtime->participant, camera_config,
                       runtime->ros_namespace)) {
            runtime->last_result = runtime->camera.last_result;
            return false;
        }
    } else {
        if (runtime->camera.writer <= DDS_ENTITY_NIL) return true;
        ros2_camera_stop(&runtime->camera);
        if (runtime->camera.last_result != DDS_RETCODE_OK) {
            runtime->last_result = runtime->camera.last_result;
            return false;
        }
    }
    return dds_runtime_refresh_graph(runtime);
}

int32_t dds_runtime_process_services(dds_runtime *runtime) {
    if (!runtime->running || !runtime->add_two_ints.running) return 0;
    int32_t processed = ros2_add_two_ints_process(&runtime->add_two_ints);
    if (processed < 0) runtime->last_result = runtime->add_two_ints.last_result;
    return processed;
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

uint64_t dds_runtime_imu_transmitted(const dds_runtime *runtime) {
    return runtime->imu.transmitted;
}

int32_t dds_runtime_imu_writer_matches(dds_runtime *runtime) {
    if (!runtime->running || runtime->imu.writer <= DDS_ENTITY_NIL) return 0;
    int32_t matches = ros2_imu_writer_matches(&runtime->imu);
    if (matches < 0) runtime->last_result = runtime->imu.last_result;
    return matches;
}

int32_t dds_runtime_camera_writer_matches(dds_runtime *runtime) {
    if (!runtime->running || runtime->camera.writer <= DDS_ENTITY_NIL) return 0;
    int32_t matches = ros2_camera_writer_matches(&runtime->camera);
    if (matches < 0) runtime->last_result = runtime->camera.last_result;
    return matches;
}

int32_t dds_runtime_add_two_ints_request_matches(dds_runtime *runtime) {
    if (!runtime->running || !runtime->add_two_ints.running) return 0;
    int32_t matches = ros2_add_two_ints_request_matches(&runtime->add_two_ints);
    if (matches < 0) runtime->last_result = runtime->add_two_ints.last_result;
    return matches;
}

int32_t dds_runtime_add_two_ints_response_matches(dds_runtime *runtime) {
    if (!runtime->running || !runtime->add_two_ints.running) return 0;
    int32_t matches = ros2_add_two_ints_response_matches(&runtime->add_two_ints);
    if (matches < 0) runtime->last_result = runtime->add_two_ints.last_result;
    return matches;
}

int32_t dds_runtime_add_two_ints_request_incompatible_qos(dds_runtime *runtime,
                                                          uint32_t *last_policy_id) {
    if (!runtime->running || !runtime->add_two_ints.running) return 0;
    int32_t count = ros2_add_two_ints_request_incompatible_qos(
        &runtime->add_two_ints, last_policy_id);
    if (count < 0) runtime->last_result = runtime->add_two_ints.last_result;
    return count;
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
