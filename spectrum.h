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

#include <gtk/gtk.h>
#include <stdint.h>
#include <fftw3.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#define MAX_BARS 2000
#define REFRESH_INTERVAL 25
#define GRADIENT_TABLE_SIZE 1024
#define MAX_FFT_SIZE 32768

/* Global variables */
extern DB_misc_t plugin;
extern DB_functions_t *deadbeef;
extern ddb_gtkui_t *gtkui_plugin;

enum PLAYBACK_STATUS { STOPPED = 0, PLAYING = 1, PAUSED = 2 };

struct motion_context {
    uint8_t entered;
    double x;
    double y;
};

struct spectrum_data_t {
    int num_channels;
    int num_samples;
    uint32_t channel_mask;
    float *samples;
    double *spectrum;
    double *window;
    double *frequency;
    int *keys;
    int *low_res_indices;

    int low_res_end;
    int low_res_indices_num;

    double *fft_in;
    fftw_complex *fft_out;
    fftw_plan fft_plan;

    intptr_t mutex;
};

typedef struct {
    ddb_gtkui_widget_t base;
    GtkWidget *drawarea;
    GtkWidget *popup;
    GtkWidget *popup_item;
    guint drawtimer;
    int samplerate;
    int need_redraw;
    int prev_width;
    int prev_height;
    enum PLAYBACK_STATUS playback_status;

    cairo_rectangle_t spectrum_rectangle;

    struct spectrum_data_t *data;
    struct spectrum_render_t *render;
    struct motion_context motion_ctx;
} w_spectrum_t;

extern char *spectrum_notes[];
extern size_t spectrum_notes_size;

