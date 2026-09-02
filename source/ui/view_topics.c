#include "ui_internal.h"

#include <stdio.h>

static void topic_row(ui_context *ui, const ui_snapshot *snapshot, size_t index, float y) {
    const ui_topic_definition *topic = &ui_topics[index];
    const bool selected = ui->selected_topic == index;
    const bool enabled = index == 0 ? snapshot->chatter_topic_enabled
                       : index == 1 ? snapshot->imu_topic_enabled
                       : snapshot->camera_topic_enabled;
    const bool available = index == 0 ? snapshot->dds_running
                         : index == 1 ? snapshot->imu_sensors_enabled
                         : snapshot->camera_available;
    const bool active = enabled && available;
    ui_rect(8, y, 384, 43, selected ? ui->theme.selected : ui->theme.surface);
    ui_rect(8, y, 5, 43, selected ? ui->theme.accent : (active ? ui->theme.success : ui->theme.danger));
    ui_text(ui, 22, y + 5, 0.40f, ui->theme.text, topic->name);
    ui_text(ui, 22, y + 25, 0.29f, ui->theme.muted, topic->type_name);
    ui_text(ui, 310, y + 14, 0.32f, active ? ui->theme.success : ui->theme.danger,
            active ? "ON" : "OFF");
}

void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot) {
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "%u registered", (unsigned int)ui_topic_count);
    ui_header(ui, "Topics", subtitle);

    const size_t visible_rows = 6;
    const size_t scroll_start = ui->selected_topic >= visible_rows
        ? ui->selected_topic - (visible_rows - 1)
        : 0u;
    const size_t max_index = ui_topic_count < visible_rows ? ui_topic_count : visible_rows;
    const size_t draw_count = ui_topic_count > scroll_start ? ui_topic_count - scroll_start : 0u;
    const size_t rows_to_draw = draw_count < max_index ? draw_count : max_index;

    for (size_t row = 0; row < rows_to_draw; row++) {
        const size_t index = scroll_start + row;
        const float y = 44.0f + (float)row * 30.0f;
        topic_row(ui, snapshot, index, y);
    }

    if (ui_topic_count > visible_rows) {
        const float list_height = (float)visible_rows * 30.0f;
        const float track_top = 44.0f;
        const float track_height = list_height;
        const float handle_height = track_height * (float)visible_rows / (float)ui_topic_count;
        const float handle_top = track_top + ((float)ui->selected_topic / (float)(ui_topic_count - 1)) * (track_height - handle_height);
        ui_rect(392.0f, track_top, 4.0f, track_height, ui->theme.surface);
        ui_rect(392.0f, handle_top, 4.0f, handle_height, ui->theme.accent);
    }
}

void ui_view_topics_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    const ui_controls *controls = app_ui_controls();
    const ui_topic_definition *topic = &ui_topics[ui->selected_topic];
    ui_textf(ui, 8, 40, 0.43f, ui->theme.text, "%s", topic->name);

    if (ui->selected_topic == 0) {
        ui_panel(ui, 8, 62, 304, 60);
        ui_text(ui, 18, 72, 0.31f, ui->theme.muted, "LAST PUBLISHED");
        ui_textf(ui, 18, 92, 0.40f, ui->theme.accent, "%.36s", snapshot->last_published_message);
        ui_panel(ui, 8, 130, 304, 60);
        ui_text(ui, 18, 140, 0.31f, ui->theme.muted, "LAST RECEIVED");
        ui_textf(ui, 18, 160, 0.40f, ui->theme.success, "%.36s", snapshot->last_received_message);
    } else if (ui->selected_topic == 1) {
        ui_panel(ui, 8, 62, 304, 60);
        ui_textf(ui, 18, 72, 0.34f, ui->theme.muted, "ANGULAR VELOCITY  rad/s   %lu Hz",
                 (unsigned long)snapshot->imu_publish_hz);
        ui_textf(ui, 18, 96, 0.41f, ui->theme.accent, "x %.3f   y %.3f   z %.3f",
                 snapshot->imu_angular_velocity[0], snapshot->imu_angular_velocity[1],
                 snapshot->imu_angular_velocity[2]);
        ui_panel(ui, 8, 130, 304, 60);
        ui_text(ui, 18, 140, 0.34f, ui->theme.muted, "LINEAR ACCELERATION  m/s^2");
        ui_textf(ui, 18, 164, 0.41f, ui->theme.success, "x %.3f   y %.3f   z %.3f",
                 snapshot->imu_linear_acceleration[0], snapshot->imu_linear_acceleration[1],
                 snapshot->imu_linear_acceleration[2]);
    } else if (ui->selected_topic == 2 || ui->selected_topic == 3) {
        ui_panel(ui, 8, 62, 304, 34);
        ui_textf(ui, 18, 72, 0.34f, ui->theme.muted, "JPEG CAMERA  %s  %u FPS",
                 snapshot->camera_resolution == 0 ? "QQVGA" : "QVGA",
                 (unsigned int)snapshot->camera_fps);
        ui_textf(ui, 192, 72, 0.34f, ui->theme.accent, "Frames %llu",
                 (unsigned long long)snapshot->camera_published);
        ui_panel(ui, 8, 104, 136, 108);
        ui_textf(ui, 18, 116, 0.32f, ui->theme.muted, "CAPTURED");
        ui_textf(ui, 18, 133, 0.42f, ui->theme.text, "%llu",
                 (unsigned long long)snapshot->camera_captured,
                 (unsigned long long)snapshot->camera_encoded);
        ui_textf(ui, 18, 154, 0.32f, ui->theme.muted, "ENCODED");
        ui_textf(ui, 18, 171, 0.42f, ui->theme.text, "%llu",
                 (unsigned long long)snapshot->camera_encoded);
        ui_textf(ui, 18, 192, 0.32f, ui->theme.success, "MATCH %ld",
                 (long)snapshot->camera_writer_matches);
        ui_panel(ui, 152, 104, 160, 108);
        ui_text(ui, 160, 109, 0.28f, ui->theme.muted, "LIVE FEED");
        if (snapshot->camera_preview != NULL && snapshot->camera_preview_width > 0 &&
            snapshot->camera_preview_height > 0) {
            ui_draw_image_preview(168.0f, 114.0f, 128.0f, 96.0f,
                                  snapshot->camera_preview,
                                  snapshot->camera_preview_width,
                                  snapshot->camera_preview_height);
        }
    }
    ui_textf(ui, 8, 220, 0.31f, ui->theme.muted, "%s/%s select topic",
             app_ui_control_label(controls->previous_item), app_ui_control_label(controls->next_item));
    const bool enabled = ui->selected_topic == 0 ? snapshot->chatter_topic_enabled
                       : ui->selected_topic == 1 ? snapshot->imu_topic_enabled
                       : snapshot->camera_topic_enabled;
    ui_textf(ui, 8, 221, 0.29f, ui->theme.muted, "%s %s publisher  |  %s",
             app_ui_control_label(controls->activate), enabled ? "disable" : "enable", topic->qos);
}
