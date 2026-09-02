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
    ui_controls controls;
    uint8_t selected_topic;
    uint8_t selected_menu_item;
    uint8_t selected_settings_item;
    uint8_t camera_setting_index;
    ros2_camera_config camera_config;
} ui_context;

typedef struct {
    ui_view_id id;
    const char *label;
    void (*render_top)(ui_context *ui, const ui_snapshot *snapshot);
    void (*render_bottom)(ui_context *ui, const ui_snapshot *snapshot);
} ui_view_definition;

typedef struct {
    const char *name;
    const char *type_name;
    const char *qos;
} ui_topic_definition;

extern const ui_view_definition ui_views[UI_VIEW_COUNT];
extern const ui_topic_definition ui_topics[];
extern const size_t ui_topic_count;

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
void ui_draw_image_preview(float x, float y, float width, float height,
                          const uint8_t *data, uint32_t image_width,
                          uint32_t image_height);
bool ui_hit(const touchPosition *touch, float x, float y, float width, float height);
bool ui_is_primary_view(ui_view_id view);
void ui_set_view(ui_context *ui, ui_view_id view);
void ui_controls_defaults(ui_controls *controls);
bool ui_controls_load(ui_controls *controls, const char *path);
const char *ui_control_label(u32 mask);

void ui_view_home_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_home_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_topics_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_topics_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_services_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_services_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_menu_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_menu_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_details_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_details_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_logs_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_logs_bottom(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_settings_top(ui_context *ui, const ui_snapshot *snapshot);
void ui_view_settings_bottom(ui_context *ui, const ui_snapshot *snapshot);

#endif