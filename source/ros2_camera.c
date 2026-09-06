#include "ros2_camera.h"

#include "ros2_common.h"
#include "ros2_names.h"
#include "ros2_types.h"
#include "logging/app_log.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <turbojpeg.h>

#define NINTENDO_EPOCH_OFFSET_SECONDS UINT64_C(2208988800)
#define CAMERA_MAX_DDS_BYTES (8u * 1024u)
#define CAMERA_INFO_INTERVAL_MS UINT64_C(5000)

static uint8_t camera_selection_for_source(ros2_camera_source source) {
    return source == ROS2_CAMERA_SOURCE_INNER ? SELECT_IN1
         : source == ROS2_CAMERA_SOURCE_OUTER_LEFT ? SELECT_OUT1 : SELECT_OUT2;
}

static uint8_t camera_port_for_source(ros2_camera_source source) {
    return source == ROS2_CAMERA_SOURCE_OUTER_RIGHT ? PORT_CAM2 : PORT_CAM1;
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
    camera->front.frame_id = "3ds_inner_camera_optical_frame";
    camera->back.frame_id = (camera->config.back_source == ROS2_CAMERA_SOURCE_OUTER_RIGHT)
        ? "3ds_outer_right_camera_optical_frame"
        : "3ds_outer_left_camera_optical_frame";
}

static bool camera_call(ros2_camera *camera, const char *stage, Result result) {
    camera->camera_result = result;
    if (R_SUCCEEDED(result)) return true;
    camera->last_result = DDS_RETCODE_ERROR;
    app_log_write(APP_LOG_ERROR, "CAM %s failed 0x%08lX", stage, (unsigned long)result);
    return false;
}

static void ros2_camera_deactivate_hardware(ros2_camera *camera) {
    if (camera->capture_active) {
        CAMU_StopCapture(camera_port_for_source(camera->active_hardware_source));
        camera->capture_active = false;
    }
    if (camera->receive_event != 0) {
        svcCloseHandle(camera->receive_event);
        camera->receive_event = 0;
    }
    camera->receiving = false;
    if (camera->camera_initialized) {
        CAMU_Activate(SELECT_NONE);
    }
}

static void ros2_camera_release_capture(ros2_camera *camera) {
    ros2_camera_deactivate_hardware(camera);
    if (camera->camera_initialized) {
        camExit();
        camera->camera_initialized = false;
    }
}

