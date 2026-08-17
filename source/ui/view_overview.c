#include "ui_internal.h"

#include <stdio.h>

static void card_title(ui_context *ui, float x, float y, const char *title) {
    ui_text(ui, x + 9, y + 7, 0.37f, ui->theme.muted, title);
}

void ui_view_overview_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Overview", snapshot->local_ip);

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
    card_title(ui, 8, 141, "CHATTER");
    char tx[24];
    char rx[24];
    snprintf(tx, sizeof(tx), "%llu", (unsigned long long)snapshot->chatter_transmitted);
    snprintf(rx, sizeof(rx), "%llu", (unsigned long long)snapshot->chatter_received);
    ui_metric(ui, 17, 166, "SENT", tx, ui->theme.accent);
    ui_metric(ui, 94, 166, "RECEIVED", rx, ui->theme.success);
    ui_textf(ui, 17, 207, 0.31f, ui->theme.muted, "Matches  pub %ld  sub %ld",
             (long)snapshot->writer_matches, (long)snapshot->reader_matches);

    ui_panel(ui, 204, 141, 188, 82);
    card_title(ui, 204, 141, "ACTIVITY");
    ui_badge(ui, 213, 166, snapshot->publishing ? "PUB ON" : "PUB OFF", snapshot->publishing);
    ui_badge(ui, 282, 166, snapshot->listening ? "SUB ON" : "SUB OFF", snapshot->listening);
    ui_textf(ui, 213, 207, 0.31f, ui->theme.muted, "RTPS remote %lu  |  Log %s",
             (unsigned long)snapshot->rtps_rx_remote, snapshot->log_has_error ? "warning" : "clean");
}

void ui_view_overview_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    ui_text(ui, 8, 38, 0.36f, ui->theme.muted, "Quick actions");
    ui_button(ui, 8, 48, 148, 42, "A", "Publish once", false);
    ui_button(ui, 164, 48, 148, 42, "B", "Auto publish", snapshot->publishing);
    ui_button(ui, 8, 98, 148, 42, "Y", "Subscriber", snapshot->listening);
    ui_button(ui, 164, 98, 148, 42, "X", "UDP probe", false);

    ui_panel(ui, 8, 151, 304, 59);
    ui_text(ui, 18, 161, 0.34f, ui->theme.muted, "Current topic");
    ui_text(ui, 18, 179, 0.48f, ui->theme.text, "/chatter");
    ui_textf(ui, 128, 181, 0.32f, ui->theme.muted, "%lu ms interval",
             (unsigned long)snapshot->send_interval_ms);
    ui_text(ui, 8, 220, 0.30f, ui->theme.muted, "L/R views     Touch tabs     START exit");
}