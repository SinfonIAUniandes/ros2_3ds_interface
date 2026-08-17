#include "ui_internal.h"

#include <stdio.h>

static void card_title(ui_context *ui, float x, float y, const char *title) {
    ui_text(ui, x + 9, y + 7, 0.37f, ui->theme.muted, title);
}

void ui_view_home_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Home", snapshot->local_ip);

    ui_panel(ui, 8, 57, 188, 76);
    card_title(ui, 8, 57, "CONNECTION");
    ui_text(ui, 17, 82, 0.56f, ui->theme.text, snapshot->network_ready ? snapshot->local_ip : "Offline");
    ui_badge(ui, 125, 78, snapshot->network_ready ? "ONLINE" : "OFFLINE", snapshot->network_ready);
    ui_textf(ui, 17, 112, 0.32f, ui->theme.muted, "Domain %lu  |  %s",
             (unsigned long)snapshot->domain_id, snapshot->static_peer ? "Static peer" : "Auto discovery");

    ui_panel(ui, 204, 57, 188, 76);
    card_title(ui, 204, 57, "DDS RUNTIME");
    ui_text(ui, 213, 82, 0.56f, snapshot->dds_running ? ui->theme.success : ui->theme.danger,
            snapshot->dds_status);
    ui_badge(ui, 321, 78, snapshot->dds_running ? "READY" : "STOPPED", snapshot->dds_running);
    ui_textf(ui, 213, 112, 0.32f, ui->theme.muted, "Result %ld  |  Graph %ld",
             (long)snapshot->dds_result, (long)snapshot->graph_matches);

    ui_panel(ui, 8, 141, 188, 82);
    card_title(ui, 8, 141, "CHATTER PUBLISHER");
    char tx[24];
    char rx[24];
    snprintf(tx, sizeof(tx), "%llu", (unsigned long long)snapshot->chatter_transmitted);
    snprintf(rx, sizeof(rx), "%llu", (unsigned long long)snapshot->chatter_received);
    ui_metric(ui, 17, 166, "SENT", tx, ui->theme.accent);
    ui_metric(ui, 94, 166, "RECEIVED", rx, ui->theme.success);
    ui_textf(ui, 17, 207, 0.31f, ui->theme.muted, "%s  |  pub %ld  sub %ld",
             snapshot->chatter_topic_enabled ? "Enabled" : "Disabled",
             (long)snapshot->writer_matches, (long)snapshot->reader_matches);

    ui_panel(ui, 204, 141, 188, 82);
    card_title(ui, 204, 141, "PUBLISHING");
    ui_badge(ui, 213, 166, snapshot->publishing ? "ALL ON" : "ALL OFF", snapshot->publishing);
    ui_badge(ui, 282, 166, snapshot->listening ? "SUB ON" : "SUB OFF", snapshot->listening);
    ui_textf(ui, 213, 207, 0.31f, ui->theme.muted, "IMU %s  |  RTPS %lu",
             snapshot->imu_topic_enabled ? "enabled" : "disabled",
             (unsigned long)snapshot->rtps_rx_remote);
}

void ui_view_home_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    ui_text(ui, 8, 38, 0.36f, ui->theme.muted, "Publisher controls");
    const ui_controls *controls = app_ui_controls();
    ui_button(ui, 8, 48, 148, 42, app_ui_control_label(controls->publish_once), "Publish enabled once", false);
    ui_button(ui, 164, 48, 148, 42, app_ui_control_label(controls->toggle_publishing),
              snapshot->publishing ? "Stop all publishing" : "Start all publishing", snapshot->publishing);
    ui_button(ui, 8, 98, 148, 42, app_ui_control_label(controls->toggle_listener), "Subscriber", snapshot->listening);
    ui_button(ui, 164, 98, 148, 42, app_ui_control_label(controls->send_probe), "UDP probe", false);

    ui_panel(ui, 8, 151, 304, 59);
    ui_text(ui, 18, 161, 0.34f, ui->theme.muted, "Enabled publishers");
    ui_textf(ui, 18, 179, 0.43f, ui->theme.text, "Chatter %s  |  IMU %s",
             snapshot->chatter_topic_enabled ? "ON" : "OFF",
             snapshot->imu_topic_enabled ? "ON" : "OFF");
    ui_textf(ui, 18, 196, 0.31f, ui->theme.muted, "Chatter %lu ms  |  IMU %lu Hz",
             (unsigned long)snapshot->send_interval_ms,
             (unsigned long)snapshot->imu_publish_hz);
    ui_textf(ui, 8, 220, 0.30f, ui->theme.muted, "%s/%s tabs  |  Touch tabs  |  %s exit",
             app_ui_control_label(controls->previous_view), app_ui_control_label(controls->next_view),
             app_ui_control_label(controls->exit));
}