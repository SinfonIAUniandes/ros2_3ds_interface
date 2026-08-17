#include "ui_internal.h"

static void menu_row(ui_context *ui, float y, const char *title, const char *detail, bool selected) {
    ui_rect(8, y, 304, 43, selected ? ui->theme.selected : ui->theme.surface);
    ui_rect(8, y, 5, 43, selected ? ui->theme.accent : ui->theme.border);
    ui_text(ui, 22, y + 7, 0.45f, ui->theme.text, title);
    ui_text(ui, 22, y + 27, 0.31f, ui->theme.muted, detail);
}

void ui_view_services_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Services", "Server registry");
    ui_panel(ui, 8, 58, 384, 164);
    ui_rect(8, 58, 5, 164, snapshot->add_two_ints_running ? ui->theme.success : ui->theme.danger);
    ui_text(ui, 22, 76, 0.50f, ui->theme.text, "/add_two_ints");
    ui_text(ui, 22, 101, 0.35f, ui->theme.muted, "example_interfaces/srv/AddTwoInts");
    ui_badge(ui, 310, 74, snapshot->add_two_ints_running ? "READY" : "OFF", snapshot->add_two_ints_running);
    ui_textf(ui, 22, 134, 0.40f, ui->theme.text, "Requests handled  %llu",
             (unsigned long long)snapshot->add_two_ints_requests_handled);
    ui_textf(ui, 22, 162, 0.40f, ui->theme.accent, "%lld + %lld = %lld",
             (long long)snapshot->add_two_ints_last_a,
             (long long)snapshot->add_two_ints_last_b,
             (long long)snapshot->add_two_ints_last_sum);
    ui_textf(ui, 22, 195, 0.33f, ui->theme.muted, "Request match %ld  |  Reply match %ld",
             (long)snapshot->add_two_ints_request_matches,
             (long)snapshot->add_two_ints_response_matches);
}

void ui_view_services_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    (void)snapshot;
    const ui_controls *controls = app_ui_controls();
    ui_text(ui, 8, 45, 0.47f, ui->theme.text, "Add two integers server");
    ui_panel(ui, 8, 70, 304, 112);
    ui_text(ui, 18, 83, 0.34f, ui->theme.muted, "Call from ROS 2");
    ui_text(ui, 18, 106, 0.33f, ui->theme.text, "ros2 service call /add_two_ints");
    ui_text(ui, 18, 126, 0.33f, ui->theme.text, "example_interfaces/srv/AddTwoInts");
    ui_text(ui, 18, 146, 0.33f, ui->theme.text, "\"{a: 2, b: 3}\"");
    ui_textf(ui, 18, 166, 0.31f, ui->theme.muted, "Taken %llu  |  Replies %llu",
             (unsigned long long)snapshot->add_two_ints_samples_taken,
             (unsigned long long)snapshot->add_two_ints_responses_written);
    ui_textf(ui, 18, 184, 0.31f, ui->theme.muted, "Take rc %ld  |  Write rc %ld",
             (long)snapshot->add_two_ints_last_take_result,
             (long)snapshot->add_two_ints_last_write_result);
    ui_textf(ui, 8, 218, 0.34f, ui->theme.muted, "%s/%s switch main tabs",
             app_ui_control_label(controls->previous_view), app_ui_control_label(controls->next_view));
}

void ui_view_menu_top(ui_context *ui, const ui_snapshot *snapshot) {
    ui_header(ui, "Menu", "System and diagnostics");
    menu_row(ui, 58, "Details", "Network, RTPS, probe and QoS", ui->selected_menu_item == 0);
    menu_row(ui, 109, "Logs", "Recent events and session path", ui->selected_menu_item == 1);
    menu_row(ui, 160, "Settings", "Theme and controller bindings", ui->selected_menu_item == 2);
    ui_text(ui, 18, 214, 0.32f, ui->theme.muted,
            snapshot->log_has_error ? "A warning or error was recorded this session" : "No errors recorded this session");
}

void ui_view_menu_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    (void)snapshot;
    const ui_controls *controls = app_ui_controls();
    ui_text(ui, 8, 43, 0.46f, ui->theme.text, "Menu navigation");
    ui_panel(ui, 8, 70, 304, 72);
    ui_textf(ui, 18, 83, 0.40f, ui->theme.text, "%s / %s  Select item",
             app_ui_control_label(controls->previous_item), app_ui_control_label(controls->next_item));
    ui_textf(ui, 18, 111, 0.40f, ui->theme.text, "%s  Open selected view",
             app_ui_control_label(controls->activate));
    ui_text(ui, 8, 218, 0.34f, ui->theme.muted, "Touch the main tabs to jump directly");
}

void ui_view_settings_top(ui_context *ui, const ui_snapshot *snapshot) {
    (void)snapshot;
    const ui_controls *controls = app_ui_controls();
    ui_header(ui, "Settings", "Build-time defaults with SD overrides");
    ui_panel(ui, 8, 58, 188, 164);
    ui_text(ui, 18, 69, 0.37f, ui->theme.muted, "RUNTIME ACTIONS");
    ui_textf(ui, 18, 94, 0.40f, ui->theme.text, "%s  Publish once", app_ui_control_label(controls->publish_once));
    ui_textf(ui, 18, 118, 0.40f, ui->theme.text, "%s  Auto publish", app_ui_control_label(controls->toggle_publishing));
    ui_textf(ui, 18, 142, 0.40f, ui->theme.text, "%s  Toggle listener", app_ui_control_label(controls->toggle_listener));
    ui_textf(ui, 18, 166, 0.40f, ui->theme.text, "%s  Send probe", app_ui_control_label(controls->send_probe));
    ui_textf(ui, 18, 190, 0.40f, ui->theme.text, "%s  Exit", app_ui_control_label(controls->exit));

    ui_panel(ui, 204, 58, 188, 164);
    ui_text(ui, 214, 69, 0.37f, ui->theme.muted, "NAVIGATION");
    ui_textf(ui, 214, 94, 0.40f, ui->theme.text, "%s / %s  Tabs",
             app_ui_control_label(controls->previous_view), app_ui_control_label(controls->next_view));
    ui_textf(ui, 214, 124, 0.40f, ui->theme.text, "%s / %s  Items",
             app_ui_control_label(controls->previous_item), app_ui_control_label(controls->next_item));
    ui_textf(ui, 214, 154, 0.40f, ui->theme.text, "%s  Activate", app_ui_control_label(controls->activate));
    ui_textf(ui, 214, 184, 0.40f, ui->theme.text, "%s  Back", app_ui_control_label(controls->back));
}

void ui_view_settings_bottom(ui_context *ui, const ui_snapshot *snapshot) {
    (void)snapshot;
    ui_text(ui, 8, 42, 0.46f, ui->theme.text, "Configuration files");
    ui_panel(ui, 8, 66, 304, 102);
    ui_text(ui, 18, 79, 0.34f, ui->theme.muted, "Theme");
    ui_text(ui, 18, 99, 0.35f, ui->theme.text, "romfs:/ui/theme.ini");
    ui_text(ui, 18, 121, 0.34f, ui->theme.muted, "Controls");
    ui_text(ui, 18, 141, 0.35f, ui->theme.text, "romfs:/ui/controls.ini");
    ui_text(ui, 8, 185, 0.31f, ui->theme.muted, "SD overrides load from /3ds/ros2_3ds_interface/");
    ui_text(ui, 8, 218, 0.34f, ui->theme.muted, "B returns to Menu");
}