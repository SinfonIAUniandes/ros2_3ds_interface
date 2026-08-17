#include "ui_internal.h"

#include <stdio.h>

void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot) {
    const ui_topic_definition *topic = &ui_topics[ui->selected_topic];
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "%u registered", (unsigned int)ui_topic_count);
    ui_header(ui, "Topics", subtitle);
    ui_panel(ui, 8, 57, 384, 66);
    ui_rect(8, 57, 5, 66, ui->selected_topic == 0 ? ui->theme.accent
            : (snapshot->dds_running ? ui->theme.success : ui->theme.danger));
    ui_text(ui, 23, 68, 0.52f, ui->theme.text, topic->name);
    ui_text(ui, 23, 94, 0.36f, ui->theme.muted, topic->type_name);
    ui_badge(ui, 318, 67, "RELIABLE", true);
    ui_textf(ui, 260, 99, 0.31f, ui->theme.muted, "pub %ld  sub %ld",
             (long)snapshot->writer_matches, (long)snapshot->reader_matches);

    ui_panel(ui, 8, 133, 384, 90);
    ui_text(ui, 18, 143, 0.37f, ui->theme.muted, "SELECTED TOPIC ACTIVITY");
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
    const ui_controls *controls = app_ui_controls();
    const ui_topic_definition *topic = &ui_topics[ui->selected_topic];
    ui_textf(ui, 8, 40, 0.43f, ui->theme.text, "%s  |  Message preview", topic->name);
    ui_panel(ui, 8, 62, 304, 60);
    ui_text(ui, 18, 72, 0.31f, ui->theme.muted, "LAST PUBLISHED");
    ui_textf(ui, 18, 92, 0.40f, ui->theme.accent, "%.36s", snapshot->last_published_message);
    ui_panel(ui, 8, 130, 304, 60);
    ui_text(ui, 18, 140, 0.31f, ui->theme.muted, "LAST RECEIVED");
    ui_textf(ui, 18, 160, 0.40f, ui->theme.success, "%.36s", snapshot->last_received_message);
    ui_textf(ui, 8, 205, 0.31f, ui->theme.muted, "%s/%s select topic  |  %s open",
             app_ui_control_label(controls->previous_item), app_ui_control_label(controls->next_item),
             app_ui_control_label(controls->activate));
    ui_text(ui, 8, 221, 0.29f, ui->theme.muted, topic->qos);
}