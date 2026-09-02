#include "ros2_imu.h"

#include "ros2_common.h"
#include "ros2_types.h"

#include <math.h>
#include <string.h>

#define NINTENDO_EPOCH_OFFSET_SECONDS UINT64_C(2208988800)
#define DEGREES_TO_RADIANS 0.017453292519943295

void ros2_imu_init(ros2_imu *imu) {
    memset(imu, 0, sizeof(*imu));
    imu->topic = DDS_ENTITY_NIL;
    imu->writer = DDS_ENTITY_NIL;
    imu->last_result = DDS_RETCODE_OK;
}

bool ros2_imu_start(ros2_imu *imu, dds_entity_t participant, double acceleration_scale) {
    imu->acceleration_scale = acceleration_scale;
    imu->accelerometer_result = HIDUSER_EnableAccelerometer();
    if (R_FAILED(imu->accelerometer_result)) {
        imu->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    imu->gyroscope_result = HIDUSER_EnableGyroscope();
    if (R_FAILED(imu->gyroscope_result)) {
        HIDUSER_DisableAccelerometer();
        imu->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    imu->coefficient_result = HIDUSER_GetGyroscopeRawToDpsCoefficient(&imu->gyroscope_raw_to_dps);
    if (R_FAILED(imu->coefficient_result)) {
        HIDUSER_DisableGyroscope();
        HIDUSER_DisableAccelerometer();
        imu->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    imu->sensors_enabled = true;

    ros2_topic_interface topic = {
        .name = ROS2_IMU_TOPIC,
        .type = &sensor_msgs_msg_dds__Imu__desc,
        .topic = imu->topic,
        .writer = imu->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };

    if (!ros2_topic_create_endpoints(&topic, participant, 5, false, DDS_MSECS(100), false)) {
        imu->last_result = topic.last_result;
        ros2_topic_cleanup(&topic);
        imu->topic = topic.topic;
        imu->writer = topic.writer;
        ros2_imu_stop(imu);
        return false;
    }

    imu->topic = topic.topic;
    imu->writer = topic.writer;
    imu->last_result = topic.last_result;
    return true;
}

bool ros2_imu_publish(ros2_imu *imu, uint64_t timestamp_ms) {
    if (!imu->sensors_enabled || imu->writer <= DDS_ENTITY_NIL) {
        imu->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return false;
    }

    accelVector acceleration;
    angularRate angular_rate;
    hidAccelRead(&acceleration);
    hidGyroRead(&angular_rate);

    const double gyro_scale = (double)imu->gyroscope_raw_to_dps * DEGREES_TO_RADIANS;
    imu->last_angular_velocity[0] = (double)angular_rate.x * gyro_scale;
    imu->last_angular_velocity[1] = (double)angular_rate.y * gyro_scale;
    imu->last_angular_velocity[2] = (double)angular_rate.z * gyro_scale;
    imu->last_linear_acceleration[0] = (double)acceleration.x * imu->acceleration_scale;
    imu->last_linear_acceleration[1] = (double)acceleration.y * imu->acceleration_scale;
    imu->last_linear_acceleration[2] = (double)acceleration.z * imu->acceleration_scale;

    uint64_t unix_seconds = timestamp_ms / 1000;
    if (unix_seconds >= NINTENDO_EPOCH_OFFSET_SECONDS) {
        unix_seconds -= NINTENDO_EPOCH_OFFSET_SECONDS;
    } else {
        unix_seconds = 0;
    }
    sensor_msgs_msg_dds__Imu_ sample = { 0 };
    sample.header.stamp.sec = (int32_t)unix_seconds;
    sample.header.stamp.nanosec = (uint32_t)((timestamp_ms % 1000) * UINT64_C(1000000));
    sample.header.frame_id = (char *)ROS2_IMU_FRAME_ID;
    sample.orientation.w = 1.0;
    sample.orientation_covariance[0] = -1.0;
    sample.angular_velocity.x = imu->last_angular_velocity[0];
    sample.angular_velocity.y = imu->last_angular_velocity[1];
    sample.angular_velocity.z = imu->last_angular_velocity[2];
    sample.linear_acceleration.x = imu->last_linear_acceleration[0];
    sample.linear_acceleration.y = imu->last_linear_acceleration[1];
    sample.linear_acceleration.z = imu->last_linear_acceleration[2];

    imu->last_result = dds_write(imu->writer, &sample);
    if (imu->last_result != DDS_RETCODE_OK) return false;
    imu->transmitted++;
    return true;
}

int32_t ros2_imu_writer_matches(ros2_imu *imu) {
    dds_publication_matched_status_t status = { 0 };
    dds_return_t result = dds_get_publication_matched_status(imu->writer, &status);
    if (result < 0) {
        imu->last_result = result;
        return result;
    }
    return status.current_count;
}

dds_entity_t ros2_imu_writer_entity(const ros2_imu *imu) {
    return imu != NULL ? imu->writer : DDS_ENTITY_NIL;
}

void ros2_imu_stop(ros2_imu *imu) {
    ros2_topic_interface topic = {
        .name = ROS2_IMU_TOPIC,
        .type = &sensor_msgs_msg_dds__Imu__desc,
        .topic = imu->topic,
        .writer = imu->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = imu->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };

    ros2_topic_cleanup(&topic);
    imu->topic = topic.topic;
    imu->writer = topic.writer;
    imu->last_result = topic.last_result;
    if (imu->sensors_enabled) {
        HIDUSER_DisableGyroscope();
        HIDUSER_DisableAccelerometer();
        imu->sensors_enabled = false;
    }
}
