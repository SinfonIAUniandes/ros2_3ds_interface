#include "app_ui.h"

#include <citro2d.h>

#include "ui_internal.h"

static ui_context g_ui;

const ui_view_definition ui_views[UI_VIEW_COUNT] = {
    { UI_VIEW_OVERVIEW, "Home", ui_view_overview_top, ui_view_overview_bottom },
    { UI_VIEW_TOPICS, "Topics", ui_view_topics_top, ui_view_topics_bottom },
    { UI_VIEW_DIAGNOSTICS, "Details", ui_view_diagnostics_top, ui_view_diagnostics_bottom },
    { UI_VIEW_LOGS, "Logs", ui_view_logs_top, ui_view_logs_bottom }
};

bool app_ui_init(void) {
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        return false;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        return false;
    }
    C2D_Prepare();
    g_ui.text_buffer = C2D_TextBufNew(16384);
    if (g_ui.text_buffer == NULL) {
        C2D_Fini();
        C3D_Fini();
        return false;
    }
    g_ui.top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_ui.bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_ui.view = UI_VIEW_OVERVIEW;
    ui_theme_defaults(&g_ui.theme);
    ui_theme_load(&g_ui.theme, "romfs:/ui/theme.ini");
    ui_theme_load(&g_ui.theme, "sdmc:/3ds/ros2_3ds_interface/theme.ini");
    return g_ui.top != NULL && g_ui.bottom != NULL;
}

void app_ui_exit(void) {
    if (g_ui.text_buffer != NULL) {
        C2D_TextBufDelete(g_ui.text_buffer);
        g_ui.text_buffer = NULL;
    }
    C2D_Fini();
    C3D_Fini();
}

ui_action app_ui_handle_input(u32 keys_down, const touchPosition *touch) {
    ui_action actions = UI_ACTION_NONE;
    if (keys_down & KEY_L) {
        g_ui.view = g_ui.view == 0 ? UI_VIEW_COUNT - 1 : g_ui.view - 1;
    }
    if (keys_down & KEY_R) {
        g_ui.view = (g_ui.view + 1) % UI_VIEW_COUNT;
    }
    if (keys_down & KEY_A) actions |= UI_ACTION_PUBLISH_ONCE;
    if (keys_down & KEY_B) actions |= UI_ACTION_TOGGLE_PUBLISHING;
    if (keys_down & KEY_Y) actions |= UI_ACTION_TOGGLE_LISTENER;
    if (keys_down & KEY_X) actions |= UI_ACTION_SEND_PROBE;
    if (keys_down & KEY_START) actions |= UI_ACTION_EXIT;
    if (keys_down & KEY_TOUCH) {
        const float tab_width = 320.0f / UI_VIEW_COUNT;
        if (touch != NULL && touch->py < 32) {
            int index = (int)(touch->px / tab_width);
            if (index >= 0 && index < UI_VIEW_COUNT) {
                g_ui.view = (ui_view_id)index;
            }
        } else if (g_ui.view == UI_VIEW_OVERVIEW) {
            if (ui_hit(touch, 8, 48, 148, 42)) actions |= UI_ACTION_PUBLISH_ONCE;
            else if (ui_hit(touch, 164, 48, 148, 42)) actions |= UI_ACTION_TOGGLE_PUBLISHING;
            else if (ui_hit(touch, 8, 98, 148, 42)) actions |= UI_ACTION_TOGGLE_LISTENER;
            else if (ui_hit(touch, 164, 98, 148, 42)) actions |= UI_ACTION_SEND_PROBE;
        }
    }
    return actions;
}

void app_ui_render(const ui_snapshot *snapshot) {
    C2D_TextBufClear(g_ui.text_buffer);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(g_ui.top, g_ui.theme.background);
    C2D_TargetClear(g_ui.bottom, g_ui.theme.background);
    C2D_SceneBegin(g_ui.top);
    ui_views[g_ui.view].render_top(&g_ui, snapshot);
    C2D_SceneBegin(g_ui.bottom);
    ui_bottom_nav(&g_ui);
    ui_views[g_ui.view].render_bottom(&g_ui, snapshot);
    C3D_FrameEnd(0);
}

ui_view_id app_ui_current_view(void) {
    return g_ui.view;
}