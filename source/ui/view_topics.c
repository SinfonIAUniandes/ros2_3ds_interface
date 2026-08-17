#include "ui_internal.h"

#include <stdio.h>

void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Topics", "1 registered topic");
    ui_panel(ui, 8, 57, 384, 66);
    ui_rect(8, 57, 5, 66, snapshot->dds_running ? ui->theme.success : ui->theme.danger);
    ui_text(ui, 23, 68, 0.52f, ui->theme.text, "/chatter");
    ui_text(ui, 23, 94, 0.33f, ui->theme.muted, "std_msgs/msg/String");
    ui_badge(ui, 318, 67, "RELIABLE", true);
    ui_textf(ui, 260, 99, 0.31f, ui->theme.muted, "pub %ld  sub %ld",
             (long)snapshot->writer_matches, (long)snapshot->reader_matches);

    ui_panel(ui, 8, 133, 384, 90);
    ui_text(ui, 18, 143, 0.37f, ui->theme.muted, "LIVE COUNTERS");
    char value[32];
    snprintf(value, sizeof(value), "%llu", (unsigned long long)snapshot->chatter_transmitted);
    ui_metric(ui, 18, 169, "TX samples", value, ui->theme.accent);
    snprintf(value, sizeof(value), "%llu", (unsigned long long)snapshot->chatter_received);
    ui_metric(ui, 116, 169, "RX samples", value, ui->theme.success);
    snprintf(value, sizeof(value), "%ld / %ld", (long)snapshot->writer_qos_rejections,
             (long)snapshot->reader_qos_rejections);
    ui_metric(ui, 226, 169, "QoS rejects", value,
              snapshot->writer_qos_rejections || snapshot->reader_qos_rejections
                  ? ui->theme.warning : ui->theme.text);
}

void ui_view_topics_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    ui_text(ui, 8, 41, 0.48f, ui->theme.text, "/chatter");
    ui_text(ui, 8, 65, 0.34f, ui->theme.muted, "Publisher + subscriber");
    ui_panel(ui, 8, 91, 304, 94);
    ui_text(ui, 18, 103, 0.34f, ui->theme.muted, "QoS profile");
    ui_text(ui, 18, 126, 0.40f, ui->theme.text, "Reliable  |  Volatile  |  Keep last 10");
    ui_textf(ui, 18, 153, 0.34f, ui->theme.muted, "Publishing %s  |  Listening %s",
             snapshot->publishing ? "ON" : "OFF", snapshot->listening ? "ON" : "OFF");
    ui_text(ui, 8, 218, 0.30f, ui->theme.muted, "Topic modules can be added to the view registry");
}