static bool ros2_camera_activate_source(ros2_camera *camera, ros2_camera_source source) {
    if (camera->active_hardware_source == source && camera->capture_active) {
        return true;
    }
    if (camera->capture_active) {
        CAMU_StopCapture(camera_port_for_source(camera->active_hardware_source));
        camera->capture_active = false;
    }
    if (camera->receive_event != 0) {
        svcCloseHandle(camera->receive_event);
        camera->receive_event = 0;
    }
    camera->receiving = false;

    if (!camera->camera_initialized) {
        if (!camera_call(camera, "init", camInit())) {
            return false;
        }
        camera->camera_initialized = true;
    }

    CAMU_Activate(SELECT_NONE);

    uint8_t selection = camera_selection_for_source(source);
    uint8_t port = camera_port_for_source(source);

    if (!camera_call(camera, "size", CAMU_SetSize(selection,
                     camera->config.resolution == ROS2_CAMERA_RESOLUTION_QVGA ? SIZE_QVGA : SIZE_QQVGA,
                     CONTEXT_A)) ||
        !camera_call(camera, "format", CAMU_SetOutputFormat(selection, OUTPUT_RGB_565, CONTEXT_A)) ||
        !camera_call(camera, "rate", CAMU_SetFrameRate(selection, camera_frame_rate(camera))) ||
        !camera_call(camera, "exposure", CAMU_SetAutoExposure(selection, true)) ||
        !camera_call(camera, "white-balance", CAMU_SetAutoWhiteBalance(selection, true)) ||
        !camera_call(camera, "activate", CAMU_Activate(selection))) {
        return false;
    }

    if (!camera_call(camera, "max-bytes", CAMU_GetMaxBytes(&camera->transfer_bytes,
                     camera->width, camera->height)) ||
        camera->transfer_bytes == 0 || camera->transfer_bytes > INT16_MAX ||
        camera->capture_buffer_size < camera->transfer_bytes ||
        !camera_call(camera, "transfer", CAMU_SetTransferBytes(port,
                     camera->transfer_bytes, camera->width, camera->height)) ||
        !camera_call(camera, "clear", CAMU_ClearBuffer(port)) ||
        !camera_call(camera, "capture", CAMU_StartCapture(port))) {
        camera->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    camera->capture_active = true;
    camera->active_hardware_source = source;
    return true;
}

static bool ros2_camera_arm_receive(ros2_camera *camera, ros2_camera_source source) {
    if (!ros2_camera_activate_source(camera, source)) {
        return false;
    }
    if (camera->receiving) return true;
    if (camera->transfer_bytes == 0 || camera->transfer_bytes > INT16_MAX ||
        camera->capture_buffer_size < camera->transfer_bytes) return false;

    uint8_t port = camera_port_for_source(source);
    if (!camera->capture_active) {
        if (!camera_call(camera, "capture-restart", CAMU_StartCapture(port))) {
            return false;
        }
        camera->capture_active = true;
    }
    if (!camera->first_receive_logged) {
        app_log_write(APP_LOG_INFO, "CAM first arm image=%lu transfer=%lu port=%u",
                      (unsigned long)camera->capture_buffer_size,
                      (unsigned long)camera->transfer_bytes, (unsigned int)port);
        camera->first_receive_logged = true;
    }
    camera->camera_result = CAMU_SetReceiving(&camera->receive_event, camera->capture_buffer,
                                               port, camera->capture_buffer_size,
                                               (s16)camera->transfer_bytes);
    if (R_FAILED(camera->camera_result)) {
        app_log_write(APP_LOG_ERROR, "CAM receive failed 0x%08lX", (unsigned long)camera->camera_result);
        return false;
    }
    camera->receiving = true;
    return true;
}

void ros2_camera_init(ros2_camera *camera) {
    memset(camera, 0, sizeof(*camera));
    camera->front.topic = DDS_ENTITY_NIL;
    camera->front.writer = DDS_ENTITY_NIL;
    camera->front.info_topic = DDS_ENTITY_NIL;
    camera->front.info_writer = DDS_ENTITY_NIL;
    camera->back.topic = DDS_ENTITY_NIL;
    camera->back.writer = DDS_ENTITY_NIL;
    camera->back.info_topic = DDS_ENTITY_NIL;
    camera->back.info_writer = DDS_ENTITY_NIL;
    camera->last_result = DDS_RETCODE_OK;
    ros2_camera_config_defaults(&camera->config);
}

void ros2_camera_config_defaults(ros2_camera_config *config) {
    config->back_source = ROS2_CAMERA_SOURCE_OUTER_LEFT;
    config->resolution = ROS2_CAMERA_RESOLUTION_QQVGA;
    config->fps = 5;
    config->jpeg_quality = 70;
}

bool ros2_camera_start(ros2_camera *camera, dds_entity_t participant,
                       const ros2_camera_config *config, const char *ros_namespace,
                       bool front_enabled, bool back_enabled) {
    if (camera->front.writer > DDS_ENTITY_NIL || camera->back.writer > DDS_ENTITY_NIL) {
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
    camera->front.enabled = front_enabled;
    camera->back.enabled = back_enabled;
    app_log_write(APP_LOG_INFO, "CAM creating topics (front & back)...");

    char front_compressed_name[256];
    char front_info_name[256];
    char back_compressed_name[256];
    char back_info_name[256];
    if (!ros2_dds_name(front_compressed_name, sizeof(front_compressed_name), "rt", ros_namespace,
                       "camera/front/image_raw/compressed") ||
        !ros2_dds_name(front_info_name, sizeof(front_info_name), "rt", ros_namespace,
                       "camera/front/camera_info") ||
        !ros2_dds_name(back_compressed_name, sizeof(back_compressed_name), "rt", ros_namespace,
                       "camera/back/image_raw/compressed") ||
        !ros2_dds_name(back_info_name, sizeof(back_info_name), "rt", ros_namespace,
                       "camera/back/camera_info")) {
        camera->last_result = DDS_RETCODE_BAD_PARAMETER;
        return false;
    }

    ros2_topic_interface front_compressed = {
        .name = front_compressed_name,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->front.topic,
        .writer = camera->front.writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface front_info = {
        .name = front_info_name,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->front.info_topic,
        .writer = camera->front.info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface back_compressed = {
        .name = back_compressed_name,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->back.topic,
        .writer = camera->back.writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface back_info = {
        .name = back_info_name,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->back.info_topic,
        .writer = camera->back.info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = false
    };

    if (!ros2_topic_create_endpoints(&front_compressed, participant, 1, false, DDS_MSECS(100), true) ||
        !ros2_topic_create_endpoints(&front_info, participant, 1, false, DDS_MSECS(100), true) ||
        !ros2_topic_create_endpoints(&back_compressed, participant, 1, false, DDS_MSECS(100), true) ||
        !ros2_topic_create_endpoints(&back_info, participant, 1, false, DDS_MSECS(100), true)) {
        camera->last_result = front_compressed.last_result != DDS_RETCODE_OK ? front_compressed.last_result
                            : front_info.last_result != DDS_RETCODE_OK ? front_info.last_result
                            : back_compressed.last_result != DDS_RETCODE_OK ? back_compressed.last_result
                            : back_info.last_result;
        app_log_write(APP_LOG_ERROR, "CAM topic creation failed");
        ros2_topic_cleanup(&front_compressed);
        ros2_topic_cleanup(&front_info);
        ros2_topic_cleanup(&back_compressed);
        ros2_topic_cleanup(&back_info);
        camera->front.topic = front_compressed.topic;
        camera->front.writer = front_compressed.writer;
        camera->front.info_topic = front_info.topic;
        camera->front.info_writer = front_info.writer;
        camera->back.topic = back_compressed.topic;
        camera->back.writer = back_compressed.writer;
        camera->back.info_topic = back_info.topic;
        camera->back.info_writer = back_info.writer;
        ros2_camera_stop(camera);
        return false;
    }

    camera->front.topic = front_compressed.topic;
    camera->front.writer = front_compressed.writer;
    camera->front.info_topic = front_info.topic;
    camera->front.info_writer = front_info.writer;
    camera->back.topic = back_compressed.topic;
    camera->back.writer = back_compressed.writer;
    camera->back.info_topic = back_info.topic;
    camera->back.info_writer = back_info.writer;

    app_log_write(APP_LOG_INFO, "CAM allocating buffers...");
    const size_t pixel_count = camera->width * camera->height;
    camera->capture_buffer_size = pixel_count * sizeof(*camera->capture_buffer);
    camera->capture_buffer = malloc(camera->capture_buffer_size);
    camera->rgbx_buffer = malloc(pixel_count * 4);
    camera->front.preview_buffer = malloc(pixel_count * 3);
    camera->back.preview_buffer = malloc(pixel_count * 3);
    camera->jpeg_capacity = tjBufSize(camera->width, camera->height, TJSAMP_420);
    camera->jpeg_buffer = tjAlloc((int)camera->jpeg_capacity);

    if (camera->capture_buffer == NULL || camera->rgbx_buffer == NULL ||
        camera->front.preview_buffer == NULL || camera->back.preview_buffer == NULL ||
        camera->jpeg_buffer == NULL) {
        camera->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        app_log_write(APP_LOG_ERROR, "CAM buffer alloc failed");
        ros2_camera_stop(camera);
        return false;
    }
    memset(camera->front.preview_buffer, 0, pixel_count * 3);
    memset(camera->back.preview_buffer, 0, pixel_count * 3);

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

    if (camera->front.enabled || camera->back.enabled) {
        ros2_camera_source initial_source = camera->front.enabled
            ? ROS2_CAMERA_SOURCE_INNER : camera->config.back_source;
        if (!ros2_camera_activate_source(camera, initial_source)) {
            app_log_write(APP_LOG_WARN, "CAM initial source activation deferred");
        }
    }

    camera->last_result = DDS_RETCODE_OK;
    return true;
}

bool ros2_camera_set_front_enabled(ros2_camera *camera, bool enabled) {
    if (camera == NULL) return false;
    camera->front.enabled = enabled;
    return true;
}

bool ros2_camera_set_back_enabled(ros2_camera *camera, bool enabled) {
    if (camera == NULL) return false;
    camera->back.enabled = enabled;
    return true;
}

bool ros2_camera_poll(ros2_camera *camera, uint64_t timestamp_ms, bool publish) {
    if (camera == NULL || !camera->camera_initialized) return false;
    if (!camera->front.enabled && !camera->back.enabled) {
        if (camera->capture_active) {
            ros2_camera_deactivate_hardware(camera);
        }
        return true;
    }

    ros2_camera_source target_source;
    if (camera->front.enabled && !camera->back.enabled) {
        target_source = ROS2_CAMERA_SOURCE_INNER;
    } else if (!camera->front.enabled && camera->back.enabled) {
        target_source = camera->config.back_source;
    } else {
        target_source = camera->alternate_flag ? ROS2_CAMERA_SOURCE_INNER : camera->config.back_source;
    }

    if (!ros2_camera_arm_receive(camera, target_source)) {
        camera->last_result = DDS_RETCODE_ERROR;
        return false;
    }

    uint8_t port = camera_port_for_source(target_source);
    bool receive_finished = false;
    camera->camera_result = CAMU_IsFinishedReceiving(&receive_finished, port);
    if (R_FAILED(camera->camera_result)) {
        camera->last_result = DDS_RETCODE_ERROR;
        app_log_write(APP_LOG_ERROR, "CAM receive status failed 0x%08lX",
                      (unsigned long)camera->camera_result);
        return false;
    }
    if (!receive_finished) return false;

    if (!camera_call(camera, "frame-stop", CAMU_StopCapture(port))) {
        return false;
    }
    camera->capture_active = false;
    if (camera->receive_event != 0) {
        svcCloseHandle(camera->receive_event);
        camera->receive_event = 0;
    }
    camera->receiving = false;

    ros2_camera_stream *stream = (target_source == ROS2_CAMERA_SOURCE_INNER)
        ? &camera->front : &camera->back;
    stream->captured++;

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
        if (stream->preview_buffer != NULL) {
            stream->preview_buffer[index * 3 + 0] = blue;
            stream->preview_buffer[index * 3 + 1] = green;
            stream->preview_buffer[index * 3 + 2] = red;
        }
    }

    if (camera->front.enabled && camera->back.enabled) {
        camera->alternate_flag = !camera->alternate_flag;
    }

    if (!publish || !stream->enabled || stream->writer <= DDS_ENTITY_NIL) {
        camera->last_result = DDS_RETCODE_OK;
        return true;
    }

    unsigned char *jpeg = camera->jpeg_buffer;
    stream->jpeg_size = camera->jpeg_capacity;
    camera->jpeg_result = tjCompress2(camera->jpeg_encoder, camera->rgbx_buffer, camera->width,
                                      camera->width * 4, camera->height, TJPF_BGRX,
                                      &jpeg, &stream->jpeg_size, TJSAMP_420,
                                      camera->config.jpeg_quality, TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
    if (camera->jpeg_result != 0) {
        app_log_write(APP_LOG_ERROR, "CAM JPEG encode failed: %s",
                      tjGetErrorStr2(camera->jpeg_encoder));
        camera->last_result = DDS_RETCODE_ERROR;
        return false;
    }
    if (stream->jpeg_size > CAMERA_MAX_DDS_BYTES && camera->config.jpeg_quality > 40) {
        stream->jpeg_size = camera->jpeg_capacity;
        camera->jpeg_result = tjCompress2(camera->jpeg_encoder, camera->rgbx_buffer, camera->width,
                                          camera->width * 4, camera->height, TJPF_BGRX,
                                          &jpeg, &stream->jpeg_size, TJSAMP_420, 40,
                                          TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
    }
    if (camera->jpeg_result != 0 || stream->jpeg_size > CAMERA_MAX_DDS_BYTES) {
        stream->dropped++;
        camera->last_result = DDS_RETCODE_OK;
        app_log_write(APP_LOG_WARN, "CAM frame skipped jpeg=%lu B", stream->jpeg_size);
        return false;
    }
    stream->encoded++;

    uint64_t unix_seconds = timestamp_ms / 1000;
    if (unix_seconds >= NINTENDO_EPOCH_OFFSET_SECONDS) unix_seconds -= NINTENDO_EPOCH_OFFSET_SECONDS;
    else unix_seconds = 0;
    sensor_msgs_msg_dds__CompressedImage_ sample = { 0 };
    sample.header.stamp.sec = (int32_t)unix_seconds;
    sample.header.stamp.nanosec = (uint32_t)((timestamp_ms % 1000) * UINT64_C(1000000));
    sample.header.frame_id = (char *)stream->frame_id;
    sample.format = (char *)ROS2_CAMERA_FORMAT;
    sample.data._maximum = stream->jpeg_size;
    sample.data._length = stream->jpeg_size;
    sample.data._buffer = camera->jpeg_buffer;
    sample.data._release = false;
    camera->last_result = dds_write(stream->writer, &sample);
    if (camera->last_result != DDS_RETCODE_OK) {
        stream->dropped++;
        app_log_write(APP_LOG_WARN, "CAM image write failed rc=%ld", (long)camera->last_result);
        return false;
    }
    stream->published++;

    if (timestamp_ms < stream->next_info_publish_at || stream->info_writer <= DDS_ENTITY_NIL) return true;
    sensor_msgs_msg_dds__CameraInfo_ info = { 0 };
    info.header.stamp.sec = sample.header.stamp.sec;
    info.header.stamp.nanosec = sample.header.stamp.nanosec;
    info.header.frame_id = (char *)stream->frame_id;
    info.height = camera->height;
    info.width = camera->width;
    info.distortion_model = "plumb_bob";
    info.D[0] = 0.0; info.D[1] = 0.0; info.D[2] = 0.0; info.D[3] = 0.0; info.D[4] = 0.0;
    info.K[0] = (double)camera->width; info.K[4] = (double)camera->height; info.K[8] = 1.0;
    info.R[0] = 1.0; info.R[4] = 1.0; info.R[8] = 1.0;
    info.P[0] = (double)camera->width; info.P[5] = (double)camera->height; info.P[10] = 1.0; info.P[11] = 0.0;
    info.binning_x = 0; info.binning_y = 0;
    info.roi.x_offset = 0; info.roi.y_offset = 0; info.roi.height = camera->height; info.roi.width = camera->width; info.roi.do_rectify = false;
    dds_return_t info_result = dds_write(stream->info_writer, &info);
    if (info_result != DDS_RETCODE_OK) {
        app_log_write(APP_LOG_WARN, "CAM info write failed rc=%ld", (long)info_result);
    }
    stream->next_info_publish_at = timestamp_ms + CAMERA_INFO_INTERVAL_MS;
    return true;
}

int32_t ros2_camera_front_writer_matches(ros2_camera *camera) {
    if (camera == NULL || camera->front.writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    camera->last_result = dds_get_publication_matched_status(camera->front.writer, &status);
    return camera->last_result < 0 ? camera->last_result : (int32_t)status.current_count;
}

int32_t ros2_camera_back_writer_matches(ros2_camera *camera) {
    if (camera == NULL || camera->back.writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    camera->last_result = dds_get_publication_matched_status(camera->back.writer, &status);
    return camera->last_result < 0 ? camera->last_result : (int32_t)status.current_count;
}

dds_entity_t ros2_camera_front_writer_entity(const ros2_camera *camera) {
    return camera != NULL ? camera->front.writer : DDS_ENTITY_NIL;
}

dds_entity_t ros2_camera_back_writer_entity(const ros2_camera *camera) {
    return camera != NULL ? camera->back.writer : DDS_ENTITY_NIL;
}

void ros2_camera_stop(ros2_camera *camera) {
    ros2_camera_release_capture(camera);
    if (camera->jpeg_encoder != NULL) {
        tjDestroy(camera->jpeg_encoder);
        camera->jpeg_encoder = NULL;
    }
    tjFree(camera->jpeg_buffer);
    free(camera->rgbx_buffer);
    free(camera->front.preview_buffer);
    free(camera->back.preview_buffer);
    free(camera->capture_buffer);
    camera->jpeg_buffer = NULL;
    camera->rgbx_buffer = NULL;
    camera->front.preview_buffer = NULL;
    camera->back.preview_buffer = NULL;
    camera->capture_buffer = NULL;
    camera->capture_buffer_size = 0;
    camera->transfer_bytes = 0;
    camera->front.next_info_publish_at = 0;
    camera->back.next_info_publish_at = 0;
    camera->first_receive_logged = false;
    camera->receiving = false;
    camera->capture_active = false;
    camera->camera_initialized = false;

    ros2_topic_interface front_compressed = {
        .name = ROS2_CAMERA_FRONT_TOPIC,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->front.topic,
        .writer = camera->front.writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface front_info = {
        .name = ROS2_CAMERA_FRONT_INFO_TOPIC,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->front.info_topic,
        .writer = camera->front.info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface back_compressed = {
        .name = ROS2_CAMERA_BACK_TOPIC,
        .type = &sensor_msgs_msg_dds__CompressedImage__desc,
        .topic = camera->back.topic,
        .writer = camera->back.writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };
    ros2_topic_interface back_info = {
        .name = ROS2_CAMERA_BACK_INFO_TOPIC,
        .type = &sensor_msgs_msg_dds__CameraInfo__desc,
        .topic = camera->back.info_topic,
        .writer = camera->back.info_writer,
        .reader = DDS_ENTITY_NIL,
        .last_result = camera->last_result,
        .writer_enabled = true,
        .reader_enabled = false
    };

    ros2_topic_cleanup(&front_compressed);
    ros2_topic_cleanup(&front_info);
    ros2_topic_cleanup(&back_compressed);
    ros2_topic_cleanup(&back_info);
    camera->front.topic = front_compressed.topic;
    camera->front.writer = front_compressed.writer;
    camera->front.info_topic = front_info.topic;
    camera->front.info_writer = front_info.writer;
    camera->back.topic = back_compressed.topic;
    camera->back.writer = back_compressed.writer;
    camera->back.info_topic = back_info.topic;
    camera->back.info_writer = back_info.writer;
    camera->last_result = DDS_RETCODE_OK;
}