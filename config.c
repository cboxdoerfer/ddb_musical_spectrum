/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2015 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    Based on DeaDBeeFs stock spectrum.
    Copyright (c) 2009-2015 Alexey Yakovenko <waker@users.sourceforge.net>
    Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>

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
int CONFIG_DB_RANGE = 70;
int CONFIG_ENABLE_HGRID = 1;
int CONFIG_ENABLE_VGRID = 1;
int CONFIG_ENABLE_OCTAVE_GRID = 0;
int CONFIG_ALIGNMENT = 0;
int CONFIG_ENABLE_BAR_MODE = 0;
int CONFIG_BAR_FALLOFF = -1;
int CONFIG_BAR_DELAY = 0;
int CONFIG_PEAK_FALLOFF = 90;
int CONFIG_PEAK_DELAY = 500;
int CONFIG_GRADIENT_ORIENTATION = 0;
int CONFIG_NUM_COLORS = 6;
int CONFIG_FFT_SIZE = 8192;
int CONFIG_WINDOW = 0;
int CONFIG_NUM_BARS = 132;
int CONFIG_BAR_W = 0;
int CONFIG_GAPS = TRUE;
GdkColor CONFIG_COLOR_BG;
GdkColor CONFIG_COLOR_VGRID;
GdkColor CONFIG_COLOR_HGRID;
GdkColor CONFIG_COLOR_OCTAVE_GRID;
GdkColor CONFIG_GRADIENT_COLORS[MAX_NUM_COLORS];
uint32_t CONFIG_COLOR_BG32 = 0xff222222;
uint32_t CONFIG_COLOR_VGRID32 = 0xff000000;
uint32_t CONFIG_COLOR_HGRID32 = 0xff666666;
uint32_t CONFIG_COLOR_OCTAVE_GRID32 = 0xff666666;

int FFT_INDEX = 4;

static char *default_colors[] = {"65535 0 0",
                                 "65535 32896 0",
                                 "65535 65535 0",
                                 "32896 65535 30840",
                                 "0 38036 41120",
                                 "0 8224 25700" };

void
save_config (void)
{
    deadbeef->conf_set_int (CONFSTR_MS_REFRESH_INTERVAL,            CONFIG_REFRESH_INTERVAL);
    deadbeef->conf_set_int (CONFSTR_MS_FFT_SIZE,                    CONFIG_FFT_SIZE);
    deadbeef->conf_set_int (CONFSTR_MS_DB_RANGE,                    CONFIG_DB_RANGE);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_HGRID,                CONFIG_ENABLE_HGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_VGRID,                CONFIG_ENABLE_VGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_OCTAVE_GRID,          CONFIG_ENABLE_OCTAVE_GRID);
    deadbeef->conf_set_int (CONFSTR_MS_ALIGNMENT,                   CONFIG_ALIGNMENT);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_BAR_MODE,             CONFIG_ENABLE_BAR_MODE);
    deadbeef->conf_set_int (CONFSTR_MS_NUM_BARS,                    CONFIG_NUM_BARS);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_W,                       CONFIG_BAR_W);
    deadbeef->conf_set_int (CONFSTR_MS_GAPS,                        CONFIG_GAPS);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_FALLOFF,                 CONFIG_BAR_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_DELAY,                   CONFIG_BAR_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_FALLOFF,                CONFIG_PEAK_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_DELAY,                  CONFIG_PEAK_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_GRADIENT_ORIENTATION,        CONFIG_GRADIENT_ORIENTATION);
    deadbeef->conf_set_int (CONFSTR_MS_WINDOW,                      CONFIG_WINDOW);
    deadbeef->conf_set_int (CONFSTR_MS_NUM_COLORS,                  CONFIG_NUM_COLORS);
    char color[100];
    char conf_str[100];
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[i].red, CONFIG_GRADIENT_COLORS[i].green, CONFIG_GRADIENT_COLORS[i].blue);
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFSTR_MS_COLOR_GRADIENT, i);
        deadbeef->conf_set_str (conf_str, color);
    }
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_BG.red, CONFIG_COLOR_BG.green, CONFIG_COLOR_BG.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_BG, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_VGRID.red, CONFIG_COLOR_VGRID.green, CONFIG_COLOR_VGRID.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_VGRID, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_HGRID.red, CONFIG_COLOR_HGRID.green, CONFIG_COLOR_HGRID.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_HGRID, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_OCTAVE_GRID.red, CONFIG_COLOR_OCTAVE_GRID.green, CONFIG_COLOR_OCTAVE_GRID.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_OCTAVE_GRID, color);
}

