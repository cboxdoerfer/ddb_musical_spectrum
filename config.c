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

#include <gtk/gtk.h>

#include "spectrum.h"
#include "config.h"

int CONFIG_REFRESH_INTERVAL = 25;
int CONFIG_TRANSPOSE = 0;
int CONFIG_PITCH = 440;
int CONFIG_NOTE_MIN = 35;
int CONFIG_NOTE_MAX = 107;
int CONFIG_AMPLITUDE_MIN = -60;
int CONFIG_AMPLITUDE_MAX = 0;
int CONFIG_ENABLE_PEAKS = 1;
int CONFIG_ENABLE_PEAKS_COLOR = 0;
int CONFIG_ENABLE_AMPLITUDES = 0;
int CONFIG_ENABLE_HGRID = 1;
int CONFIG_ENABLE_BOTTOM_LABELS = 1;
int CONFIG_ENABLE_TOP_LABELS = 0;
int CONFIG_ENABLE_LEFT_LABELS = 0;
int CONFIG_ENABLE_RIGHT_LABELS = 1;
int CONFIG_ENABLE_VGRID = 1;
int CONFIG_ENABLE_TOOLTIP = 1;
int CONFIG_ENABLE_OGRID = 0;
int CONFIG_ENABLE_WHITE_KEYS = 0;
int CONFIG_ENABLE_BLACK_KEYS = 1;
int CONFIG_ALIGNMENT = 0;
int CONFIG_ENABLE_BAR_MODE = 0;
int CONFIG_DISPLAY_OCTAVES = 0;
int CONFIG_BAR_FALLOFF = -1;
int CONFIG_BAR_DELAY = 0;
int CONFIG_PEAK_FALLOFF = 50;
int CONFIG_PEAK_DELAY = 500;
int CONFIG_GRADIENT_ORIENTATION = 0;
int CONFIG_NUM_COLORS = 6;
int CONFIG_FFT_SIZE = 8192;
int CONFIG_WINDOW = 0;
int CONFIG_NUM_BARS = 132;
int CONFIG_BAR_W = 0;
int CONFIG_GAPS = TRUE;
int CONFIG_SPACING = TRUE;
int CONFIG_DRAW_STYLE = 0;
int CONFIG_FILL_SPECTRUM = TRUE;
const char *CONFIG_FONT = NULL;
GdkColor CONFIG_COLOR_BG;
GdkColor CONFIG_COLOR_VGRID;
GdkColor CONFIG_COLOR_HGRID;
GdkColor CONFIG_COLOR_OGRID;
GdkColor CONFIG_COLOR_BLACK_KEYS;
GdkColor CONFIG_COLOR_WHITE_KEYS;
GdkColor CONFIG_COLOR_PEAKS;
GdkColor CONFIG_COLOR_TEXT;
GList *CONFIG_GRADIENT_COLORS = NULL;

int FFT_INDEX = 4;

static char *default_colors[] = {"65535 0 0",
                                 "65535 32896 0",
                                 "65535 65535 0",
                                 "32896 65535 30840",
                                 "0 38036 41120",
                                 "0 8224 25700" };

static void
set_color (const char *name, GdkColor *color)
{
    char color_formated[100] = {};
    snprintf (color_formated, sizeof (color_formated), "%d %d %d", color->red, color->green, color->blue);
    deadbeef->conf_set_str (name, color_formated);
}

