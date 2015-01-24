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

#ifndef SPECTRUM_HEADER
#define SPECTRUM_HEADER

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

typedef struct {
    ddb_gtkui_widget_t base;
    GtkWidget *drawarea;
    GtkWidget *popup;
    GtkWidget *popup_item;
    cairo_surface_t *surf;
    unsigned char *surf_data;
    guint drawtimer;
    // spectrum_data: holds amplitude of frequency bins (result of fft)
    double *spectrum_data;
    double window[MAX_FFT_SIZE];
    // keys: index of frequencies of musical notes (c0;d0;...;f10) in data
    int keys[MAX_BARS + 1];
    // freq: hold frequency values
    float freq[MAX_BARS + 1];
    uint32_t colors[GRADIENT_TABLE_SIZE];
    int samplerate;
    double *samples;
    double *fft_in;
    fftw_complex *fft_out;
    fftw_plan p_r2c;
    int buffered;
    int low_res_end;
    float bars[MAX_BARS + 1];
    float peaks[MAX_BARS + 1];
    int delay_bars[MAX_BARS + 1];
    int delay_peaks[MAX_BARS + 1];
    intptr_t mutex;
} w_spectrum_t;

#endif
