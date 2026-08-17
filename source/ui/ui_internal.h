#ifndef ROS2_3DS_UI_INTERNAL_H
#define ROS2_3DS_UI_INTERNAL_H

#include <citro2d.h>

#include "app_ui.h"
#include "ui_theme.h"

typedef struct {
    C3D_RenderTarget *top;
    C3D_RenderTarget *bottom;
    C2D_TextBuf text_buffer;
    ui_theme theme;
    ui_view_id view;
} ui_context;

typedef struct {
    ui_view_id id;
    const char *label;
    void (*render_top)(ui_context *ui, const ui_snapshot *snapshot);
    void (*render_bottom)(ui_context *ui, const ui_snapshot *snapshot);
} ui_view_definition;

extern const ui_view_definition ui_views[UI_VIEW_COUNT];

void ui_text(ui_context *ui, float x, float y, float scale, u32 color, const char *text);
void ui_textf(ui_context *ui, float x, float y, float scale, u32 color, const char *format, ...);
void ui_rect(float x, float y, float width, float height, u32 color);
void ui_panel(ui_context *ui, float x, float y, float width, float height);
void ui_header(ui_context *ui, const char *title, const char *subtitle);
void ui_bottom_nav(ui_context *ui);
void ui_badge(ui_context *ui, float x, float y, const char *label, bool active);
void ui_metric(ui_context *ui, float x, float y, const char *label, const char *value,
               u32 value_color);
void ui_button(ui_context *ui, float x, float y, float width, float height,
               const char *key, const char *label, bool active);
bool ui_hit(const touchPosition *touch, float x, float y, float width, float height);

void ui_view_overview_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_overview_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_topics_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_diagnostics_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_diagnostics_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_logs_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_logs_bottom(ui_context *ui, const ui_snapshot *snapshot);

#endif