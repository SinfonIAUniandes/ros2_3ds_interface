#include "ui_internal.h"

#include "logging/app_log.h"

static u32 log_color(const ui_context *ui, app_log_level level) {
    if (level == APP_LOG_ERROR) return ui->theme.danger;
    if (level == APP_LOG_WARN) return ui->theme.warning;
    if (level == APP_LOG_DDS) return ui->theme.accent;
    return ui->theme.text;
}

void ui_view_logs_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Event log", snapshot->log_has_error ? "Errors recorded" : "Live session");
    app_log_record records[8];
    size_t count = app_log_copy_recent(records, 8);
    if (count == 0) {
        ui_text(ui, 16, 70, 0.42f, ui->theme.muted, "No events yet");
        return;
    }
    float y = 57;
    for (size_t index = 0; index < count; index++) {
        const app_log_record *record = &records[index];
        if (index % 2 == 0) {
            ui_rect(8, y, 384, 20, ui->theme.surface);
        }
        ui_textf(ui, 13, y + 4, 0.29f, ui->theme.muted, "%5llu",
                 (unsigned long long)(record->timestamp_ms / 1000));
        ui_text(ui, 58, y + 4, 0.29f, log_color(ui, record->level),
                app_log_level_name(record->level));
        ui_textf(ui, 91, y + 4, 0.29f, ui->theme.text, "%.47s", record->message);
        y += 21;
    }
}

void ui_view_logs_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    ui_text(ui, 8, 42, 0.40f, ui->theme.text, "Persistent session log");
    ui_panel(ui, 8, 66, 304, 82);
    ui_text(ui, 18, 78, 0.31f, ui->theme.muted, "SD path");
    ui_textf(ui, 18, 99, 0.29f, ui->theme.text, "%.46s", snapshot->log_path);
    ui_badge(ui, 18, 121, snapshot->log_has_error ? "ERRORS" : "CLEAN", !snapshot->log_has_error);
    ui_text(ui, 8, 164, 0.31f, ui->theme.muted, "Colors: blue DDS, amber warning, red error");
    ui_text(ui, 8, 218, 0.30f, ui->theme.muted, "Newest eight events are shown on the top screen");
}