void
save_config (void)
{
    deadbeef->conf_set_int (CONFSTR_MS_REFRESH_INTERVAL,            CONFIG_REFRESH_INTERVAL);
    deadbeef->conf_set_int (CONFSTR_MS_FFT_SIZE,                    CONFIG_FFT_SIZE);
    deadbeef->conf_set_int (CONFSTR_MS_TRANSPOSE,                   CONFIG_TRANSPOSE);
    deadbeef->conf_set_int (CONFSTR_MS_PITCH,                       CONFIG_PITCH);
    deadbeef->conf_set_int (CONFSTR_MS_NOTE_MIN,                    CONFIG_NOTE_MIN);
    deadbeef->conf_set_int (CONFSTR_MS_NOTE_MAX,                    CONFIG_NOTE_MAX);
    deadbeef->conf_set_int (CONFSTR_MS_AMPLITUDE_MIN,               CONFIG_AMPLITUDE_MIN);
    deadbeef->conf_set_int (CONFSTR_MS_AMPLITUDE_MAX,               CONFIG_AMPLITUDE_MAX);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_TOP_LABELS,           CONFIG_ENABLE_TOP_LABELS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_BOTTOM_LABELS,        CONFIG_ENABLE_BOTTOM_LABELS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_LEFT_LABELS,          CONFIG_ENABLE_LEFT_LABELS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_RIGHT_LABELS,         CONFIG_ENABLE_RIGHT_LABELS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_PEAKS,                CONFIG_ENABLE_PEAKS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_PEAKS_COLOR,          CONFIG_ENABLE_PEAKS_COLOR);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_AMPLITUDES,           CONFIG_ENABLE_AMPLITUDES);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_HGRID,                CONFIG_ENABLE_HGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_VGRID,                CONFIG_ENABLE_VGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_OGRID,                CONFIG_ENABLE_OGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_WHITE_KEYS,           CONFIG_ENABLE_WHITE_KEYS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_BLACK_KEYS,           CONFIG_ENABLE_BLACK_KEYS);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_TOOLTIP,              CONFIG_ENABLE_TOOLTIP);
    deadbeef->conf_set_int (CONFSTR_MS_ALIGNMENT,                   CONFIG_ALIGNMENT);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_BAR_MODE,             CONFIG_ENABLE_BAR_MODE);
    deadbeef->conf_set_int (CONFSTR_MS_DISPLAY_OCTAVES,             CONFIG_DISPLAY_OCTAVES);
    deadbeef->conf_set_int (CONFSTR_MS_NUM_BARS,                    CONFIG_NUM_BARS);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_W,                       CONFIG_BAR_W);
    deadbeef->conf_set_int (CONFSTR_MS_GAPS,                        CONFIG_GAPS);
    deadbeef->conf_set_int (CONFSTR_MS_SPACING,                     CONFIG_SPACING);
    deadbeef->conf_set_int (CONFSTR_MS_DRAW_STYLE,                  CONFIG_DRAW_STYLE);
    deadbeef->conf_set_int (CONFSTR_MS_FILL_SPECTRUM,               CONFIG_FILL_SPECTRUM);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_FALLOFF,                 CONFIG_BAR_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_DELAY,                   CONFIG_BAR_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_FALLOFF,                CONFIG_PEAK_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_DELAY,                  CONFIG_PEAK_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_GRADIENT_ORIENTATION,        CONFIG_GRADIENT_ORIENTATION);
    deadbeef->conf_set_int (CONFSTR_MS_WINDOW,                      CONFIG_WINDOW);
    deadbeef->conf_set_int (CONFSTR_MS_NUM_COLORS,                  CONFIG_NUM_COLORS);
    deadbeef->conf_set_str (CONFSTR_MS_FONT,                        CONFIG_FONT);
    char color[100];
    char conf_str[100];
    GList *c = CONFIG_GRADIENT_COLORS;
    for (int i = 0; c != NULL; c = c->next, i++) {
        GdkColor *clr = c->data;
        snprintf (color, sizeof (color), "%d %d %d", clr->red, clr->green, clr->blue);
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFSTR_MS_COLOR_GRADIENT, i);
        deadbeef->conf_set_str (conf_str, color);
    }
    set_color (CONFSTR_MS_COLOR_BG,         &CONFIG_COLOR_BG);
    set_color (CONFSTR_MS_COLOR_TEXT,       &CONFIG_COLOR_TEXT);
    set_color (CONFSTR_MS_COLOR_VGRID,      &CONFIG_COLOR_VGRID);
    set_color (CONFSTR_MS_COLOR_HGRID,      &CONFIG_COLOR_HGRID);
    set_color (CONFSTR_MS_COLOR_OGRID,      &CONFIG_COLOR_OGRID);
    set_color (CONFSTR_MS_COLOR_BLACK_KEYS, &CONFIG_COLOR_BLACK_KEYS);
    set_color (CONFSTR_MS_COLOR_WHITE_KEYS, &CONFIG_COLOR_WHITE_KEYS);
    set_color (CONFSTR_MS_COLOR_PEAKS,      &CONFIG_COLOR_PEAKS);
}

