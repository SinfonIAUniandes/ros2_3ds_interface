#include "ui_internal.h"

#include <stdio.h>

static void diagnostic_row(ui_context *ui, float label_x, float value_x, float y,
                           const char *label, const char *value, u32 color) {
    ui_text(ui, label_x, y, 0.34f, ui->theme.muted, label);
    ui_text(ui, value_x, y, 0.36f, color, value);
}

void ui_view_details_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Details", snapshot->build_id);
    ui_panel(ui, 8, 57, 188, 166);
    ui_text(ui, 18, 68, 0.37f, ui->theme.muted, "NETWORK & DISCOVERY");
    diagnostic_row(ui, 18, 91, 94, "Local IP", snapshot->local_ip, ui->theme.text);
    diagnostic_row(ui, 18, 91, 116, "Netmask", snapshot->netmask, ui->theme.text);
    diagnostic_row(ui, 18, 91, 138, "Broadcast", snapshot->broadcast_ip, ui->theme.text);
    diagnostic_row(ui, 18, 91, 160, "Peer", snapshot->static_peer ? snapshot->peer_ip : "automatic", ui->theme.text);
    diagnostic_row(ui, 18, 91, 182, "Config", snapshot->external_config ? "SD override" : "ROMFS", ui->theme.text);
    char value[32];
    snprintf(value, sizeof(value), "%u", snapshot->probe_port);
    diagnostic_row(ui, 18, 91, 204, "Probe port", value, ui->theme.text);

    ui_panel(ui, 204, 57, 188, 166);
    ui_text(ui, 214, 68, 0.37f, ui->theme.muted, "RTPS TRANSPORT");
    snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->rtps_tx_multicast);
    diagnostic_row(ui, 214, 310, 94, "TX multicast", value, ui->theme.accent);
    snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->rtps_tx_unicast);
    diagnostic_row(ui, 214, 310, 116, "TX unicast", value, ui->theme.accent);
    snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->rtps_rx_total);
    diagnostic_row(ui, 214, 310, 138, "RX total", value, ui->theme.success);
    snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->rtps_rx_remote);
    diagnostic_row(ui, 214, 310, 160, "RX remote", value, ui->theme.success);
    snprintf(value, sizeof(value), "%ld / %ld", (long)snapshot->rtps_last_send_errno,
             (long)snapshot->rtps_last_recv_errno);
    diagnostic_row(ui, 214, 310, 182, "Socket errno", value,
                   snapshot->rtps_last_send_errno || snapshot->rtps_last_recv_errno
                       ? ui->theme.warning : ui->theme.text);
    snprintf(value, sizeof(value), "%ld", (long)snapshot->rtps_multicast_if_errno);
    diagnostic_row(ui, 214, 310, 204, "Mcast IF", value, ui->theme.warning);
}

void ui_view_details_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    ui_text(ui, 8, 41, 0.50f, ui->theme.text, "Probe and endpoint detail");
    ui_panel(ui, 8, 66, 304, 126);
    char value[48];
    snprintf(value, sizeof(value), "M %lu  U %lu  RX %lu", (unsigned long)snapshot->probe_multicast_sent,
             (unsigned long)snapshot->probe_unicast_sent, (unsigned long)snapshot->probe_received);
    diagnostic_row(ui, 18, 136, 78, "Probe packets", value, ui->theme.text);
    diagnostic_row(ui, 18, 136, 102, "Last sender", snapshot->last_probe_sender, ui->theme.text);
    snprintf(value, sizeof(value), "%.26s", snapshot->last_probe_payload);
    diagnostic_row(ui, 18, 136, 126, "Last payload", value, ui->theme.text);
    snprintf(value, sizeof(value), "%ld / %ld", (long)snapshot->writer_qos_policy,
             (long)snapshot->reader_qos_policy);
    diagnostic_row(ui, 18, 136, 150, "QoS policy", value, ui->theme.text);
    snprintf(value, sizeof(value), "%ld / %ld / %ld", (long)snapshot->probe_bind_error,
             (long)snapshot->probe_membership_error, (long)snapshot->probe_loopback_error);
    diagnostic_row(ui, 18, 136, 174, "Bind/join/loop", value,
                   snapshot->probe_bind_error || snapshot->probe_membership_error
                       ? ui->theme.warning : ui->theme.text);
    ui_text(ui, 8, 217, 0.34f, ui->theme.muted, "B returns to Menu");
}