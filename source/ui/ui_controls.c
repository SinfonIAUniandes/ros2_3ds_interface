#include "ui_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    u32 mask;
} control_name;

static const control_name control_names[] = {
    { "A", KEY_A }, { "B", KEY_B }, { "X", KEY_X }, { "Y", KEY_Y },
    { "L", KEY_L }, { "R", KEY_R }, { "UP", KEY_DUP }, { "DOWN", KEY_DDOWN },
    { "LEFT", KEY_DLEFT }, { "RIGHT", KEY_DRIGHT }, { "START", KEY_START },
    { "SELECT", KEY_SELECT }, { "ZL", KEY_ZL }, { "ZR", KEY_ZR }
};

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return text;
}

static u32 parse_control(const char *value) {
    for (size_t index = 0; index < sizeof(control_names) / sizeof(control_names[0]); index++) {
        if (strcmp(value, control_names[index].name) == 0) return control_names[index].mask;
    }
    return 0;
}

void ui_controls_defaults(ui_controls *controls) {
    controls->publish_once = KEY_A;
    controls->toggle_publishing = KEY_B;
    controls->toggle_listener = KEY_Y;
    controls->send_probe = KEY_X;
    controls->exit = KEY_START;
    controls->next_view = KEY_R;
    controls->previous_view = KEY_L;
    controls->next_item = KEY_DDOWN;
    controls->previous_item = KEY_DUP;
    controls->activate = KEY_A;
    controls->back = KEY_B;
}

bool ui_controls_load(ui_controls *controls, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    char line[96];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#' || *entry == ';') continue;
        char *separator = strchr(entry, '=');
        if (separator == NULL) continue;
        *separator = '\0';
        char *key = trim(entry);
        u32 mask = parse_control(trim(separator + 1));
        if (mask == 0) continue;
        if (strcmp(key, "publish_once") == 0) controls->publish_once = mask;
        else if (strcmp(key, "toggle_publishing") == 0) controls->toggle_publishing = mask;
        else if (strcmp(key, "toggle_listener") == 0) controls->toggle_listener = mask;
        else if (strcmp(key, "send_probe") == 0) controls->send_probe = mask;
        else if (strcmp(key, "exit") == 0) controls->exit = mask;
        else if (strcmp(key, "next_view") == 0) controls->next_view = mask;
        else if (strcmp(key, "previous_view") == 0) controls->previous_view = mask;
        else if (strcmp(key, "next_item") == 0) controls->next_item = mask;
        else if (strcmp(key, "previous_item") == 0) controls->previous_item = mask;
        else if (strcmp(key, "activate") == 0) controls->activate = mask;
        else if (strcmp(key, "back") == 0) controls->back = mask;
    }
    fclose(file);
    return true;
}

const char *ui_control_label(u32 mask) {
    for (size_t index = 0; index < sizeof(control_names) / sizeof(control_names[0]); index++) {
        if (mask == control_names[index].mask) return control_names[index].name;
    }
    return "-";
}