static void
get_color (const char *name, GdkColor *color, const char *color_default_formated)
{
    const char *color_formated = deadbeef->conf_get_str_fast (name, color_default_formated);
    sscanf (color_formated, "%hd %hd %hd", &color->red, &color->green, &color->blue);
}

void
load_config (void)
{
    deadbeef->conf_lock ();
    CONFIG_GRADIENT_ORIENTATION = deadbeef->conf_get_int (CONFSTR_MS_GRADIENT_ORIENTATION,   0);
    CONFIG_WINDOW = deadbeef->conf_get_int (CONFSTR_MS_WINDOW,                 BLACKMAN_HARRIS);
    CONFIG_FFT_SIZE = deadbeef->conf_get_int (CONFSTR_MS_FFT_SIZE,                        8192);
    CONFIG_FFT_SIZE = CLAMP (CONFIG_FFT_SIZE, 512, 32768);
    FFT_INDEX = log2 (CONFIG_FFT_SIZE) - 9;
    CONFIG_TRANSPOSE = deadbeef->conf_get_int (CONFSTR_MS_TRANSPOSE,                         0);
    CONFIG_PITCH = deadbeef->conf_get_int (CONFSTR_MS_PITCH,                               440);
    CONFIG_NOTE_MIN = deadbeef->conf_get_int (CONFSTR_MS_NOTE_MIN,                          35);
    CONFIG_NOTE_MAX = deadbeef->conf_get_int (CONFSTR_MS_NOTE_MAX,                         107);
    CONFIG_AMPLITUDE_MIN = deadbeef->conf_get_int (CONFSTR_MS_AMPLITUDE_MIN,               -60);
    CONFIG_AMPLITUDE_MAX = deadbeef->conf_get_int (CONFSTR_MS_AMPLITUDE_MAX,                 0);
    CONFIG_ENABLE_PEAKS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_PEAKS,                   1);
    CONFIG_ENABLE_PEAKS_COLOR = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_PEAKS_COLOR,       0);
    CONFIG_ENABLE_AMPLITUDES = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_AMPLITUDES,         0);
    CONFIG_ENABLE_TOP_LABELS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_TOP_LABELS,         0);
    CONFIG_ENABLE_BOTTOM_LABELS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_BOTTOM_LABELS,   1);
    CONFIG_ENABLE_LEFT_LABELS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_LEFT_LABELS,       0);
    CONFIG_ENABLE_RIGHT_LABELS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_RIGHT_LABELS,     1);
    CONFIG_ENABLE_HGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_HGRID,                   1);
    CONFIG_ENABLE_VGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_VGRID,                   1);
    CONFIG_ENABLE_OGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_OGRID,                   0);
    CONFIG_ENABLE_WHITE_KEYS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_WHITE_KEYS,         0);
    CONFIG_ENABLE_BLACK_KEYS = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_BLACK_KEYS,         1);
    CONFIG_ENABLE_TOOLTIP = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_TOOLTIP,               1);
    CONFIG_ALIGNMENT = deadbeef->conf_get_int (CONFSTR_MS_ALIGNMENT,                      LEFT);
    CONFIG_ENABLE_BAR_MODE = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_BAR_MODE,             0);
    CONFIG_DISPLAY_OCTAVES = deadbeef->conf_get_int (CONFSTR_MS_DISPLAY_OCTAVES,             0);
    CONFIG_REFRESH_INTERVAL = deadbeef->conf_get_int (CONFSTR_MS_REFRESH_INTERVAL,          25);
    CONFIG_NUM_BARS = deadbeef->conf_get_int (CONFSTR_MS_NUM_BARS,                         132);
    CONFIG_BAR_W = deadbeef->conf_get_int (CONFSTR_MS_BAR_W,                                 0);
    CONFIG_GAPS = deadbeef->conf_get_int (CONFSTR_MS_GAPS,                                TRUE);
    CONFIG_SPACING = deadbeef->conf_get_int (CONFSTR_MS_SPACING,                          TRUE);
    CONFIG_DRAW_STYLE = deadbeef->conf_get_int (CONFSTR_MS_DRAW_STYLE,                       0);
    CONFIG_FILL_SPECTRUM = deadbeef->conf_get_int (CONFSTR_MS_FILL_SPECTRUM,              TRUE);
    CONFIG_BAR_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_BAR_FALLOFF,                    -1);
    CONFIG_BAR_DELAY = deadbeef->conf_get_int (CONFSTR_MS_BAR_DELAY,                         0);
    CONFIG_PEAK_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_PEAK_FALLOFF,                  90);
    CONFIG_PEAK_DELAY = deadbeef->conf_get_int (CONFSTR_MS_PEAK_DELAY,                     500);
    CONFIG_NUM_COLORS = deadbeef->conf_get_int (CONFSTR_MS_NUM_COLORS,                       6);
    CONFIG_FONT = deadbeef->conf_get_str_fast (CONFSTR_MS_FONT,                       "Sans 7");

    get_color (CONFSTR_MS_COLOR_BG,         &CONFIG_COLOR_BG,         "8738 8738 8738");
    get_color (CONFSTR_MS_COLOR_TEXT,       &CONFIG_COLOR_TEXT,       "65535 65535 65535");
    get_color (CONFSTR_MS_COLOR_VGRID,      &CONFIG_COLOR_VGRID,      "0 0 0");
    get_color (CONFSTR_MS_COLOR_HGRID,      &CONFIG_COLOR_HGRID,      "26214 26214 26214");
    get_color (CONFSTR_MS_COLOR_OGRID,      &CONFIG_COLOR_OGRID,      "26214 26214 26214");
    get_color (CONFSTR_MS_COLOR_BLACK_KEYS, &CONFIG_COLOR_BLACK_KEYS, "8738 8738 8738");
    get_color (CONFSTR_MS_COLOR_WHITE_KEYS, &CONFIG_COLOR_WHITE_KEYS, "8738 8738 8738");
    get_color (CONFSTR_MS_COLOR_PEAKS,      &CONFIG_COLOR_PEAKS,      "65535 0 0");

    const char *color = NULL;
    char conf_str[100] = {};
    g_list_free_full (CONFIG_GRADIENT_COLORS, g_free);
    CONFIG_GRADIENT_COLORS = NULL;
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFSTR_MS_COLOR_GRADIENT, i);
        if (i < NUM_DEFAULT_COLORS) {
            color = deadbeef->conf_get_str_fast (conf_str, default_colors[i]);
        }
        else {
            color = deadbeef->conf_get_str_fast (conf_str, "0 0 0");
        }
        int red, green, blue;
        sscanf (color, "%d %d %d", &red, &green, &blue);
        GdkColor *clr = g_new0 (GdkColor, 1);
        clr->red = red;
        clr->green = green;
        clr->blue = blue;
        CONFIG_GRADIENT_COLORS = g_list_append (CONFIG_GRADIENT_COLORS, clr);
    }

    deadbeef->conf_unlock ();
}

