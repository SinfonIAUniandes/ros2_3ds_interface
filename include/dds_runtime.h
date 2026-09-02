#ifndef ROS2_3DS_DDS_RUNTIME_H
#define ROS2_3DS_DDS_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>
#include <dds/ddsrt/sockets/3ds.h>

#include "ros2_common.h"
#include "ros2_chatter.h"
#include "ros2_graph.h"
#include "ros2_imu.h"
#include "ros2_add_two_ints.h"
#include "ros2_camera.h"

typedef void (*dds_runtime_log_fn)(void *context, int level, const char *message);

typedef struct {
    dds_entity_t domain;
    int32_t participant;
    int32_t last_result;
    bool running;
    ros2_topic_interface common_topic;
    ros2_service_interface common_service;
    ros2_chatter chatter;
    ros2_graph graph;
    ros2_imu imu;
    ros2_add_two_ints add_two_ints;
    ros2_camera camera;
} dds_runtime;

void dds_runtime_init(dds_runtime *runtime);
void dds_runtime_set_log_sink(dds_runtime_log_fn callback, void *context);
bool dds_runtime_start(dds_runtime *runtime, uint32_t domain_id, const char *peer_ip,
                       const char *broadcast_ip, bool imu_enabled,
                       double imu_acceleration_scale, bool camera_enabled,
                       const ros2_camera_config *camera_config);
void dds_runtime_stop(dds_runtime *runtime);
bool dds_runtime_publish_chatter(dds_runtime *runtime, const char *data);
bool dds_runtime_refresh_graph(dds_runtime *runtime);
bool dds_runtime_publish_imu(dds_runtime *runtime, uint64_t timestamp_ms);
bool dds_runtime_poll_camera(dds_runtime *runtime, uint64_t timestamp_ms, bool publish);
bool dds_runtime_set_camera_enabled(dds_runtime *runtime, bool enabled,
                                    const ros2_camera_config *camera_config);
int32_t dds_runtime_process_services(dds_runtime *runtime);
int32_t dds_runtime_poll_chatter(dds_runtime *runtime, ros2_chatter_receive_fn callback, void *context);
uint64_t dds_runtime_chatter_transmitted(const dds_runtime *runtime);
uint64_t dds_runtime_chatter_received(const dds_runtime *runtime);
uint64_t dds_runtime_graph_published(const dds_runtime *runtime);
uint64_t dds_runtime_imu_transmitted(const dds_runtime *runtime);
int32_t dds_runtime_imu_writer_matches(dds_runtime *runtime);
int32_t dds_runtime_camera_writer_matches(dds_runtime *runtime);
int32_t dds_runtime_add_two_ints_request_matches(dds_runtime *runtime);
int32_t dds_runtime_add_two_ints_response_matches(dds_runtime *runtime);
int32_t dds_runtime_add_two_ints_request_incompatible_qos(dds_runtime *runtime,
                                                          uint32_t *last_policy_id);
void dds_runtime_socket_stats(ddsrt_3ds_socket_stats_t *stats);
int32_t dds_runtime_chatter_writer_matches(dds_runtime *runtime);
int32_t dds_runtime_chatter_reader_matches(dds_runtime *runtime);
int32_t dds_runtime_chatter_writer_incompatible_qos(dds_runtime *runtime, uint32_t *last_policy_id);
int32_t dds_runtime_chatter_reader_incompatible_qos(dds_runtime *runtime, uint32_t *last_policy_id);
int32_t dds_runtime_graph_writer_matches(dds_runtime *runtime);
const char *dds_runtime_status(const dds_runtime *runtime);
const char *dds_runtime_error_text(const dds_runtime *runtime);

#endif
