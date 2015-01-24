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

#ifndef CONFIG_HEADER
#define CONFIG_HEADER

#define     CONFSTR_MS_REFRESH_INTERVAL       "musical_spectrum.refresh_interval"
#define     CONFSTR_MS_FFT_SIZE               "musical_spectrum.fft_size"
#define     CONFSTR_MS_DB_RANGE               "musical_spectrum.db_range"
#define     CONFSTR_MS_ENABLE_HGRID           "musical_spectrum.enable_hgrid"
#define     CONFSTR_MS_ENABLE_VGRID           "musical_spectrum.enable_vgrid"
#define     CONFSTR_MS_ENABLE_OCTAVE_GRID     "musical_spectrum.enable_octave_grid"
#define     CONFSTR_MS_ENABLE_BAR_MODE        "musical_spectrum.enable_bar_mode"
#define     CONFSTR_MS_NUM_BARS               "musical_spectrum.num_bars"
#define     CONFSTR_MS_BAR_W                  "musical_spectrum.bar_w"
#define     CONFSTR_MS_GAPS                   "musical_spectrum.gaps"
#define     CONFSTR_MS_BAR_FALLOFF            "musical_spectrum.bar_falloff"
#define     CONFSTR_MS_BAR_DELAY              "musical_spectrum.bar_delay"
#define     CONFSTR_MS_PEAK_FALLOFF           "musical_spectrum.peak_falloff"
#define     CONFSTR_MS_PEAK_DELAY             "musical_spectrum.peak_delay"
#define     CONFSTR_MS_GRADIENT_ORIENTATION   "musical_spectrum.gradient_orientation"
#define     CONFSTR_MS_ALIGNMENT              "musical_spectrum.alignment"
#define     CONFSTR_MS_WINDOW                 "musical_spectrum.window"
#define     CONFSTR_MS_COLOR_BG               "musical_spectrum.color.background"
#define     CONFSTR_MS_COLOR_VGRID            "musical_spectrum.color.vgrid"
#define     CONFSTR_MS_COLOR_HGRID            "musical_spectrum.color.hgrid"
#define     CONFSTR_MS_COLOR_OCTAVE_GRID      "musical_spectrum.color.octave_grid"
#define     CONFSTR_MS_NUM_COLORS             "musical_spectrum.num_colors"
#define     CONFSTR_MS_COLOR_GRADIENT         "musical_spectrum.color.gradient_"

#define MAX_NUM_COLORS 16
#define NUM_DEFAULT_COLORS 6

extern int CONFIG_REFRESH_INTERVAL;
extern int CONFIG_DB_RANGE;
extern int CONFIG_ENABLE_HGRID;
extern int CONFIG_ENABLE_VGRID;
extern int CONFIG_ENABLE_OCTAVE_GRID;
extern int CONFIG_ALIGNMENT;
extern int CONFIG_ENABLE_BAR_MODE;
extern int CONFIG_BAR_FALLOFF;
extern int CONFIG_BAR_DELAY;
extern int CONFIG_PEAK_FALLOFF;
extern int CONFIG_PEAK_DELAY;
extern int CONFIG_GRADIENT_ORIENTATION;
extern int CONFIG_NUM_COLORS;
extern int CONFIG_FFT_SIZE;
extern int CONFIG_WINDOW;
extern int CONFIG_NUM_BARS;
extern int CONFIG_BAR_W;
extern int CONFIG_GAPS;
extern GdkColor CONFIG_COLOR_BG;
extern GdkColor CONFIG_COLOR_VGRID;
extern GdkColor CONFIG_COLOR_HGRID;
extern GdkColor CONFIG_COLOR_OCTAVE_GRID;
extern GdkColor CONFIG_GRADIENT_COLORS[];
extern uint32_t CONFIG_COLOR_BG32;
extern uint32_t CONFIG_COLOR_VGRID32;
extern uint32_t CONFIG_COLOR_HGRID32;
extern uint32_t CONFIG_COLOR_OCTAVE_GRID32;

extern int FFT_INDEX;

enum WINDOW { BLACKMAN_HARRIS = 0, HANNING = 1 };
enum ALIGNMENT { LEFT = 0, RIGHT = 1, CENTER = 2 };

void
load_config (void);

void
save_config (void);

#endif
