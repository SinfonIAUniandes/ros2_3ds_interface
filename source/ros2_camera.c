#include "ros2_camera.h"

#include "ros2_common.h"
#include "ros2_types.h"
#include "logging/app_log.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <turbojpeg.h>

#define NINTENDO_EPOCH_OFFSET_SECONDS UINT64_C(2208988800)
#define CAMERA_MAX_DDS_BYTES (8u * 1024u)
#define CAMERA_INFO_INTERVAL_MS UINT64_C(5000)

static uint8_t camera_port(const ros2_camera *camera);

static bool camera_call(ros2_camera *camera, const char *stage, Result result) {
    camera->camera_result = result;
    if (R_SUCCEEDED(result)) return true;
    camera->last_result = DDS_RETCODE_ERROR;
    app_log_write(APP_LOG_ERROR, "CAM %s failed 0x%08lX", stage, (unsigned long)result);
    return false;
}

static void ros2_camera_release_capture(ros2_camera *camera) {
    if (camera->capture_active) {
        CAMU_StopCapture(camera_port(camera));
        camera->capture_active = false;
    }
    if (camera->receive_event != 0) {
        svcCloseHandle(camera->receive_event);
        camera->receive_event = 0;
    }
    if (camera->camera_initialized) {
        CAMU_Activate(SELECT_NONE);
        camExit();
        camera->camera_initialized = false;
    }
    camera->receiving = false;
}

static uint8_t camera_selection(const ros2_camera *camera) {
    return camera->config.source == ROS2_CAMERA_SOURCE_INNER ? SELECT_IN1
         : camera->config.source == ROS2_CAMERA_SOURCE_OUTER_LEFT ? SELECT_OUT1 : SELECT_OUT2;
}

static uint8_t camera_port(const ros2_camera *camera) {
    return camera->config.source == ROS2_CAMERA_SOURCE_OUTER_RIGHT ? PORT_CAM2 : PORT_CAM1;
}

static CAMU_FrameRate camera_frame_rate(const ros2_camera *camera) {
    return camera->config.fps >= 15 ? FRAME_RATE_15
         : camera->config.fps >= 10 ? FRAME_RATE_10 : FRAME_RATE_5;
}

static void camera_dimensions(ros2_camera *camera) {
    if (camera->config.resolution == ROS2_CAMERA_RESOLUTION_QVGA) {
        camera->width = 320;
        camera->height = 240;
    } else {
        camera->width = 160;
        camera->height = 120;
    }
    camera->frame_id = camera->config.source == ROS2_CAMERA_SOURCE_INNER
        ? "3ds_inner_camera_optical_frame"
        : camera->config.source == ROS2_CAMERA_SOURCE_OUTER_LEFT
        ? "3ds_outer_left_camera_optical_frame" : "3ds_outer_right_camera_optical_frame";
}

void ros2_camera_init(ros2_camera *camera) {
    memset(camera, 0, sizeof(*camera));
    camera->topic = DDS_ENTITY_NIL;
    camera->writer = DDS_ENTITY_NIL;
    camera->last_result = DDS_RETCODE_OK;
    ros2_camera_config_defaults(&camera->config);
}

void ros2_camera_config_defaults(ros2_camera_config *config) {
    config->source = ROS2_CAMERA_SOURCE_INNER;
    config->resolution = ROS2_CAMERA_RESOLUTION_QQVGA;
    config->fps = 5;
    config->jpeg_quality = 70;
}

