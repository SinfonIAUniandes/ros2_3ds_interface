#ifndef ROS2_3DS_UI_THEME_H
#define ROS2_3DS_UI_THEME_H

#include <3ds/types.h>

#include <stdbool.h>

typedef struct {
    u32 background;
    u32 surface;
    u32 header;
    u32 toolbar;
    u32 accent;
    u32 selected;
    u32 success;
    u32 warning;
    u32 danger;
    u32 text;
    u32 muted;
    u32 border;
    u32 on_color;
    float corner_radius;
    float spacing;
} ui_theme;

void ui_theme_defaults(ui_theme *theme);
bool ui_theme_load(ui_theme *theme, const char *path);

#endif