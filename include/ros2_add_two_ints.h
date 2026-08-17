#ifndef ROS2_3DS_ROS2_ADD_TWO_INTS_H
#define ROS2_3DS_ROS2_ADD_TWO_INTS_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

#define ROS2_ADD_TWO_INTS_SERVICE "/add_two_ints"
#define ROS2_ADD_TWO_INTS_REQUEST_TOPIC "rq/add_two_intsRequest"
#define ROS2_ADD_TWO_INTS_RESPONSE_TOPIC "rr/add_two_intsReply"

typedef struct {
    dds_entity_t request_topic;
    dds_entity_t response_topic;
    dds_entity_t request_reader;
    dds_entity_t response_writer;
    dds_return_t last_result;
    uint64_t requests_handled;
    uint64_t take_calls;
    uint64_t samples_taken;
    uint64_t responses_written;
    uint64_t invalid_samples;
    uint64_t last_request_guid;
    int64_t last_request_seq;
    int64_t last_a;
    int64_t last_b;
    int64_t last_sum;
    char service_id[64];
    dds_return_t last_take_result;
    dds_return_t last_write_result;
    bool running;
} ros2_add_two_ints;

void ros2_add_two_ints_init(ros2_add_two_ints *service);
bool ros2_add_two_ints_start(ros2_add_two_ints *service, dds_entity_t participant);
int32_t ros2_add_two_ints_process(ros2_add_two_ints *service);
int32_t ros2_add_two_ints_request_matches(ros2_add_two_ints *service);
int32_t ros2_add_two_ints_response_matches(ros2_add_two_ints *service);
int32_t ros2_add_two_ints_request_incompatible_qos(ros2_add_two_ints *service,
                                                   uint32_t *last_policy_id);
void ros2_add_two_ints_stop(ros2_add_two_ints *service);

#endif