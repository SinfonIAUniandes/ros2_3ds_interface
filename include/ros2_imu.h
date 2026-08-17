#ifndef ROS2_3DS_ROS2_IMU_H
#define ROS2_3DS_ROS2_IMU_H

#include <3ds.h>

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

#define ROS2_IMU_TOPIC "rt/imu/data_raw"
#define ROS2_IMU_FRAME_ID "3ds_imu_link"

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_return_t last_result;
    Result accelerometer_result;
    Result gyroscope_result;
    Result coefficient_result;
    float gyroscope_raw_to_dps;
    double acceleration_scale;
    uint64_t transmitted;
    double last_angular_velocity[3];
    double last_linear_acceleration[3];
    bool sensors_enabled;
} ros2_imu;

void ros2_imu_init(ros2_imu *imu);
bool ros2_imu_start(ros2_imu *imu, dds_entity_t participant, double acceleration_scale);
bool ros2_imu_publish(ros2_imu *imu, uint64_t timestamp_ms);
int32_t ros2_imu_writer_matches(ros2_imu *imu);
dds_entity_t ros2_imu_writer_entity(const ros2_imu *imu);
void ros2_imu_stop(ros2_imu *imu);

#endif