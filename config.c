/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2019 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <inttypes.h>

#include <gtk/gtk.h>

#include "spectrum.h"
#include "config.h"

#define CONFIG_PREFIX "musical_spectrum"

struct spectrum_config_int_t spectrum_config_int[NUM_ID_INT] = {
    [ID_REFRESH_INTERVAL] =     {"refresh_interval",     0, 25},
    [ID_INTERPOLATE] =          {"interpolate",          0, 1},
    [ID_CHANNEL] =              {"channel",              0, 262143},
    [ID_TRANSPOSE] =            {"transpose",            0, 0},
    [ID_PITCH] =                {"pitch",                0, 440},
    [ID_NOTE_MIN] =             {"note_min",             0, 0},
    [ID_NOTE_MAX] =             {"note_max",             0, 125},
    [ID_AMPLITUDE_MIN] =        {"amp_min",              0, -60},
    [ID_AMPLITUDE_MAX] =        {"amp_max",              0, 0},
    [ID_ENABLE_PEAKS] =         {"enable_peaks",         0, TRUE},
    [ID_ENABLE_PEAKS_COLOR] =   {"enable_peaks_color",   0, FALSE},
    [ID_ENABLE_AMPLITUDES] =    {"enable_amp",           0, FALSE},
    [ID_ENABLE_TOP_LABELS] =    {"enable_top_labels",    0, FALSE},
    [ID_ENABLE_BOTTOM_LABELS] = {"enable_bottom_labels", 0, TRUE},
    [ID_ENABLE_LEFT_LABELS] =   {"enable_left_labels",   0, TRUE},
    [ID_ENABLE_RIGHT_LABELS] =  {"enable_right_labels",  0, TRUE},
    [ID_ENABLE_HGRID] =         {"enable_hgrid",         0, TRUE},
    [ID_ENABLE_VGRID] =         {"enable_vgrid",         0, TRUE},
    [ID_ENABLE_OGRID] =         {"enable_ogrid",         0, FALSE},
    [ID_ENABLE_WHITE_KEYS] =    {"enable_white_keys",    0, TRUE},
    [ID_ENABLE_BLACK_KEYS] =    {"enable_black_keys",    0, FALSE},
    [ID_ENABLE_TOOLTIP] =       {"enable_tooltip",       0, TRUE},
    [ID_ALIGNMENT] =            {"alignment",            0, CENTER_ALIGN},
    [ID_ENABLE_BAR_MODE] =      {"enable_bar_mode",      0, FALSE},
    [ID_BAR_FALLOFF] =          {"bar_falloff",          0, 100},
    [ID_BAR_DELAY] =            {"bar_delay",            0, 100},
    [ID_PEAK_FALLOFF] =         {"peak_falloff",         0, 50},
    [ID_PEAK_DELAY] =           {"peak_delay",           0, 500},
    [ID_GRADIENT_ORIENTATION] = {"gradient_orientation", 0, 0},
    [ID_NUM_COLORS] =           {"num_colors",           0, 6},
    [ID_FFT_SIZE] =             {"fft_size",             0, 8192},
    [ID_WINDOW] =               {"window",               0, HANNING_WINDOW},
    [ID_BAR_W] =                {"bar_w",                0, 0},
    [ID_GAPS] =                 {"gaps",                 0, 1},
    [ID_SPACING] =              {"spacing",              0, 1},
    [ID_DRAW_STYLE] =           {"draw_style",           0, MUSICAL_STYLE},
    [ID_FILL_SPECTRUM] =        {"fill_spectrum",        0, 1},
};

struct spectrum_config_color_t spectrum_config_color[NUM_ID_COLOR] = {
    [ID_COLOR_BG] =         { "background", {0}, {.red = 0,     .green = 0,     .blue = 0}},
    [ID_COLOR_TEXT] =       { "text",       {0}, {.red = 65535, .green = 65535, .blue = 65535}},
    [ID_COLOR_VGRID] =      { "vgrid",      {0}, {.red = 21845, .green = 21845, .blue = 21845}},
    [ID_COLOR_HGRID] =      { "hgrid",      {0}, {.red = 21845, .green = 21845, .blue = 21845}},
    [ID_COLOR_OGRID] =      { "ogrid",      {0}, {.red = 42148, .green = 0,     .blue = 0}},
    [ID_COLOR_BLACK_KEYS] = { "black_keys", {0}, {.red = 0,     .green = 0,     .blue = 0}},
    [ID_COLOR_WHITE_KEYS] = { "white_keys", {0}, {.red = 10240, .green = 10240, .blue = 10240}},
    [ID_COLOR_PEAKS] =      { "peaks",      {0}, {.red = 42148, .green = 0,     .blue = 0}},
};

struct spectrum_config_string_t spectrum_config_string[NUM_ID_STRING] = {
    [ID_STRING_FONT] = {"font", NULL, "Sans 7"},
    [ID_STRING_FONT_TOOLTIP] = {"font_tooltip", NULL, "Sans 9"},
};

