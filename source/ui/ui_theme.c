#include "ui_theme.h"

#include <citro2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return text;
}

static bool parse_color(const char *value, u32 *color) {
    if (value[0] == '#') {
        value++;
    }
    if (strlen(value) != 6 && strlen(value) != 8) {
        return false;
    }
    char *end = NULL;
    unsigned long rgba = strtoul(value, &end, 16);
    if (end == value || *end != '\0') {
        return false;
    }
    if (strlen(value) == 6) {
        rgba = (rgba << 8) | 0xffu;
    }
    *color = C2D_Color32((rgba >> 24) & 0xffu, (rgba >> 16) & 0xffu,
                         (rgba >> 8) & 0xffu, rgba & 0xffu);
    return true;
}

void ui_theme_defaults(ui_theme *theme) {
    theme->background = C2D_Color32(244, 246, 248, 255);
    theme->surface = C2D_Color32(255, 255, 255, 255);
    theme->header = C2D_Color32(37, 79, 174, 255);
    theme->toolbar = C2D_Color32(51, 103, 214, 255);
    theme->accent = C2D_Color32(30, 136, 229, 255);
    theme->selected = C2D_Color32(232, 240, 254, 255);
    theme->success = C2D_Color32(46, 125, 50, 255);
    theme->warning = C2D_Color32(249, 168, 37, 255);
    theme->danger = C2D_Color32(198, 40, 40, 255);
    theme->text = C2D_Color32(32, 33, 36, 255);
    theme->muted = C2D_Color32(104, 112, 122, 255);
    theme->border = C2D_Color32(217, 222, 228, 255);
    theme->on_color = C2D_Color32(255, 255, 255, 255);
    theme->corner_radius = 4.0f;
    theme->spacing = 8.0f;
}

bool ui_theme_load(ui_theme *theme, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }
    char line[128];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#' || *entry == ';') {
            continue;
        }
        char *separator = strchr(entry, '=');
        if (separator == NULL) {
            continue;
        }
        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        u32 *target = NULL;
        if (strcmp(key, "background") == 0) target = &theme->background;
        else if (strcmp(key, "surface") == 0) target = &theme->surface;
        else if (strcmp(key, "header") == 0) target = &theme->header;
        else if (strcmp(key, "toolbar") == 0) target = &theme->toolbar;
        else if (strcmp(key, "accent") == 0) target = &theme->accent;
        else if (strcmp(key, "selected") == 0) target = &theme->selected;
        else if (strcmp(key, "success") == 0) target = &theme->success;
        else if (strcmp(key, "warning") == 0) target = &theme->warning;
        else if (strcmp(key, "danger") == 0) target = &theme->danger;
        else if (strcmp(key, "text") == 0) target = &theme->text;
        else if (strcmp(key, "muted") == 0) target = &theme->muted;
        else if (strcmp(key, "border") == 0) target = &theme->border;
        else if (strcmp(key, "on_color") == 0) target = &theme->on_color;
        if (target != NULL) {
            parse_color(value, target);
        } else if (strcmp(key, "corner_radius") == 0) {
            theme->corner_radius = strtof(value, NULL);
        } else if (strcmp(key, "spacing") == 0) {
            theme->spacing = strtof(value, NULL);
        }
    }
    fclose(file);
    return true;
}