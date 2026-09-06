#include "ros2_joy.h"

#include "ros2_common.h"
#include "ros2_names.h"
#include "ros2_types.h"

#include <string.h>

#define NINTENDO_EPOCH_OFFSET_SECONDS UINT64_C(2208988800)
#define CPAD_MAX 156.0f
#define CPAD_DEADZONE 12

static float normalize_stick(int16_t val, float max_val, int16_t deadzone) {
    if (val > -deadzone && val < deadzone) {
        return 0.0f;
    }
    float norm = (float)val / max_val;
    if (norm > 1.0f) norm = 1.0f;
    if (norm < -1.0f) norm = -1.0f;
    return norm;
}

void ros2_joy_init(ros2_joy *joy) {
    if (joy == NULL) return;
    memset(joy, 0, sizeof(*joy));
    joy->topic = DDS_ENTITY_NIL;
    joy->writer = DDS_ENTITY_NIL;
    joy->last_result = DDS_RETCODE_OK;
    joy->frame_id = ROS2_JOY_FRAME_ID;
}

bool ros2_joy_start(ros2_joy *joy, dds_entity_t participant, const char *ros_namespace) {
    if (joy == NULL || participant <= DDS_ENTITY_NIL) return false;

    char topic_name[256];
    if (!ros2_dds_name(topic_name, sizeof(topic_name), "rt", ros_namespace, "joy")) {
        joy->last_result = DDS_RETCODE_BAD_PARAMETER;
        ros2_joy_stop(joy);
        return false;
    }

    ros2_topic_interface topic = {
        .name = topic_name,
        .type = &sensor_msgs_msg_dds__Joy__desc,
        .topic = joy->topic,
        .writer = joy->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };

    if (!ros2_topic_create_endpoints(&topic, participant, 1, false, DDS_MSECS(100), false)) {
        joy->last_result = topic.last_result;
        ros2_topic_cleanup(&topic);
        joy->topic = topic.topic;
        joy->writer = topic.writer;
        ros2_joy_stop(joy);
        return false;
    }

    joy->topic = topic.topic;
    joy->writer = topic.writer;
    joy->last_result = topic.last_result;
    joy->enabled = true;
    return true;
}

