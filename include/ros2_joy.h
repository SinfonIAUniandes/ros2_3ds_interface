#ifndef ROS2_3DS_ROS2_JOY_H
#define ROS2_3DS_ROS2_JOY_H

#include <3ds.h>

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

#define ROS2_JOY_TOPIC "rt/joy"
#define ROS2_JOY_FRAME_ID "3ds_joy_link"
#define ROS2_JOY_NUM_AXES 8
#define ROS2_JOY_NUM_BUTTONS 15

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_return_t last_result;
    uint64_t transmitted;
    bool enabled;
    const char *frame_id;
    float last_axes[ROS2_JOY_NUM_AXES];
    int32_t last_buttons[ROS2_JOY_NUM_BUTTONS];
} ros2_joy;

void ros2_joy_init(ros2_joy *joy);
bool ros2_joy_start(ros2_joy *joy, dds_entity_t participant, const char *ros_namespace);
bool ros2_joy_publish(ros2_joy *joy, uint64_t timestamp_ms,
                      const circlePosition *circle,
                      const circlePosition *cstick,
                      u32 keys_held,
                      const touchPosition *touch,
                      bool is_touching);
int32_t ros2_joy_writer_matches(ros2_joy *joy);
dds_entity_t ros2_joy_writer_entity(const ros2_joy *joy);
void ros2_joy_stop(ros2_joy *joy);

#endif

