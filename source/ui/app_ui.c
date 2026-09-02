#include "app_ui.h"

#include <citro2d.h>

#include "ui_internal.h"

static ui_context g_ui;

const ui_view_definition ui_views[UI_VIEW_COUNT] = {
    { UI_VIEW_HOME, "Home", ui_view_home_top, ui_view_home_bottom },
    { UI_VIEW_TOPICS, "Topics", ui_view_topics_top, ui_view_topics_bottom },
    { UI_VIEW_SERVICES, "Services", ui_view_services_top, ui_view_services_bottom },
    { UI_VIEW_MENU, "Menu", ui_view_menu_top, ui_view_menu_bottom },
    { UI_VIEW_DETAILS, "Details", ui_view_details_top, ui_view_details_bottom },
    { UI_VIEW_LOGS, "Logs", ui_view_logs_top, ui_view_logs_bottom },
    { UI_VIEW_SETTINGS, "Settings", ui_view_settings_top, ui_view_settings_bottom }
};

bool ui_is_primary_view(ui_view_id view) {
    return view == UI_VIEW_HOME || view == UI_VIEW_TOPICS || view == UI_VIEW_SERVICES || view == UI_VIEW_MENU;
}

void ui_set_view(ui_context *ui, ui_view_id view) {
    ui->view = view;
    if (view == UI_VIEW_MENU) ui->selected_menu_item = 0;
}

