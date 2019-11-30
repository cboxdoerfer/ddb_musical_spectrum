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

#define     CONFSTR_MS_REFRESH_INTERVAL       "musical_spectrum.refresh_interval"
#define     CONFSTR_MS_FFT_SIZE               "musical_spectrum.fft_size"
#define     CONFSTR_MS_TRANSPOSE              "musical_spectrum.transpose"
#define     CONFSTR_MS_PITCH                  "musical_spectrum.pitch"
#define     CONFSTR_MS_NOTE_MIN               "musical_spectrum.note_min"
#define     CONFSTR_MS_NOTE_MAX               "musical_spectrum.note_max"
#define     CONFSTR_MS_AMPLITUDE_MIN          "musical_spectrum.amplitude_min"
#define     CONFSTR_MS_AMPLITUDE_MAX          "musical_spectrum.amplitude_max"
#define     CONFSTR_MS_ENABLE_TOP_LABELS      "musical_spectrum.enable_top_labels"
#define     CONFSTR_MS_ENABLE_BOTTOM_LABELS   "musical_spectrum.enable_bottom_labels"
#define     CONFSTR_MS_ENABLE_LEFT_LABELS     "musical_spectrum.enable_left_labels"
#define     CONFSTR_MS_ENABLE_RIGHT_LABELS    "musical_spectrum.enable_right_labels"
#define     CONFSTR_MS_ENABLE_PEAKS           "musical_spectrum.enable_peaks"
#define     CONFSTR_MS_ENABLE_PEAKS_COLOR     "musical_spectrum.enable_peaks_color"
#define     CONFSTR_MS_ENABLE_AMPLITUDES      "musical_spectrum.enable_amplitudes"
#define     CONFSTR_MS_ENABLE_HGRID           "musical_spectrum.enable_hgrid"
#define     CONFSTR_MS_ENABLE_VGRID           "musical_spectrum.enable_vgrid"
#define     CONFSTR_MS_ENABLE_OGRID           "musical_spectrum.enable_octave_grid"
#define     CONFSTR_MS_ENABLE_WHITE_KEYS      "musical_spectrum.enable_white_keys"
#define     CONFSTR_MS_ENABLE_BLACK_KEYS      "musical_spectrum.enable_black_keys"
#define     CONFSTR_MS_ENABLE_TOOLTIP         "musical_spectrum.enable_tooltip"
#define     CONFSTR_MS_ENABLE_BAR_MODE        "musical_spectrum.enable_bar_mode"
#define     CONFSTR_MS_DISPLAY_OCTAVES        "musical_spectrum.display_octaves_on_hover"
#define     CONFSTR_MS_NUM_BARS               "musical_spectrum.num_bars"
#define     CONFSTR_MS_BAR_W                  "musical_spectrum.bar_w"
#define     CONFSTR_MS_GAPS                   "musical_spectrum.gaps"
#define     CONFSTR_MS_SPACING                "musical_spectrum.spacing"
#define     CONFSTR_MS_DRAW_STYLE             "musical_spectrum.draw_style"
#define     CONFSTR_MS_FILL_SPECTRUM          "musical_spectrum.fill_spectrum"
#define     CONFSTR_MS_BAR_FALLOFF            "musical_spectrum.bar_falloff"
#define     CONFSTR_MS_BAR_DELAY              "musical_spectrum.bar_delay"
#define     CONFSTR_MS_PEAK_FALLOFF           "musical_spectrum.peak_falloff"
#define     CONFSTR_MS_PEAK_DELAY             "musical_spectrum.peak_delay"
#define     CONFSTR_MS_GRADIENT_ORIENTATION   "musical_spectrum.gradient_orientation"
#define     CONFSTR_MS_ALIGNMENT              "musical_spectrum.alignment"
#define     CONFSTR_MS_WINDOW                 "musical_spectrum.window"
#define     CONFSTR_MS_COLOR_BG               "musical_spectrum.color.background"
#define     CONFSTR_MS_COLOR_TEXT             "musical_spectrum.color.text"
#define     CONFSTR_MS_COLOR_VGRID            "musical_spectrum.color.vgrid"
#define     CONFSTR_MS_COLOR_HGRID            "musical_spectrum.color.hgrid"
#define     CONFSTR_MS_COLOR_OGRID            "musical_spectrum.color.octave_grid"
#define     CONFSTR_MS_COLOR_WHITE_KEYS       "musical_spectrum.color.white_keys"
#define     CONFSTR_MS_COLOR_BLACK_KEYS       "musical_spectrum.color.black_keys"
#define     CONFSTR_MS_COLOR_PEAKS            "musical_spectrum.color.peaks"
#define     CONFSTR_MS_NUM_COLORS             "musical_spectrum.num_colors"
#define     CONFSTR_MS_FONT                   "musical_spectrum.font.labels"
#define     CONFSTR_MS_COLOR_GRADIENT         "musical_spectrum.color.gradient_"

#define MAX_NUM_COLORS 16
#define NUM_DEFAULT_COLORS 6

extern int CONFIG_REFRESH_INTERVAL;
extern int CONFIG_TRANSPOSE;
extern int CONFIG_PITCH;
extern int CONFIG_NOTE_MIN;
extern int CONFIG_NOTE_MAX;
extern int CONFIG_AMPLITUDE_MIN;
extern int CONFIG_AMPLITUDE_MAX;
extern int CONFIG_ENABLE_PEAKS;
extern int CONFIG_ENABLE_PEAKS_COLOR;
extern int CONFIG_ENABLE_AMPLITUDES;
extern int CONFIG_ENABLE_TOP_LABELS;
extern int CONFIG_ENABLE_BOTTOM_LABELS;
extern int CONFIG_ENABLE_LEFT_LABELS;
extern int CONFIG_ENABLE_RIGHT_LABELS;
extern int CONFIG_ENABLE_HGRID;
extern int CONFIG_ENABLE_VGRID;
extern int CONFIG_ENABLE_OGRID;
extern int CONFIG_ENABLE_WHITE_KEYS;
extern int CONFIG_ENABLE_BLACK_KEYS;
extern int CONFIG_ENABLE_TOOLTIP;
extern int CONFIG_ALIGNMENT;
extern int CONFIG_ENABLE_BAR_MODE;
extern int CONFIG_DISPLAY_OCTAVES;
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
extern int CONFIG_SPACING;
extern int CONFIG_DRAW_STYLE;
extern int CONFIG_FILL_SPECTRUM;
extern const char *CONFIG_FONT;
extern GdkColor CONFIG_COLOR_BG;
extern GdkColor CONFIG_COLOR_TEXT;
extern GdkColor CONFIG_COLOR_VGRID;
extern GdkColor CONFIG_COLOR_HGRID;
extern GdkColor CONFIG_COLOR_OGRID;
extern GdkColor CONFIG_COLOR_BLACK_KEYS;
extern GdkColor CONFIG_COLOR_WHITE_KEYS;
extern GdkColor CONFIG_COLOR_PEAKS;
extern GList *CONFIG_GRADIENT_COLORS;

extern int FFT_INDEX;

enum WINDOW { BLACKMAN_HARRIS = 0, HANNING = 1 };
enum ALIGNMENT { LEFT = 0, RIGHT = 1, CENTER = 2 };
enum STYLE { MUSICAL_STYLE = 0, SOLID_STYLE = 1, BAR_STYLE = 2 };

void
load_config (void);

void
save_config (void);