void
load_config (void)
{
    deadbeef->conf_lock ();
    CONFIG_GRADIENT_ORIENTATION = deadbeef->conf_get_int (CONFSTR_MS_GRADIENT_ORIENTATION,   0);
    CONFIG_WINDOW = deadbeef->conf_get_int (CONFSTR_MS_WINDOW,                 BLACKMAN_HARRIS);
    CONFIG_FFT_SIZE = deadbeef->conf_get_int (CONFSTR_MS_FFT_SIZE,                        8192);
    FFT_INDEX = log2 (CONFIG_FFT_SIZE) - 9;
    CONFIG_DB_RANGE = deadbeef->conf_get_int (CONFSTR_MS_DB_RANGE,                          70);
    CONFIG_ENABLE_HGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_HGRID,                   1);
    CONFIG_ENABLE_VGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_VGRID,                   1);
    CONFIG_ENABLE_OCTAVE_GRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_OCTAVE_GRID,       0);
    CONFIG_ALIGNMENT = deadbeef->conf_get_int (CONFSTR_MS_ALIGNMENT,                      LEFT);
    CONFIG_ENABLE_BAR_MODE = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_BAR_MODE,             0);
    CONFIG_REFRESH_INTERVAL = deadbeef->conf_get_int (CONFSTR_MS_REFRESH_INTERVAL,          25);
    CONFIG_NUM_BARS = deadbeef->conf_get_int (CONFSTR_MS_NUM_BARS,                         132);
    CONFIG_BAR_W = deadbeef->conf_get_int (CONFSTR_MS_BAR_W,                                 0);
    CONFIG_GAPS = deadbeef->conf_get_int (CONFSTR_MS_GAPS,                                TRUE);
    CONFIG_BAR_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_BAR_FALLOFF,                    -1);
    CONFIG_BAR_DELAY = deadbeef->conf_get_int (CONFSTR_MS_BAR_DELAY,                         0);
    CONFIG_PEAK_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_PEAK_FALLOFF,                  90);
    CONFIG_PEAK_DELAY = deadbeef->conf_get_int (CONFSTR_MS_PEAK_DELAY,                     500);
    CONFIG_NUM_COLORS = deadbeef->conf_get_int (CONFSTR_MS_NUM_COLORS,                       6);
    const char *color;
    char conf_str[100];
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_BG,                   "8738 8738 8738");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_BG.red, &CONFIG_COLOR_BG.green, &CONFIG_COLOR_BG.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_VGRID,                         "0 0 0");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_VGRID.red, &CONFIG_COLOR_VGRID.green, &CONFIG_COLOR_VGRID.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_HGRID,             "26214 26214 26214");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_HGRID.red, &CONFIG_COLOR_HGRID.green, &CONFIG_COLOR_HGRID.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_OCTAVE_GRID,             "26214 26214 26214");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_OCTAVE_GRID.red, &CONFIG_COLOR_OCTAVE_GRID.green, &CONFIG_COLOR_OCTAVE_GRID.blue);
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        snprintf (conf_str, sizeof (conf_str), "%s%02d", CONFSTR_MS_COLOR_GRADIENT, i);
        if (i < NUM_DEFAULT_COLORS) {
            color = deadbeef->conf_get_str_fast (conf_str, default_colors[i]);
        }
        else {
            color = deadbeef->conf_get_str_fast (conf_str, "0 0 0");
        }
        sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[i].red), &(CONFIG_GRADIENT_COLORS[i].green), &(CONFIG_GRADIENT_COLORS[i].blue));
    }

    float scale = 255/65535.f;
    CONFIG_COLOR_BG32 = ((uint32_t)(CONFIG_COLOR_BG.red * scale) & 0xFF) << 16 |
                        ((uint32_t)(CONFIG_COLOR_BG.green * scale) & 0xFF) << 8 |
                        ((uint32_t)(CONFIG_COLOR_BG.blue * scale) & 0xFF) << 0;

    CONFIG_COLOR_VGRID32 = ((uint32_t)(CONFIG_COLOR_VGRID.red * scale) & 0xFF) << 16 |
                        ((uint32_t)(CONFIG_COLOR_VGRID.green * scale) & 0xFF) << 8 |
                        ((uint32_t)(CONFIG_COLOR_VGRID.blue * scale) & 0xFF) << 0;

    CONFIG_COLOR_HGRID32 = ((uint32_t)(CONFIG_COLOR_HGRID.red * scale) & 0xFF) << 16 |
                        ((uint32_t)(CONFIG_COLOR_HGRID.green * scale) & 0xFF) << 8 |
                        ((uint32_t)(CONFIG_COLOR_HGRID.blue * scale) & 0xFF) << 0;
    CONFIG_COLOR_OCTAVE_GRID32 = ((uint32_t)(CONFIG_COLOR_OCTAVE_GRID.red * scale) & 0xFF) << 16 |
                        ((uint32_t)(CONFIG_COLOR_OCTAVE_GRID.green * scale) & 0xFF) << 8 |
                        ((uint32_t)(CONFIG_COLOR_OCTAVE_GRID.blue * scale) & 0xFF) << 0;
    deadbeef->conf_unlock ();
}