GList *CONFIG_GRADIENT_COLORS = NULL;

static GdkColor
color_from_string (const char *color_string)
{
    GdkColor color = {};
    sscanf (color_string, "%hd %hd %hd", &color.red, &color.green, &color.blue);
    return color;
}

static void
string_from_color (const GdkColor *color, char *dest, size_t dest_size)
{
    snprintf (dest, dest_size, "%d %d %d", color->red, color->green, color->blue);
}

static void
config_save_int (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".%s", spectrum_config_int[index].name);
    deadbeef->conf_set_int (config_name, spectrum_config_int[index].val);
}

static void
config_save_color (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".color.%s", spectrum_config_color[index].name);

    GdkColor *color = &spectrum_config_color[index].val;
    char color_formated[100] = {};
    string_from_color (color, color_formated, sizeof (color_formated));
    deadbeef->conf_set_str (config_name, color_formated);
}

static void
config_save_string (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".%s", spectrum_config_string[index].name);
    deadbeef->conf_set_str (config_name, spectrum_config_string[index].val);
}

void
save_config (void)
{
    for (int i = 0; i < NUM_ID_INT; i++) {
        config_save_int (i);
    }
    for (int i = 0; i < NUM_ID_STRING; i++) {
        config_save_string (i);
    }
    for (int i = 0; i < NUM_ID_COLOR; i++) {
        config_save_color (i);
    }

    // Gradient colors
    char color[100] = {};
    char conf_str[100] = {};
    GList *c = CONFIG_GRADIENT_COLORS;
    for (int i = 0; c != NULL; c = c->next, i++) {
        GdkColor *clr = c->data;
        snprintf (color, sizeof (color), "%d %d %d", clr->red, clr->green, clr->blue);
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFIG_PREFIX ".color.gradient_", i);
        deadbeef->conf_set_str (conf_str, color);
    }
}

static void
config_load_color (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".color.%s", spectrum_config_color[index].name);

    char color_def_string[100] = {};
    GdkColor clr = spectrum_config_color[index].val_def;
    string_from_color (&clr, color_def_string, sizeof (color_def_string));

    const char *color_string = deadbeef->conf_get_str_fast (config_name, color_def_string);
    spectrum_config_color[index].val = color_from_string (color_string);
}

static void
config_load_int (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".%s", spectrum_config_int[index].name);
    spectrum_config_int[index].val = deadbeef->conf_get_int (config_name, spectrum_config_int[index].val_def);
}

static void
config_load_string (const int index)
{
    char config_name[200] = {};
    snprintf (config_name, sizeof (config_name), CONFIG_PREFIX ".%s", spectrum_config_string[index].name);
    spectrum_config_string[index].val = deadbeef->conf_get_str_fast (config_name, spectrum_config_string[index].val_def);
}

void
load_config (void)
{
    deadbeef->conf_lock ();
    for (int i = 0; i < NUM_ID_INT; i++) {
        config_load_int (i);
    }
    for (int i = 0; i < NUM_ID_STRING; i++) {
        config_load_string (i);
    }
    for (int i = 0; i < NUM_ID_COLOR; i++) {
        config_load_color (i);
    }

    const size_t num_default_colors = 6;
    char *default_colors[] = {
        "65535 0 0", // red
        "65535 32896 0", // orange
        "65535 65535 0", // yellow
        "32896 65535 30840", // green
        "0 38036 41120", // teal
        "0 8224 25700" // blue
    };

    const char *color = NULL;
    char conf_str[100] = {};
    g_list_free_full (CONFIG_GRADIENT_COLORS, g_free);
    CONFIG_GRADIENT_COLORS = NULL;
    for (int i = 0; i < spectrum_config_int[ID_NUM_COLORS].val; i++) {
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFIG_PREFIX ".color.gradient_", i);
        if (i < num_default_colors) {
            color = deadbeef->conf_get_str_fast (conf_str, default_colors[i]);
        }
        else {
            color = deadbeef->conf_get_str_fast (conf_str, "0 0 0");
        }
        GdkColor *clr = g_new0 (GdkColor, 1);
        sscanf (color, "%hd %hd %hd", &clr->red, &clr->green, &clr->blue);
        CONFIG_GRADIENT_COLORS = g_list_append (CONFIG_GRADIENT_COLORS, clr);
    }

    deadbeef->conf_unlock ();
}

void
config_set_int (const int val, const int index)
{
    spectrum_config_int[index].val = val;
}

void
config_set_string (const char *val, const int index)
{
    spectrum_config_string[index].val = val;
}

void
config_set_color (const GdkColor *color, const int index)
{
    spectrum_config_color[index].val.red = color->red;
    spectrum_config_color[index].val.green = color->green;
    spectrum_config_color[index].val.blue = color->blue;
}

int
config_get_int (const int index)
{
    return spectrum_config_int[index].val;
}

const char *
config_get_string (const int index)
{
    return spectrum_config_string[index].val;
}

GdkColor *
config_get_color (const int index)
{
    return &spectrum_config_color[index].val;
}