static ui_view_id next_primary_view(ui_view_id current, int direction) {
    static const ui_view_id primary[] = { UI_VIEW_HOME, UI_VIEW_TOPICS, UI_VIEW_SERVICES, UI_VIEW_MENU };
    size_t index = 0;
    for (; index < sizeof(primary) / sizeof(primary[0]); index++) {
        if (primary[index] == current) break;
    }
    if (index == sizeof(primary) / sizeof(primary[0])) return UI_VIEW_HOME;
    if (direction > 0) index = (index + 1) % (sizeof(primary) / sizeof(primary[0]));
    else index = index == 0 ? (sizeof(primary) / sizeof(primary[0])) - 1 : index - 1;
    return primary[index];
}

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
    g_ui.view = UI_VIEW_HOME;
    app_ui_init_camera_settings();
    ui_theme_defaults(&g_ui.theme);
    ui_theme_load(&g_ui.theme, "romfs:/ui/theme.ini");
    ui_theme_load(&g_ui.theme, "sdmc:/3ds/ros2_3ds_interface/theme.ini");
    ui_controls_defaults(&g_ui.controls);
    ui_controls_load(&g_ui.controls, "romfs:/ui/controls.ini");
    ui_controls_load(&g_ui.controls, "sdmc:/3ds/ros2_3ds_interface/controls.ini");
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
    if (keys_down & g_ui.controls.exit) actions |= UI_ACTION_EXIT;
    if (keys_down & g_ui.controls.next_view) actions |= UI_ACTION_NEXT_VIEW;
    if (keys_down & g_ui.controls.previous_view) actions |= UI_ACTION_PREVIOUS_VIEW;
    if (keys_down & g_ui.controls.next_item) actions |= UI_ACTION_NEXT_ITEM;
    if (keys_down & g_ui.controls.previous_item) actions |= UI_ACTION_PREVIOUS_ITEM;
    if (keys_down & g_ui.controls.activate) actions |= UI_ACTION_ACTIVATE;
    if (keys_down & g_ui.controls.back) actions |= UI_ACTION_BACK;
    if (keys_down & KEY_TOUCH) {
        const float tab_width = 320.0f / 4.0f;
        if (touch != NULL && touch->py < 32) {
            int index = (int)(touch->px / tab_width);
            if (index >= 0 && index < 4) {
                ui_set_view(&g_ui, (ui_view_id)index);
            }
        } else if (g_ui.view == UI_VIEW_HOME) {
            if (ui_hit(touch, 8, 48, 148, 42)) actions |= UI_ACTION_PUBLISH_ONCE;
            else if (ui_hit(touch, 164, 48, 148, 42)) actions |= UI_ACTION_TOGGLE_PUBLISHING;
            else if (ui_hit(touch, 8, 98, 148, 42)) actions |= UI_ACTION_TOGGLE_LISTENER;
            else if (ui_hit(touch, 164, 98, 148, 42)) actions |= UI_ACTION_SEND_PROBE;
        }
    }
    if (actions & UI_ACTION_NEXT_VIEW) ui_set_view(&g_ui, next_primary_view(g_ui.view, 1));
    if (actions & UI_ACTION_PREVIOUS_VIEW) ui_set_view(&g_ui, next_primary_view(g_ui.view, -1));
    if (g_ui.view == UI_VIEW_TOPICS) {
        if (actions & UI_ACTION_NEXT_ITEM) g_ui.selected_topic =
            (uint8_t)((g_ui.selected_topic + 1) % ui_topic_count);
        if (actions & UI_ACTION_PREVIOUS_ITEM) g_ui.selected_topic =
            g_ui.selected_topic == 0 ? (uint8_t)(ui_topic_count - 1)
                                     : (uint8_t)(g_ui.selected_topic - 1);
        if (actions & UI_ACTION_ACTIVATE) {
            actions |= g_ui.selected_topic == 0 ? UI_ACTION_TOGGLE_CHATTER_TOPIC
                     : g_ui.selected_topic == 1 ? UI_ACTION_TOGGLE_IMU_TOPIC
                                                 : UI_ACTION_TOGGLE_CAMERA_TOPIC;
        }
    } else if (g_ui.view == UI_VIEW_SETTINGS) {
        if (actions & UI_ACTION_NEXT_ITEM) g_ui.selected_settings_item =
            (g_ui.selected_settings_item + 1u) % 2u;
        if (actions & UI_ACTION_PREVIOUS_ITEM) g_ui.selected_settings_item =
            g_ui.selected_settings_item == 0u ? 1u : 0u;
        if (actions & UI_ACTION_ACTIVATE) {
            actions |= g_ui.selected_settings_item == 0u
                ? UI_ACTION_EDIT_NAMESPACE : UI_ACTION_EDIT_DOMAIN_ID;
        }
    } else if (g_ui.view == UI_VIEW_MENU) {
        if (actions & UI_ACTION_NEXT_ITEM) g_ui.selected_menu_item = (g_ui.selected_menu_item + 1) % 3;
        if (actions & UI_ACTION_PREVIOUS_ITEM) g_ui.selected_menu_item =
            g_ui.selected_menu_item == 0 ? 2 : g_ui.selected_menu_item - 1;
        if (actions & UI_ACTION_ACTIVATE) {
            ui_set_view(&g_ui, g_ui.selected_menu_item == 0 ? UI_VIEW_DETAILS
                              : g_ui.selected_menu_item == 1 ? UI_VIEW_LOGS : UI_VIEW_SETTINGS);
        }
    } else if (!ui_is_primary_view(g_ui.view) && (actions & UI_ACTION_BACK)) {
        ui_set_view(&g_ui, UI_VIEW_MENU);
    }
    if (g_ui.view == UI_VIEW_HOME) {
        if (keys_down & g_ui.controls.publish_once) actions |= UI_ACTION_PUBLISH_ONCE;
        if (keys_down & g_ui.controls.toggle_publishing) actions |= UI_ACTION_TOGGLE_PUBLISHING;
        if (keys_down & g_ui.controls.toggle_listener) actions |= UI_ACTION_TOGGLE_LISTENER;
        if (keys_down & g_ui.controls.send_probe) actions |= UI_ACTION_SEND_PROBE;
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

const ui_controls *app_ui_controls(void) {
    return &g_ui.controls;
}

const char *app_ui_control_label(u32 mask) {
    return ui_control_label(mask);
}

uint8_t app_ui_camera_setting_index(void) {
    return g_ui.camera_setting_index;
}

void app_ui_set_camera_settings(const ros2_camera_config *config) {
    if (config == NULL) {
        ros2_camera_config_defaults(&g_ui.camera_config);
        return;
    }
    g_ui.camera_config = *config;
    if (g_ui.camera_setting_index > 3) {
        g_ui.camera_setting_index = 0;
    }
}

void app_ui_cycle_camera_setting(int direction) {
    if (direction >= 0) {
        g_ui.camera_setting_index = (g_ui.camera_setting_index + 1u) % 4u;
    } else {
        if (g_ui.camera_setting_index == 0u) {
            g_ui.camera_setting_index = 3u;
        } else {
            g_ui.camera_setting_index--;
        }
    }
}

bool app_ui_apply_camera_setting(int direction, ros2_camera_config *config) {
    if (config == NULL) {
        return false;
    }
    switch (g_ui.camera_setting_index) {
        case 0:
            if (direction > 0) {
                config->source = (config->source + 1u) % 3u;
            } else if (config->source == 0u) {
                config->source = ROS2_CAMERA_SOURCE_OUTER_RIGHT;
            } else {
                config->source--;
            }
            return true;
        case 1:
            if (direction > 0) {
                config->resolution = (config->resolution == ROS2_CAMERA_RESOLUTION_QQVGA)
                    ? ROS2_CAMERA_RESOLUTION_QVGA : ROS2_CAMERA_RESOLUTION_QQVGA;
            } else {
                config->resolution = (config->resolution == ROS2_CAMERA_RESOLUTION_QQVGA)
                    ? ROS2_CAMERA_RESOLUTION_QQVGA : ROS2_CAMERA_RESOLUTION_QQVGA;
            }
            return true;
        case 2: {
            static const uint32_t fps_values[] = { 5u, 10u, 15u };
            size_t index = 0;
            for (; index < sizeof(fps_values) / sizeof(fps_values[0]); index++) {
                if (fps_values[index] == config->fps) {
                    break;
                }
            }
            if (index >= sizeof(fps_values) / sizeof(fps_values[0])) {
                config->fps = 5u;
                index = 0u;
            }
            if (direction > 0) index = (index + 1u) % (sizeof(fps_values) / sizeof(fps_values[0]));
            else if (index == 0u) index = (sizeof(fps_values) / sizeof(fps_values[0])) - 1u;
            else index--;
            config->fps = fps_values[index];
            return true;
        }
        case 3:
            config->jpeg_quality += direction > 0 ? 5 : -5;
            if (config->jpeg_quality < 40) config->jpeg_quality = 40;
            if (config->jpeg_quality > 85) config->jpeg_quality = 85;
            return true;
        default:
            return false;
    }
}

void app_ui_init_camera_settings(void) {
    ros2_camera_config_defaults(&g_ui.camera_config);
    g_ui.camera_setting_index = 0u;
    g_ui.selected_settings_item = 0u;
}
