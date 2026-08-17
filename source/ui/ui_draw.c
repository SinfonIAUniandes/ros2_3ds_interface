#include "ui_internal.h"

#include <stdarg.h>
#include <stdio.h>

void ui_text(ui_context *ui, float x, float y, float scale, u32 color, const char *text) {
    C2D_Text parsed;
    C2D_TextParse(&parsed, ui->text_buffer, text != NULL ? text : "-");
    C2D_TextOptimize(&parsed);
    C2D_DrawText(&parsed, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void ui_textf(ui_context *ui, float x, float y, float scale, u32 color, const char *format, ...) {
    char buffer[192];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    ui_text(ui, x, y, scale, color, buffer);
}

void ui_rect(float x, float y, float width, float height, u32 color) {
    C2D_DrawRectSolid(x, y, 0.5f, width, height, color);
}

void ui_panel(ui_context *ui, float x, float y, float width, float height) {
    ui_rect(x, y, width, height, ui->theme.border);
    ui_rect(x + 1, y + 1, width - 2, height - 2, ui->theme.surface);
}

void ui_header(ui_context *ui, const char *title, const char *subtitle) {
    ui_rect(0, 0, 400, 18, ui->theme.header);
    ui_rect(0, 18, 400, 30, ui->theme.toolbar);
    ui_text(ui, 8, 2, 0.42f, ui->theme.on_color, "ROS 2 3DS");
    ui_text(ui, 10, 24, 0.55f, ui->theme.on_color, title);
    if (subtitle != NULL) {
        ui_text(ui, 220, 27, 0.34f, ui->theme.on_color, subtitle);
    }
}

void ui_bottom_nav(ui_context *ui) {
    ui_rect(0, 0, 320, 32, ui->theme.header);
    const float width = 80.0f;
    static const ui_view_id primary[] = { UI_VIEW_HOME, UI_VIEW_TOPICS, UI_VIEW_SERVICES, UI_VIEW_MENU };
    for (int index = 0; index < 4; index++) {
        const bool selected = ui->view == primary[index];
        if (selected) {
            ui_rect(index * width, 29, width, 3, ui->theme.accent);
        }
        ui_text(ui, index * width + 7, 9, 0.34f,
                selected ? ui->theme.on_color : C2D_Color32(205, 215, 235, 255),
                ui_views[primary[index]].label);
    }
}

void ui_badge(ui_context *ui, float x, float y, const char *label, bool active) {
    const u32 color = active ? ui->theme.success : ui->theme.danger;
    ui_rect(x, y, 62, 17, color);
    ui_text(ui, x + 6, y + 3, 0.34f, ui->theme.on_color, label);
}

void ui_metric(ui_context *ui, float x, float y, const char *label, const char *value,
               u32 value_color) {
    ui_text(ui, x, y, 0.32f, ui->theme.muted, label);
    ui_text(ui, x, y + 13, 0.48f, value_color, value);
}

void ui_button(ui_context *ui, float x, float y, float width, float height,
               const char *key, const char *label, bool active) {
    ui_rect(x, y, width, height, active ? ui->theme.selected : ui->theme.surface);
    ui_rect(x, y, 31, height, active ? ui->theme.accent : ui->theme.toolbar);
    ui_text(ui, x + 10, y + 9, 0.45f, ui->theme.on_color, key);
    ui_text(ui, x + 39, y + 9, 0.40f, ui->theme.text, label);
}

bool ui_hit(const touchPosition *touch, float x, float y, float width, float height) {
    return touch != NULL && touch->px >= x && touch->px < x + width &&
           touch->py >= y && touch->py < y + height;
}