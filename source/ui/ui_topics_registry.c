#include "ui_internal.h"

/* Add a topic here, then expose its counters and message previews in ui_snapshot. */
const ui_topic_definition ui_topics[] = {
    { "/chatter", "std_msgs/msg/String", "Reliable | Volatile | Keep last 10" },
    { "/imu/data_raw", "sensor_msgs/msg/Imu", "Best effort | Volatile | Keep last 5" },
    { "/camera/image_raw/compressed", "sensor_msgs/msg/CompressedImage", "Best effort | Volatile | Keep last 1" },
    { "/camera/camera_info", "sensor_msgs/msg/CameraInfo", "Best effort | Volatile | Keep last 1" }
};

const size_t ui_topic_count = sizeof(ui_topics) / sizeof(ui_topics[0]);