bool ros2_joy_publish(ros2_joy *joy, uint64_t timestamp_ms,
                      const circlePosition *circle,
                      const circlePosition *cstick,
                      u32 keys_held,
                      const touchPosition *touch,
                      bool is_touching) {
    if (joy == NULL || joy->writer <= DDS_ENTITY_NIL) {
        if (joy != NULL) joy->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }

    float axes[ROS2_JOY_NUM_AXES] = { 0 };
    int32_t buttons[ROS2_JOY_NUM_BUTTONS] = { 0 };

    /* Circle Pad: dx is right (+), dy is up (+).
       ROS Joy convention: Left is (+1.0), Right is (-1.0); Up is (+1.0), Down is (-1.0). */
    if (circle != NULL) {
        axes[0] = -normalize_stick(circle->dx, CPAD_MAX, CPAD_DEADZONE);
        axes[1] = normalize_stick(circle->dy, CPAD_MAX, CPAD_DEADZONE);
    }

    /* C-Stick (New 3DS / CPP): same convention */
    if (cstick != NULL) {
        axes[2] = -normalize_stick(cstick->dx, CPAD_MAX, CPAD_DEADZONE);
        axes[3] = normalize_stick(cstick->dy, CPAD_MAX, CPAD_DEADZONE);
    }

    /* D-Pad axes */
    float dpad_x = 0.0f;
    if (keys_held & KEY_DLEFT) dpad_x += 1.0f;
    if (keys_held & KEY_DRIGHT) dpad_x -= 1.0f;
    axes[4] = dpad_x;

    float dpad_y = 0.0f;
    if (keys_held & KEY_DUP) dpad_y += 1.0f;
    if (keys_held & KEY_DDOWN) dpad_y -= 1.0f;
    axes[5] = dpad_y;

    /* Touch Screen axes (320x240) */
    if (is_touching && touch != NULL) {
        axes[6] = ((float)touch->px / 159.5f) - 1.0f;
        axes[7] = 1.0f - ((float)touch->py / 119.5f);
        if (axes[6] < -1.0f) axes[6] = -1.0f;
        if (axes[6] > 1.0f) axes[6] = 1.0f;
        if (axes[7] < -1.0f) axes[7] = -1.0f;
        if (axes[7] > 1.0f) axes[7] = 1.0f;
    }

    /* Buttons */
    buttons[0]  = (keys_held & KEY_A) ? 1 : 0;
    buttons[1]  = (keys_held & KEY_B) ? 1 : 0;
    buttons[2]  = (keys_held & KEY_X) ? 1 : 0;
    buttons[3]  = (keys_held & KEY_Y) ? 1 : 0;
    buttons[4]  = (keys_held & KEY_L) ? 1 : 0;
    buttons[5]  = (keys_held & KEY_R) ? 1 : 0;
    buttons[6]  = (keys_held & KEY_ZL) ? 1 : 0;
    buttons[7]  = (keys_held & KEY_ZR) ? 1 : 0;
    buttons[8]  = (keys_held & KEY_SELECT) ? 1 : 0;
    buttons[9]  = (keys_held & KEY_START) ? 1 : 0;
    buttons[10] = (is_touching || (keys_held & KEY_TOUCH)) ? 1 : 0;
    buttons[11] = (keys_held & KEY_DUP) ? 1 : 0;
    buttons[12] = (keys_held & KEY_DDOWN) ? 1 : 0;
    buttons[13] = (keys_held & KEY_DLEFT) ? 1 : 0;
    buttons[14] = (keys_held & KEY_DRIGHT) ? 1 : 0;

    memcpy(joy->last_axes, axes, sizeof(axes));
    memcpy(joy->last_buttons, buttons, sizeof(buttons));

    uint64_t unix_seconds = timestamp_ms / 1000;
    if (unix_seconds >= NINTENDO_EPOCH_OFFSET_SECONDS) {
        unix_seconds -= NINTENDO_EPOCH_OFFSET_SECONDS;
    } else {
        unix_seconds = 0;
    }

    sensor_msgs_msg_dds__Joy_ sample = { 0 };
    sample.header.stamp.sec = (int32_t)unix_seconds;
    sample.header.stamp.nanosec = (uint32_t)((timestamp_ms % 1000) * UINT64_C(1000000));
    sample.header.frame_id = (char *)(joy->frame_id != NULL ? joy->frame_id : ROS2_JOY_FRAME_ID);

    sample.axes._maximum = ROS2_JOY_NUM_AXES;
    sample.axes._length = ROS2_JOY_NUM_AXES;
    sample.axes._buffer = axes;
    sample.axes._release = false;

    sample.buttons._maximum = ROS2_JOY_NUM_BUTTONS;
    sample.buttons._length = ROS2_JOY_NUM_BUTTONS;
    sample.buttons._buffer = buttons;
    sample.buttons._release = false;

    joy->last_result = dds_write(joy->writer, &sample);
    if (joy->last_result != DDS_RETCODE_OK) return false;
    joy->transmitted++;
    return true;
}

int32_t ros2_joy_writer_matches(ros2_joy *joy) {
    if (joy == NULL || joy->writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    dds_return_t result = dds_get_publication_matched_status(joy->writer, &status);
    if (result < 0) {
        joy->last_result = result;
        return result;
    }
    return status.current_count;
}

dds_entity_t ros2_joy_writer_entity(const ros2_joy *joy) {
    return joy != NULL ? joy->writer : DDS_ENTITY_NIL;
}

void ros2_joy_stop(ros2_joy *joy) {
    if (joy == NULL) return;
    ros2_topic_interface topic = {
        .name = ROS2_JOY_TOPIC,
        .type = &sensor_msgs_msg_dds__Joy__desc,
        .topic = joy->topic,
        .writer = joy->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = joy->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };

    ros2_topic_cleanup(&topic);
    joy->topic = topic.topic;
    joy->writer = topic.writer;
    joy->last_result = topic.last_result;
    joy->enabled = false;
}

