#ifndef ROS2_3DS_APP_UI_H
#define ROS2_3DS_APP_UI_H

#include <3ds.h>

#include <stdbool.h>
#include <stdint.h>

#include "ros2_camera.h"

typedef enum {
    UI_VIEW_HOME,
    UI_VIEW_TOPICS,
    UI_VIEW_SERVICES,
    UI_VIEW_MENU,
    UI_VIEW_DETAILS,
    UI_VIEW_LOGS,
    UI_VIEW_SETTINGS,
    UI_VIEW_COUNT
} ui_view_id;

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_PUBLISH_ONCE = 1u << 0,
    UI_ACTION_TOGGLE_PUBLISHING = 1u << 1,
    UI_ACTION_TOGGLE_LISTENER = 1u << 2,
    UI_ACTION_SEND_PROBE = 1u << 3,
    UI_ACTION_EXIT = 1u << 4,
    UI_ACTION_NEXT_VIEW = 1u << 5,
    UI_ACTION_PREVIOUS_VIEW = 1u << 6,
    UI_ACTION_NEXT_ITEM = 1u << 7,
    UI_ACTION_PREVIOUS_ITEM = 1u << 8,
    UI_ACTION_ACTIVATE = 1u << 9,
    UI_ACTION_BACK = 1u << 10,
    UI_ACTION_TOGGLE_CHATTER_TOPIC = 1u << 11,
    UI_ACTION_TOGGLE_IMU_TOPIC = 1u << 12,
    UI_ACTION_TOGGLE_CAMERA_TOPIC = 1u << 13,
    UI_ACTION_CAMERA_SETTING_NEXT = 1u << 14,
    UI_ACTION_CAMERA_SETTING_PREVIOUS = 1u << 15
} ui_action;

typedef struct {
    u32 publish_once;
    u32 toggle_publishing;
    u32 toggle_listener;
    u32 send_probe;
    u32 exit;
    u32 next_view;
    u32 previous_view;
    u32 next_item;
    u32 previous_item;
    u32 activate;
    u32 back;
} ui_controls;

typedef struct {
    const char *build_id;
    const char *local_ip;
    const char *netmask;
    const char *broadcast_ip;
    const char *peer_ip;
    const char *last_probe_sender;
    const char *last_probe_payload;
    const char *last_published_message;
    const char *last_received_message;
    const char *dds_status;
    const char *dds_error;
    const char *log_path;
    uint32_t domain_id;
    uint32_t send_interval_ms;
    uint16_t probe_port;
    int32_t soc_result;
    int32_t dds_result;
    bool network_ready;
    bool dds_running;
    bool dds_enabled;
    bool external_config;
    bool static_peer;
    bool publishing;
    bool chatter_topic_enabled;
    bool imu_topic_enabled;
    bool camera_topic_enabled;
    bool listening;
    bool log_has_error;
    bool probe_socket_ready;
    bool probe_membership_joined;
    int32_t probe_socket_error;
    int32_t probe_reuse_error;
    int32_t probe_bind_error;
    int32_t probe_nonblocking_error;
    int32_t probe_membership_error;
    int32_t probe_loopback_error;
    int32_t probe_last_send_error;
    int32_t probe_last_receive_error;
    uint32_t probe_multicast_sent;
    uint32_t probe_unicast_sent;
    uint32_t probe_received;
    uint64_t chatter_transmitted;
    uint64_t chatter_received;
    uint64_t graph_published;
    int32_t graph_matches;
    int32_t writer_matches;
    int32_t reader_matches;
    int32_t writer_qos_rejections;
    int32_t reader_qos_rejections;
    uint32_t writer_qos_policy;
    uint32_t reader_qos_policy;
    bool imu_enabled;
    bool imu_sensors_enabled;
    uint32_t imu_publish_hz;
    uint64_t imu_transmitted;
    int32_t imu_writer_matches;
    double imu_angular_velocity[3];
    double imu_linear_acceleration[3];
    bool camera_available;
    uint32_t camera_source;
    uint32_t camera_resolution;
    uint32_t camera_fps;
    uint32_t camera_quality;
    uint64_t camera_captured;
    uint64_t camera_encoded;
    uint64_t camera_published;
    uint64_t camera_dropped;
    uint32_t camera_jpeg_bytes;
    const uint8_t *camera_preview;
    uint32_t camera_preview_width;
    uint32_t camera_preview_height;
    int32_t camera_writer_matches;
    bool add_two_ints_running;
    uint64_t add_two_ints_requests_handled;
    int32_t add_two_ints_request_matches;
    int32_t add_two_ints_response_matches;
    int64_t add_two_ints_last_a;
    int64_t add_two_ints_last_b;
    int64_t add_two_ints_last_sum;
    uint64_t add_two_ints_take_calls;
    uint64_t add_two_ints_samples_taken;
    uint64_t add_two_ints_responses_written;
    uint64_t add_two_ints_last_guid;
    int64_t add_two_ints_last_seq;
    int32_t add_two_ints_last_take_result;
    int32_t add_two_ints_last_write_result;
    uint32_t rtps_tx_multicast;
    uint32_t rtps_tx_unicast;
    uint32_t rtps_rx_total;
    uint32_t rtps_rx_remote;
    int32_t rtps_last_send_errno;
    int32_t rtps_last_recv_errno;
    int32_t rtps_multicast_if_errno;
} ui_snapshot;

bool app_ui_init(void);
void app_ui_exit(void);
ui_action app_ui_handle_input(u32 keys_down, const touchPosition *touch);
void app_ui_render(const ui_snapshot *snapshot);
ui_view_id app_ui_current_view(void);
const ui_controls *app_ui_controls(void);
const char *app_ui_control_label(u32 mask);
uint8_t app_ui_camera_setting_index(void);
void app_ui_set_camera_settings(const ros2_camera_config *config);
void app_ui_cycle_camera_setting(int direction);
void app_ui_init_camera_settings(void);
bool app_ui_apply_camera_setting(int direction, ros2_camera_config *config);

#endif