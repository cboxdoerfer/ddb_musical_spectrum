/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2014 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    Based on DeaDBeeFs stock spectrum.
    Copyright (c) 2009-2014 Alexey Yakovenko <waker@users.sourceforge.net>
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

#include "config.h"

int CONFIG_REFRESH_INTERVAL = 25;
int CONFIG_DB_RANGE = 70;
int CONFIG_ENABLE_HGRID = 1;
int CONFIG_ENABLE_VGRID = 1;
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
GdkColor CONFIG_COLOR_BG;
GdkColor CONFIG_COLOR_VGRID;
GdkColor CONFIG_COLOR_HGRID;
GdkColor CONFIG_GRADIENT_COLORS[6];
uint32_t CONFIG_COLOR_BG32 = 0xff222222;
uint32_t CONFIG_COLOR_VGRID32 = 0xff000000;
uint32_t CONFIG_COLOR_HGRID32 = 0xff666666;

void
save_config (void)
{
    deadbeef->conf_set_int (CONFSTR_MS_REFRESH_INTERVAL,            CONFIG_REFRESH_INTERVAL);
    deadbeef->conf_set_int (CONFSTR_MS_FFT_SIZE,                    CONFIG_FFT_SIZE);
    deadbeef->conf_set_int (CONFSTR_MS_DB_RANGE,                    CONFIG_DB_RANGE);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_HGRID,                CONFIG_ENABLE_HGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_VGRID,                CONFIG_ENABLE_VGRID);
    deadbeef->conf_set_int (CONFSTR_MS_ALIGNMENT,                   CONFIG_ALIGNMENT);
    deadbeef->conf_set_int (CONFSTR_MS_ENABLE_BAR_MODE,             CONFIG_ENABLE_BAR_MODE);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_FALLOFF,                 CONFIG_BAR_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_BAR_DELAY,                   CONFIG_BAR_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_FALLOFF,                CONFIG_PEAK_FALLOFF);
    deadbeef->conf_set_int (CONFSTR_MS_PEAK_DELAY,                  CONFIG_PEAK_DELAY);
    deadbeef->conf_set_int (CONFSTR_MS_GRADIENT_ORIENTATION,        CONFIG_GRADIENT_ORIENTATION);
    deadbeef->conf_set_int (CONFSTR_MS_WINDOW,                      CONFIG_WINDOW);
    char color[100];
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_BG.red, CONFIG_COLOR_BG.green, CONFIG_COLOR_BG.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_BG, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_VGRID.red, CONFIG_COLOR_VGRID.green, CONFIG_COLOR_VGRID.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_VGRID, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_HGRID.red, CONFIG_COLOR_HGRID.green, CONFIG_COLOR_HGRID.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_HGRID, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[0].red, CONFIG_GRADIENT_COLORS[0].green, CONFIG_GRADIENT_COLORS[0].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_00, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[1].red, CONFIG_GRADIENT_COLORS[1].green, CONFIG_GRADIENT_COLORS[1].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_01, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[2].red, CONFIG_GRADIENT_COLORS[2].green, CONFIG_GRADIENT_COLORS[2].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_02, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[3].red, CONFIG_GRADIENT_COLORS[3].green, CONFIG_GRADIENT_COLORS[3].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_03, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[4].red, CONFIG_GRADIENT_COLORS[4].green, CONFIG_GRADIENT_COLORS[4].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_04, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[5].red, CONFIG_GRADIENT_COLORS[5].green, CONFIG_GRADIENT_COLORS[5].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_05, color);
}

void
load_config (void)
{
    deadbeef->conf_lock ();
    CONFIG_GRADIENT_ORIENTATION = deadbeef->conf_get_int (CONFSTR_MS_GRADIENT_ORIENTATION,   0);
    CONFIG_WINDOW = deadbeef->conf_get_int (CONFSTR_MS_WINDOW,                 BLACKMAN_HARRIS);
    CONFIG_FFT_SIZE = deadbeef->conf_get_int (CONFSTR_MS_FFT_SIZE,                        8192);
    CONFIG_DB_RANGE = deadbeef->conf_get_int (CONFSTR_MS_DB_RANGE,                          70);
    CONFIG_ENABLE_HGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_HGRID,                   1);
    CONFIG_ENABLE_VGRID = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_VGRID,                   1);
    CONFIG_ALIGNMENT = deadbeef->conf_get_int (CONFSTR_MS_ALIGNMENT,                      LEFT);
    CONFIG_ENABLE_BAR_MODE = deadbeef->conf_get_int (CONFSTR_MS_ENABLE_BAR_MODE,             0);
    CONFIG_REFRESH_INTERVAL = deadbeef->conf_get_int (CONFSTR_MS_REFRESH_INTERVAL,          25);
    CONFIG_BAR_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_BAR_FALLOFF,                    -1);
    CONFIG_BAR_DELAY = deadbeef->conf_get_int (CONFSTR_MS_BAR_DELAY,                         0);
    CONFIG_PEAK_FALLOFF = deadbeef->conf_get_int (CONFSTR_MS_PEAK_FALLOFF,                  90);
    CONFIG_PEAK_DELAY = deadbeef->conf_get_int (CONFSTR_MS_PEAK_DELAY,                     500);
    const char *color;
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_BG,                   "8738 8738 8738");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_BG.red, &CONFIG_COLOR_BG.green, &CONFIG_COLOR_BG.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_VGRID,                         "0 0 0");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_VGRID.red, &CONFIG_COLOR_VGRID.green, &CONFIG_COLOR_VGRID.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_HGRID,             "26214 26214 26214");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_HGRID.red, &CONFIG_COLOR_HGRID.green, &CONFIG_COLOR_HGRID.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_00,        "65535 0 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[0].red), &(CONFIG_GRADIENT_COLORS[0].green), &(CONFIG_GRADIENT_COLORS[0].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_01,      "65535 32896 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[1].red), &(CONFIG_GRADIENT_COLORS[1].green), &(CONFIG_GRADIENT_COLORS[1].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_02,      "65535 65535 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[2].red), &(CONFIG_GRADIENT_COLORS[2].green), &(CONFIG_GRADIENT_COLORS[2].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_03,    "32896 65535 30840");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[3].red), &(CONFIG_GRADIENT_COLORS[3].green), &(CONFIG_GRADIENT_COLORS[3].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_04,      "0 38036 41120");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[4].red), &(CONFIG_GRADIENT_COLORS[4].green), &(CONFIG_GRADIENT_COLORS[4].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_05,       "0 8224 25700");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[5].red), &(CONFIG_GRADIENT_COLORS[5].green), &(CONFIG_GRADIENT_COLORS[5].blue));

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
    deadbeef->conf_unlock ();
}

