#include "ui_internal.h"

#include <stdio.h>

static void topic_row(ui_context *ui, const ui_snapshot *snapshot, size_t index, float y) {
    const ui_topic_definition *topic = &ui_topics[index];
    const bool selected = ui->selected_topic == index;
    const bool enabled = index == 0 ? snapshot->chatter_topic_enabled : snapshot->imu_topic_enabled;
    const bool available = index == 0 ? snapshot->dds_running : snapshot->imu_sensors_enabled;
    const bool active = enabled && available;
    ui_rect(8, y, 384, 49, selected ? ui->theme.selected : ui->theme.surface);
    ui_rect(8, y, 5, 49, selected ? ui->theme.accent : (active ? ui->theme.success : ui->theme.danger));
    ui_text(ui, 22, y + 7, 0.44f, ui->theme.text, topic->name);
    ui_text(ui, 22, y + 28, 0.32f, ui->theme.muted, topic->type_name);
        ui_text(ui, 310, y + 16, 0.32f, active ? ui->theme.success : ui->theme.danger,
            active ? "ON" : "OFF");
}

void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot) {
    const ui_topic_definition *topic = &ui_topics[ui->selected_topic];
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "%u registered", (unsigned int)ui_topic_count);
    ui_header(ui, "Topics", subtitle);
    for (size_t index = 0; index < ui_topic_count && index < 3; index++) {
        topic_row(ui, snapshot, index, 57 + (float)index * 55.0f);
    }

    ui_panel(ui, 8, 171, 384, 52);
    ui_text(ui, 18, 180, 0.32f, ui->theme.muted, "SELECTED");
    ui_text(ui, 18, 199, 0.42f, ui->theme.text, topic->name);
    if (ui->selected_topic == 0) {
        ui_textf(ui, 180, 199, 0.33f, ui->theme.muted, "TX %llu  RX %llu",
                 (unsigned long long)snapshot->chatter_transmitted,
                 (unsigned long long)snapshot->chatter_received);
    } else {
        ui_textf(ui, 180, 199, 0.33f, ui->theme.muted, "TX %llu  match %ld",
                 (unsigned long long)snapshot->imu_transmitted,
                 (long)snapshot->imu_writer_matches);
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
    } else {
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
    }
    ui_textf(ui, 8, 205, 0.31f, ui->theme.muted, "%s/%s select topic",
             app_ui_control_label(controls->previous_item), app_ui_control_label(controls->next_item));
    const bool enabled = ui->selected_topic == 0 ? snapshot->chatter_topic_enabled
                                                  : snapshot->imu_topic_enabled;
    ui_textf(ui, 8, 221, 0.29f, ui->theme.muted, "%s %s publisher  |  %s",
             app_ui_control_label(controls->activate), enabled ? "disable" : "enable", topic->qos);
}
