#ifndef ROS2_3DS_ROS2_TYPES_H
#define ROS2_3DS_ROS2_TYPES_H

#include "std_msgs_string.h"
#include "sensor_msgs_imu.h"
#include "sensor_msgs_compressed_image.h"
#include "sensor_msgs_camera_info.h"
#include "add_two_ints.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef std_msgs_msg_dds__String_ ros2_std_string_msg_t;
typedef sensor_msgs_msg_dds__Imu_ ros2_imu_msg_t;
typedef sensor_msgs_msg_dds__CompressedImage_ ros2_compressed_image_msg_t;
typedef sensor_msgs_msg_dds__CameraInfo_ ros2_camera_info_msg_t;
typedef example_interfaces_srv_dds__AddTwoInts_Request_ ros2_add_two_ints_request_t;
typedef example_interfaces_srv_dds__AddTwoInts_Response_ ros2_add_two_ints_response_t;

#define ROS2_TOPIC_CHATTER "rt/chatter"
#define ROS2_TOPIC_IMU_DATA "rt/imu/data_raw"
#define ROS2_TOPIC_CAMERA_COMPRESSED "rt/camera/image_raw/compressed"
#define ROS2_TOPIC_CAMERA_INFO "rt/camera/camera_info"
#define ROS2_SERVICE_ADD_TWO_INTS "/add_two_ints"
#define ROS2_SERVICE_ADD_TWO_INTS_REQUEST "rq/add_two_intsRequest"
#define ROS2_SERVICE_ADD_TWO_INTS_RESPONSE "rr/add_two_intsReply"

#ifdef __cplusplus
}
#endif

#endif
