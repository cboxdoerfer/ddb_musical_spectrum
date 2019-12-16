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

enum spectrum_window {
    BLACKMAN_HARRIS_WINDOW,
    HANNING_WINDOW,
    NO_WINDOW,
    NUM_WINDOW
};
enum spectrum_alignment {
    LEFT_ALIGN,
    RIGHT_ALIGN,
    CENTER_ALIGN,
    NUM_ALIGNMENT
};

enum spectrum_style {
    MUSICAL_STYLE,
    SOLID_STYLE,
    BAR_STYLE,
    NUM_STYLE
};

// Config Integer
enum spectrum_config_int_index {
    ID_REFRESH_INTERVAL,
    ID_INTERPOLATE,
    ID_CHANNEL,
    ID_TRANSPOSE,
    ID_PITCH,
    ID_NOTE_MIN,
    ID_NOTE_MAX,
    ID_AMPLITUDE_MIN,
    ID_AMPLITUDE_MAX,
    ID_ENABLE_PEAKS,
    ID_ENABLE_PEAKS_COLOR,
    ID_ENABLE_AMPLITUDES,
    ID_ENABLE_TOP_LABELS,
    ID_ENABLE_BOTTOM_LABELS,
    ID_ENABLE_LEFT_LABELS,
    ID_ENABLE_RIGHT_LABELS,
    ID_ENABLE_HGRID,
    ID_ENABLE_VGRID,
    ID_ENABLE_OGRID,
    ID_ENABLE_WHITE_KEYS,
    ID_ENABLE_BLACK_KEYS,
    ID_ENABLE_TOOLTIP,
    ID_ALIGNMENT,
    ID_ENABLE_BAR_MODE,
    ID_BAR_FALLOFF,
    ID_BAR_DELAY,
    ID_PEAK_FALLOFF,
    ID_PEAK_DELAY,
    ID_GRADIENT_ORIENTATION,
    ID_NUM_COLORS,
    ID_FFT_SIZE,
    ID_WINDOW,
    ID_BAR_W,
    ID_GAPS,
    ID_SPACING,
    ID_DRAW_STYLE,
    ID_FILL_SPECTRUM,
    NUM_ID_INT
};

struct spectrum_config_int_t {
    const char *name;
    int val;
    const int val_def;
};

extern struct spectrum_config_int_t spectrum_config_int[NUM_ID_INT];

// Config Colors
enum spectrum_config_color_index {
    ID_COLOR_BG,
    ID_COLOR_TEXT,
    ID_COLOR_VGRID,
    ID_COLOR_HGRID,
    ID_COLOR_OGRID,
    ID_COLOR_BLACK_KEYS,
    ID_COLOR_WHITE_KEYS,
    ID_COLOR_PEAKS,
    NUM_ID_COLOR
};

struct spectrum_config_color_t {
    const char *name;
    GdkColor val;
    const GdkColor val_def;
};

extern struct spectrum_config_color_t spectrum_config_color[NUM_ID_COLOR];

// Config Strings
enum spectrum_config_string_index {
    ID_STRING_FONT,
    ID_STRING_FONT_TOOLTIP,
    NUM_ID_STRING
};

struct spectrum_config_string_t {
    const char *name;
    const char* val;
    const char *val_def;
};

extern struct spectrum_config_string_t spectrum_config_string[NUM_ID_STRING];

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

