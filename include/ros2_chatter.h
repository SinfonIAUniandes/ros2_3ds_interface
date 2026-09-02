#ifndef ROS2_3DS_ROS2_CHATTER_H
#define ROS2_3DS_ROS2_CHATTER_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

typedef void (*ros2_chatter_receive_fn)(void *context, const char *data);

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_entity_t reader;
    dds_return_t last_result;
    uint64_t transmitted;
    uint64_t received;
} ros2_chatter;

void ros2_chatter_init(ros2_chatter *chatter);
bool ros2_chatter_start(ros2_chatter *chatter, dds_entity_t participant,
                        const char *ros_namespace);
bool ros2_chatter_publish(ros2_chatter *chatter, const char *data);
dds_return_t ros2_chatter_take(ros2_chatter *chatter, ros2_chatter_receive_fn callback, void *context);
dds_entity_t ros2_chatter_writer_entity(const ros2_chatter *chatter);
dds_entity_t ros2_chatter_reader_entity(const ros2_chatter *chatter);
int32_t ros2_chatter_writer_matches(ros2_chatter *chatter);
int32_t ros2_chatter_reader_matches(ros2_chatter *chatter);
int32_t ros2_chatter_writer_incompatible_qos(ros2_chatter *chatter, uint32_t *last_policy_id);
int32_t ros2_chatter_reader_incompatible_qos(ros2_chatter *chatter, uint32_t *last_policy_id);
void ros2_chatter_stop(ros2_chatter *chatter);

#endif