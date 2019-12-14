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

#pragma once

#include <stdint.h>
#include <gdk/gdk.h>

enum spectrum_window { BLACKMAN_HARRIS_WINDOW = 0, HANNING_WINDOW = 1, NO_WINDOW = 2, NUM_WINDOW };
enum spectrum_alignment { LEFT_ALIGN = 0, RIGHT_ALIGN = 1, CENTER_ALIGN = 2 , NUM_ALIGNMENT};
enum spectrum_style { MUSICAL_STYLE = 0, SOLID_STYLE = 1, BAR_STYLE = 2 , NUM_STYLE};

enum spectrum_config_int_index {
    IDX_REFRESH_INTERVAL,
    IDX_INTERPOLATE,
    IDX_CHANNEL,
    IDX_TRANSPOSE,
    IDX_PITCH,
    IDX_NOTE_MIN,
    IDX_NOTE_MAX,
    IDX_AMPLITUDE_MIN,
    IDX_AMPLITUDE_MAX,
    IDX_ENABLE_PEAKS,
    IDX_ENABLE_PEAKS_COLOR,
    IDX_ENABLE_AMPLITUDES,
    IDX_ENABLE_TOP_LABELS,
    IDX_ENABLE_BOTTOM_LABELS,
    IDX_ENABLE_LEFT_LABELS,
    IDX_ENABLE_RIGHT_LABELS,
    IDX_ENABLE_HGRID,
    IDX_ENABLE_VGRID,
    IDX_ENABLE_OGRID,
    IDX_ENABLE_WHITE_KEYS,
    IDX_ENABLE_BLACK_KEYS,
    IDX_ENABLE_TOOLTIP,
    IDX_ALIGNMENT,
    IDX_ENABLE_BAR_MODE,
    IDX_BAR_FALLOFF,
    IDX_BAR_DELAY,
    IDX_PEAK_FALLOFF,
    IDX_PEAK_DELAY,
    IDX_GRADIENT_ORIENTATION,
    IDX_NUM_COLORS,
    IDX_FFT_SIZE,
    IDX_WINDOW,
    IDX_NUM_BARS,
    IDX_BAR_W,
    IDX_GAPS,
    IDX_SPACING,
    IDX_DRAW_STYLE,
    IDX_FILL_SPECTRUM,
    NUM_IDX_INT
};

struct spectrum_config_int_t {
    const char *name;
    int val;
    const int val_def;
};

struct spectrum_config_string_t {
    const char *name;
    const char* val;
    const char *val_def;
};
struct spectrum_config_color_t {
    const char *name;
    GdkColor val;
    GdkColor val_def;
};

extern struct spectrum_config_int_t spectrum_config_int[NUM_IDX_INT];


enum spectrum_config_color_index {
    IDX_COLOR_BG,
    IDX_COLOR_TEXT,
    IDX_COLOR_VGRID,
    IDX_COLOR_HGRID,
    IDX_COLOR_OGRID,
    IDX_COLOR_BLACK_KEYS,
    IDX_COLOR_WHITE_KEYS,
    IDX_COLOR_PEAKS,
    NUM_IDX_COLOR
};

extern struct spectrum_config_color_t spectrum_config_color[NUM_IDX_COLOR];

enum spectrum_config_string_index {
    IDX_STRING_FONT,
    NUM_IDX_STRING
};

extern struct spectrum_config_string_t spectrum_config_string[NUM_IDX_STRING];

extern GList *CONFIG_GRADIENT_COLORS;

void
load_config (void);

void
save_config (void);

int
config_get_int (const int index);

const char *
config_get_string (const int index);

GdkColor *
config_get_color (const int index);

void
config_set_int (const int val, const int index);

void
config_set_string (const char *val, const int index);

void
config_set_color (const GdkColor *color, const int index);

