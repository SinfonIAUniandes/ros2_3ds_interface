#ifndef ROS2_3DS_ROS2_CAMERA_H
#define ROS2_3DS_ROS2_CAMERA_H

#include <3ds.h>

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>
#include <turbojpeg.h>

#define ROS2_CAMERA_TOPIC "rt/camera/image_raw/compressed"
#define ROS2_CAMERA_INFO_TOPIC "rt/camera/camera_info"
#define ROS2_CAMERA_FORMAT "bgr8; jpeg compressed bgr8"
typedef enum {
    ROS2_CAMERA_SOURCE_INNER,
    ROS2_CAMERA_SOURCE_OUTER_LEFT,
    ROS2_CAMERA_SOURCE_OUTER_RIGHT
} ros2_camera_source;

typedef enum {
    ROS2_CAMERA_RESOLUTION_QQVGA,
    ROS2_CAMERA_RESOLUTION_QVGA
} ros2_camera_resolution;

typedef struct {
    ros2_camera_source source;
    ros2_camera_resolution resolution;
    uint32_t fps;
    int jpeg_quality;
} ros2_camera_config;

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_entity_t info_topic;
    dds_entity_t info_writer;
    dds_return_t last_result;
    Handle receive_event;
    uint16_t *capture_buffer;
    size_t capture_buffer_size;
    uint32_t transfer_bytes;
    uint8_t *rgbx_buffer;
    uint8_t *preview_buffer;
    unsigned char *jpeg_buffer;
    tjhandle jpeg_encoder;
    unsigned long jpeg_capacity;
    unsigned long jpeg_size;
    uint32_t width;
    uint32_t height;
    const char *frame_id;
    ros2_camera_config config;
    uint64_t captured;
    uint64_t encoded;
    uint64_t published;
    uint64_t dropped;
    uint64_t next_info_publish_at;
    Result camera_result;
    int jpeg_result;
    bool camera_initialized;
    bool capture_active;
    bool receiving;
    bool first_receive_logged;
} ros2_camera;

void ros2_camera_init(ros2_camera *camera);
void ros2_camera_config_defaults(ros2_camera_config *config);
bool ros2_camera_start(ros2_camera *camera, dds_entity_t participant,
                       const ros2_camera_config *config);
bool ros2_camera_poll(ros2_camera *camera, uint64_t timestamp_ms, bool publish);
int32_t ros2_camera_writer_matches(ros2_camera *camera);
dds_entity_t ros2_camera_writer_entity(const ros2_camera *camera);
void ros2_camera_stop(ros2_camera *camera);

#endif