bool ros2_camera_start(ros2_camera *camera, dds_entity_t participant,
                       const ros2_camera_config *config) {
    if (camera->writer > DDS_ENTITY_NIL || camera->info_writer > DDS_ENTITY_NIL ||
        camera->topic > DDS_ENTITY_NIL || camera->info_topic > DDS_ENTITY_NIL) {
        ros2_camera_stop(camera);
    }
    if (config != NULL) camera->config = *config;
    if (camera->config.fps != 5 && camera->config.fps != 10 && camera->config.fps != 15) {
        camera->last_result = DDS_RETCODE_BAD_PARAMETER;
        return false;
    }
    if (camera->config.jpeg_quality < 40 || camera->config.jpeg_quality > 85) {
        camera->last_result = DDS_RETCODE_BAD_PARAMETER;
        return false;
    }
    camera_dimensions(camera);
    app_log_write(APP_LOG_INFO, "CAM creating topic...");

    ros2_topic_interface compressed = {
        .name = ROS2_CAMERA_TOPIC,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->topic,
        .writer = camera->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface info = {
        .name = ROS2_CAMERA_INFO_TOPIC,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->info_topic,
        .writer = camera->info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };

    if (!ros2_topic_create_endpoints(&compressed, participant, 1, false, DDS_MSECS(100), true) ||
        !ros2_topic_create_endpoints(&info, participant, 1, false, DDS_MSECS(100), true)) {
        camera->last_result = compressed.last_result != DDS_RETCODE_OK ? compressed.last_result : info.last_result;
        app_log_write(APP_LOG_ERROR, "CAM topic creation failed compressed=%ld info=%ld",
                      (long)compressed.last_result, (long)info.last_result);
        ros2_topic_cleanup(&compressed);
        ros2_topic_cleanup(&info);
        camera->topic = compressed.topic;
        camera->writer = compressed.writer;
        camera->info_topic = info.topic;
        camera->info_writer = info.writer;
        ros2_camera_stop(camera);
        return false;
    }

    camera->topic = compressed.topic;
    camera->writer = compressed.writer;
    camera->info_topic = info.topic;
    camera->info_writer = info.writer;
    app_log_write(APP_LOG_INFO, "CAM writer result=%ld info_writer=%ld",
                  (long)camera->writer, (long)camera->info_writer);
    app_log_write(APP_LOG_INFO, "CAM allocating buffers...");
    const size_t pixel_count = camera->width * camera->height;
    camera->capture_buffer_size = pixel_count * sizeof(*camera->capture_buffer);
    camera->capture_buffer = malloc(camera->capture_buffer_size);
    camera->rgbx_buffer = malloc(pixel_count * 4);
    camera->preview_buffer = malloc(pixel_count * 3);
    camera->jpeg_capacity = tjBufSize(camera->width, camera->height, TJSAMP_420);
    camera->jpeg_buffer = tjAlloc((int)camera->jpeg_capacity);
    app_log_write(APP_LOG_INFO, "CAM buffers capture=%p rgbx=%p preview=%p jpeg=%p",
                  camera->capture_buffer, camera->rgbx_buffer, camera->preview_buffer,
                  camera->jpeg_buffer);
    if (camera->capture_buffer == NULL || camera->rgbx_buffer == NULL ||
        camera->preview_buffer == NULL || camera->jpeg_buffer == NULL) {
        camera->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        app_log_write(APP_LOG_ERROR, "CAM buffer alloc failed");
        ros2_camera_stop(camera);
        return false;
    }
    camera->jpeg_encoder = tjInitCompress();
    if (camera->jpeg_encoder == NULL) {
        camera->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        app_log_write(APP_LOG_ERROR, "CAM JPEG encoder init failed: %s", tjGetErrorStr());
        ros2_camera_stop(camera);
        return false;
    }
    app_log_write(APP_LOG_INFO, "CAM init camInit...");
    if (!camera_call(camera, "init", camInit())) {
        app_log_write(APP_LOG_ERROR, "CAM init failed, stopping");
        ros2_camera_stop(camera);
        return false;
    }
    camera->camera_initialized = true;
    const uint8_t selection = camera_selection(camera);
    if (!camera_call(camera, "size", CAMU_SetSize(selection,
                     camera->config.resolution == ROS2_CAMERA_RESOLUTION_QVGA ? SIZE_QVGA : SIZE_QQVGA,
                     CONTEXT_A)) ||
        !camera_call(camera, "format", CAMU_SetOutputFormat(selection, OUTPUT_RGB_565, CONTEXT_A)) ||
        !camera_call(camera, "rate", CAMU_SetFrameRate(selection, camera_frame_rate(camera))) ||
        !camera_call(camera, "exposure", CAMU_SetAutoExposure(selection, true)) ||
        !camera_call(camera, "white-balance", CAMU_SetAutoWhiteBalance(selection, true)) ||
        !camera_call(camera, "activate", CAMU_Activate(selection))) {
        ros2_camera_stop(camera);
        return false;
    }
    if (!camera_call(camera, "max-bytes", CAMU_GetMaxBytes(&camera->transfer_bytes,
                     camera->width, camera->height)) ||
        camera->transfer_bytes == 0 || camera->transfer_bytes > INT16_MAX ||
        camera->capture_buffer_size < camera->transfer_bytes ||
        !camera_call(camera, "transfer", CAMU_SetTransferBytes(camera_port(camera),
                     camera->transfer_bytes, camera->width, camera->height)) ||
        !camera_call(camera, "clear", CAMU_ClearBuffer(camera_port(camera))) ||
        !camera_call(camera, "capture", CAMU_StartCapture(camera_port(camera)))) {
        camera->last_result = DDS_RETCODE_ERROR;
        app_log_write(APP_LOG_ERROR, "CAM setup rejected transfer=%lu buffer=%lu",
                      (unsigned long)camera->transfer_bytes,
                      (unsigned long)camera->capture_buffer_size);
        ros2_camera_stop(camera);
        return false;
    }
    camera->capture_active = true;
    app_log_write(APP_LOG_INFO, "CAM ready source=%u size=%lux%lu rate=%lu transfer=%lu",
                  (unsigned int)camera->config.source, (unsigned long)camera->width,
                  (unsigned long)camera->height, (unsigned long)camera->config.fps,
                  (unsigned long)camera->transfer_bytes);
    camera->last_result = DDS_RETCODE_OK;
    return true;
}

static bool ros2_camera_arm_receive(ros2_camera *camera) {
    if (camera->receiving) return true;
    if (camera->transfer_bytes == 0 || camera->transfer_bytes > INT16_MAX ||
        camera->capture_buffer_size < camera->transfer_bytes) return false;
    if (!camera->capture_active) {
        if (!camera_call(camera, "capture-restart", CAMU_StartCapture(camera_port(camera)))) {
            return false;
        }
        camera->capture_active = true;
    }
    if (!camera->first_receive_logged) {
        app_log_write(APP_LOG_INFO, "CAM first arm image=%lu transfer=%lu port=%u",
                      (unsigned long)camera->capture_buffer_size,
                      (unsigned long)camera->transfer_bytes, (unsigned int)camera_port(camera));
        camera->first_receive_logged = true;
    }
    camera->camera_result = CAMU_SetReceiving(&camera->receive_event, camera->capture_buffer,
                                               camera_port(camera), camera->capture_buffer_size,
                                               (s16)camera->transfer_bytes);
    if (R_FAILED(camera->camera_result)) {
        app_log_write(APP_LOG_ERROR, "CAM receive failed 0x%08lX", (unsigned long)camera->camera_result);
        return false;
    }
    camera->receiving = true;
    return true;
}

bool ros2_camera_poll(ros2_camera *camera, uint64_t timestamp_ms, bool publish) {
    if (camera->writer <= DDS_ENTITY_NIL || !camera->camera_initialized) return false;
    if (!ros2_camera_arm_receive(camera)) {
        camera->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    bool receive_finished = false;
    camera->camera_result = CAMU_IsFinishedReceiving(&receive_finished, camera_port(camera));
    if (R_FAILED(camera->camera_result)) {
        camera->last_result = DDS_RETCODE_ERROR;
        app_log_write(APP_LOG_ERROR, "CAM receive status failed 0x%08lX",
                      (unsigned long)camera->camera_result);
        return false;
    }
    if (!receive_finished) return false;
    if (!camera_call(camera, "frame-stop", CAMU_StopCapture(camera_port(camera)))) {
        return false;
    }
    camera->capture_active = false;
    if (camera->receive_event != 0) {
        svcCloseHandle(camera->receive_event);
        camera->receive_event = 0;
    }
    camera->receiving = false;
    camera->captured++;

    const size_t pixel_count = camera->width * camera->height;
    for (size_t index = 0; index < pixel_count; index++) {
        uint16_t pixel = camera->capture_buffer[index];
        uint8_t *destination = camera->rgbx_buffer + index * 4;
        const uint8_t red = (uint8_t)(((pixel >> 11) & 0x1f) * 255 / 31);
        const uint8_t green = (uint8_t)(((pixel >> 5) & 0x3f) * 255 / 63);
        const uint8_t blue = (uint8_t)((pixel & 0x1f) * 255 / 31);
        destination[0] = blue;
        destination[1] = green;
        destination[2] = red;
        destination[3] = 0;
        camera->preview_buffer[index * 3 + 0] = blue;
        camera->preview_buffer[index * 3 + 1] = green;
        camera->preview_buffer[index * 3 + 2] = red;
    }
    if (!publish) {
        camera->last_result = DDS_RETCODE_OK;
        return true;
    }
    unsigned char *jpeg = camera->jpeg_buffer;
    camera->jpeg_size = camera->jpeg_capacity;
    camera->jpeg_result = tjCompress2(camera->jpeg_encoder, camera->rgbx_buffer, camera->width,
                                      camera->width * 4, camera->height, TJPF_BGRX,
                                      &jpeg, &camera->jpeg_size, TJSAMP_420,
                                      camera->config.jpeg_quality, TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
    if (camera->jpeg_result != 0) {
        app_log_write(APP_LOG_ERROR, "CAM JPEG encode failed: %s",
                      tjGetErrorStr2(camera->jpeg_encoder));
        camera->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    if (camera->jpeg_size > CAMERA_MAX_DDS_BYTES && camera->config.jpeg_quality > 40) {
        camera->jpeg_size = camera->jpeg_capacity;
        camera->jpeg_result = tjCompress2(camera->jpeg_encoder, camera->rgbx_buffer, camera->width,
                                          camera->width * 4, camera->height, TJPF_BGRX,
                                          &jpeg, &camera->jpeg_size, TJSAMP_420, 40,
                                          TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
    }
    if (camera->jpeg_result != 0 || camera->jpeg_size > CAMERA_MAX_DDS_BYTES) {
        camera->dropped++;
        camera->last_result = DDS_RETCODE_OK;
        app_log_write(APP_LOG_WARN, "CAM frame skipped jpeg=%lu B", camera->jpeg_size);
        return false;
    }
    camera->encoded++;

    uint64_t unix_seconds = timestamp_ms / 1000;
    if (unix_seconds >= NINTENDO_EPOCH_OFFSET_SECONDS) unix_seconds -= NINTENDO_EPOCH_OFFSET_SECONDS;
    else unix_seconds = 0;
    sensor_msgs_msg_dds__CompressedImage_ sample = { 0 };
    sample.header.stamp.sec = (int32_t)unix_seconds;
    sample.header.stamp.nanosec = (uint32_t)((timestamp_ms % 1000) * UINT64_C(1000000));
    sample.header.frame_id = (char *)camera->frame_id;
    sample.format = (char *)ROS2_CAMERA_FORMAT;
    sample.data._maximum = camera->jpeg_size;
    sample.data._length = camera->jpeg_size;
    sample.data._buffer = camera->jpeg_buffer;
    sample.data._release = false;
    camera->last_result = dds_write(camera->writer, &sample);
    if (camera->last_result != DDS_RETCODE_OK) {
        camera->dropped++;
        app_log_write(APP_LOG_WARN, "CAM image write failed rc=%ld", (long)camera->last_result);
        return false;
    }
    camera->published++;

    if (timestamp_ms < camera->next_info_publish_at) return true;
    sensor_msgs_msg_dds__CameraInfo_ info = { 0 };
    info.header.stamp.sec = sample.header.stamp.sec;
    info.header.stamp.nanosec = sample.header.stamp.nanosec;
    info.header.frame_id = (char *)camera->frame_id;
    info.height = camera->height;
    info.width = camera->width;
    info.distortion_model = "plumb_bob";
    info.D[0] = 0.0; info.D[1] = 0.0; info.D[2] = 0.0; info.D[3] = 0.0; info.D[4] = 0.0;
    info.K[0] = (double)camera->width; info.K[4] = (double)camera->height; info.K[8] = 1.0;
    info.R[0] = 1.0; info.R[4] = 1.0; info.R[8] = 1.0;
    info.P[0] = (double)camera->width; info.P[5] = (double)camera->height; info.P[10] = 1.0; info.P[11] = 0.0;
    info.binning_x = 0; info.binning_y = 0;
    info.roi.x_offset = 0; info.roi.y_offset = 0; info.roi.height = camera->height; info.roi.width = camera->width; info.roi.do_rectify = false;
    dds_return_t info_result = dds_write(camera->info_writer, &info);
    if (info_result != DDS_RETCODE_OK) {
        app_log_write(APP_LOG_WARN, "CAM info write failed rc=%ld", (long)info_result);
    }
    camera->next_info_publish_at = timestamp_ms + CAMERA_INFO_INTERVAL_MS;
    return true;
}

int32_t ros2_camera_writer_matches(ros2_camera *camera) {
    dds_publication_matched_status_t status = { 0 };
    camera->last_result = dds_get_publication_matched_status(camera->writer, &status);
    return camera->last_result < 0 ? camera->last_result : (int32_t)status.current_count;
}

dds_entity_t ros2_camera_writer_entity(const ros2_camera *camera) {
    return camera != NULL ? camera->writer : DDS_ENTITY_NIL;
}

void ros2_camera_stop(ros2_camera *camera) {
    ros2_camera_release_capture(camera);
    if (camera->jpeg_encoder != NULL) {
        tjDestroy(camera->jpeg_encoder);
        camera->jpeg_encoder = NULL;
    }
    tjFree(camera->jpeg_buffer);
    free(camera->rgbx_buffer);
    free(camera->preview_buffer);
    free(camera->capture_buffer);
    camera->jpeg_buffer = NULL;
    camera->rgbx_buffer = NULL;
    camera->preview_buffer = NULL;
    camera->capture_buffer = NULL;
    camera->capture_buffer_size = 0;
    camera->transfer_bytes = 0;
    camera->next_info_publish_at = 0;
    camera->first_receive_logged = false;
    camera->receiving = false;
    camera->capture_active = false;
    camera->camera_initialized = false;

    ros2_topic_interface compressed = {
        .name = ROS2_CAMERA_TOPIC,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->topic,
        .writer = camera->writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface info = {
        .name = ROS2_CAMERA_INFO_TOPIC,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->info_topic,
        .writer = camera->info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };

    ros2_topic_cleanup(&compressed);
    ros2_topic_cleanup(&info);
    camera->topic = compressed.topic;
    camera->writer = compressed.writer;
    camera->info_topic = info.topic;
    camera->info_writer = info.writer;
    camera->last_result = DDS_RETCODE_OK;
}