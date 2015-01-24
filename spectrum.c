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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <fftw3.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "fastftoi.h"
#include "config.h"
#include "config_dialog.h"
#include "utils.h"
#include "draw_utils.h"
#include "spectrum.h"

DB_functions_t *deadbeef = NULL;
ddb_gtkui_t *gtkui_plugin = NULL;

static char *notes[] = {"C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
                 "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
                 "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
                 "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
                 "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
                 "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
                 "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
                 "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
                 "C8","C#8","D8","D#8","E8","F8","F#8","G8","G#8","A8","A#8","B8",
                 "C9","C#9","D9","D#9","E9","F9","F#9","G9","G#9","A9","A#9","B9",
                 "C10","C#10","D10","D#10","E10","F10","F#10","G10","G#10","A10","A#10","B10"
                };

static int
get_align_pos (int width, int bands, int bar_width)
{
    int left = 0;
    switch (CONFIG_ALIGNMENT) {
        case LEFT:
            left = 0;
            break;
        case RIGHT:
            left = MIN (width, width - (bar_width * bands));
            break;
        case CENTER:
            left = MAX (0, (width - (bar_width * bands))/2);
            break;
        default:
            left = 0;
            break;
    }
    return left;
}

static void
do_fft (w_spectrum_t *w)
{
    if (!w->samples || w->buffered < CONFIG_FFT_SIZE) {
        return;
    }
    deadbeef->mutex_lock (w->mutex);
    double real,imag;

    for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
        w->fft_in[i] = w->samples[i] * w->window[i];
    }

    fftw_execute (w->p_r2c);
    for (int i = 0; i < CONFIG_FFT_SIZE/2; i++)
    {
        real = w->fft_out[i][0];
        imag = w->fft_out[i][1];
        w->spectrum_data[i] = (real*real + imag*imag);
    }
    deadbeef->mutex_unlock (w->mutex);
}

static int need_redraw = 0;

static int
on_config_changed (gpointer user_data, uintptr_t ctx)
{
    need_redraw = 1;
    w_spectrum_t *w = user_data;
    load_config ();
    deadbeef->mutex_lock (w->mutex);
    if (w->p_r2c) {
        fftw_destroy_plan (w->p_r2c);
    }
    create_window_table (w);
    create_frequency_table (w);
    create_gradient_table (w->colors, CONFIG_GRADIENT_COLORS, CONFIG_NUM_COLORS);

    w->p_r2c = fftw_plan_dft_r2c_1d (CLAMP (CONFIG_FFT_SIZE, 512, MAX_FFT_SIZE), w->fft_in, w->fft_out, FFTW_ESTIMATE);
    memset (w->spectrum_data, 0, sizeof (double) * MAX_FFT_SIZE);
    deadbeef->mutex_unlock (w->mutex);
    gtk_widget_queue_draw (w->drawarea);
    return 0;
}

///// spectrum vis
static void
w_spectrum_destroy (ddb_gtkui_widget_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    deadbeef->vis_waveform_unlisten (w);
    if (s->spectrum_data) {
        free (s->spectrum_data);
        s->spectrum_data = NULL;
    }
    if (s->samples) {
        free (s->samples);
        s->samples = NULL;
    }
    if (s->p_r2c) {
        fftw_destroy_plan (s->p_r2c);
    }
    if (s->fft_in) {
        fftw_free (s->fft_in);
        s->fft_in = NULL;
    }
    if (s->fft_out) {
        fftw_free (s->fft_out);
        s->fft_out = NULL;
    }
    if (s->drawtimer) {
        g_source_remove (s->drawtimer);
        s->drawtimer = 0;
    }
    if (s->surf) {
        cairo_surface_destroy (s->surf);
        s->surf = NULL;
    }
    if (s->surf_data) {
        free (s->surf_data);
        s->surf_data = NULL;
    }
    if (s->mutex) {
        deadbeef->mutex_free (s->mutex);
        s->mutex = 0;
    }
}

static gboolean
spectrum_draw_cb (void *data) {
    w_spectrum_t *s = data;
    gtk_widget_queue_draw (s->drawarea);
    return TRUE;
}

static void
spectrum_wavedata_listener (void *ctx, ddb_audio_data_t *data) {
    w_spectrum_t *w = ctx;
    if (!w->samples) {
        return;
    }
    deadbeef->mutex_lock (w->mutex);
    const int nsamples = data->nframes;
    const int sz = MIN (CONFIG_FFT_SIZE, nsamples);
    const int n = CONFIG_FFT_SIZE - sz;
    memmove (w->samples, w->samples + sz, n * sizeof (double));

    int pos = 0;
    for (int i = 0; i < sz && pos < nsamples; i++, pos++ ) {
        w->samples[n+i] = -1000.0;
        for (int j = 0; j < data->fmt->channels; j++) {
            w->samples[n + i] = MAX (w->samples[n + i], data->data[ftoi (pos * data->fmt->channels) + j]);
        }
    }
    deadbeef->mutex_unlock (w->mutex);
    if (w->buffered < CONFIG_FFT_SIZE) {
        w->buffered += sz;
    }
}

static inline float
spectrum_get_value (gpointer user_data, int start, int end)
{
    w_spectrum_t *w = user_data;
    if (start >= end) {
        return w->spectrum_data[end];
    }
    float value = 0.0;
    for (int i = start; i < end; i++) {
        value = MAX (w->spectrum_data[i],value);
    }
    return value;
}

static float
spectrum_interpolate (gpointer user_data, int bands, int index)
{
    w_spectrum_t *w = user_data;
    float x = 0.0;
    if (index <= w->low_res_end+1) {
        const float v1 = 10 * log10 (w->spectrum_data[w->keys[index]]);

        // find index of next value
        int j = 0;
        while (index+j < bands && w->keys[index+j] == w->keys[index]) {
            j++;
        }
        const float v2 = 10 * log10 (w->spectrum_data[w->keys[index+j]]);

        int l = j;
        while (index+l < bands && w->keys[index+l] == w->keys[index+j]) {
            l++;
        }
        const float v3 = 10 * log10 (w->spectrum_data[w->keys[index+l]]);

        int k = 0;
        while ((k+index) >= 0 && w->keys[k+index] == w->keys[index]) {
            j++;
            k--;
        }
        const float v0 = 10 * log10 (w->spectrum_data[w->keys[CLAMP(index+k,0,bands-1)]]);

        //x = linear_interpolate (v1,v2,(1.0/(j-1)) * ((-1 * k) - 1));
        x = lagrange_interpolate (v0,v1,v2,v3,1 + (1.0 / (j - 1)) * ((-1 * k) - 1));
    }
    else {
        int start = 0;
        int end = 0;
        if (index > 0) {
            start = (w->keys[index] - w->keys[index-1])/2 + w->keys[index-1];
            if (start == w->keys[index-1]) start = w->keys[index];
        }
        else {
            start = w->keys[index];
        }
        if (index < bands-1) {
            end = (w->keys[index+1] - w->keys[index])/2 + w->keys[index];
            if (end == w->keys[index+1]) end = w->keys[index];
        }
        else {
            end = w->keys[index];
        }
        x = 10 * log10 (spectrum_get_value (w, start, end));
    }
    return x;
}

static void
draw_static_content (unsigned char *data, int stride, int bands, int width, int height)
{
    if (!data) {
        return;
    }
    memset (data, 0, height * stride);

    int barw;
    if (CONFIG_GAPS || CONFIG_BAR_W > 1)
        barw = CLAMP (width / bands, 2, 20);
    else
        barw = CLAMP (width / bands, 2, 20) - 1;

    const int left = get_align_pos (width, bands, barw);

    //draw background
    _draw_background (data, width, height, CONFIG_COLOR_BG32);
    // draw vertical grid
    if (CONFIG_ENABLE_VGRID && CONFIG_GAPS) {
        int num_lines = MIN (width/barw, bands);
        for (int i = 0; i < num_lines; i++) {
            _draw_vline (data, stride, left + barw * i, 0, height-1, CONFIG_COLOR_VGRID32);
        }
    }

    // draw octave grid
    if (CONFIG_ENABLE_OCTAVE_GRID) {
        int num_lines = MIN (width/barw, bands);
        int spectrum_width = MIN (barw * bands, width);
        float octave_width = CLAMP (((float)spectrum_width / 11), 1, spectrum_width);
        int x = 0;
        for (float i = left; i < spectrum_width - 1 && i < width - 1; i += octave_width) {
            x = ftoi (i) + (CONFIG_GAPS ? (ftoi (i) % barw) : 0);
            _draw_vline (data, stride, x, 0, height-1, CONFIG_COLOR_OCTAVE_GRID32);
        }
    }

    const int hgrid_num = CONFIG_DB_RANGE/10;
    // draw horizontal grid
    if (CONFIG_ENABLE_HGRID && height > 2*hgrid_num && width > 1) {
        for (int i = 1; i < hgrid_num; i++) {
            _draw_hline (data, stride, 0, ftoi (i/(float)hgrid_num * height), width-1, CONFIG_COLOR_HGRID32);
        }
    }
}

static gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;
    if (!w->samples) {
        return FALSE;
    }

    GtkAllocation a;
    gtk_widget_get_allocation (w->drawarea, &a);

    static int last_bar_w = 0;
    if (a.width != last_bar_w) {
        create_frequency_table(w);
    }
    last_bar_w = a.width;

    const int bands = get_num_bars ();
    const int width = a.width;
    const int height = a.height;

    const int output_state = deadbeef->get_output ()->state ();

    if (output_state == OUTPUT_STATE_PLAYING) {
        do_fft (w);

        const float bar_falloff = CONFIG_BAR_FALLOFF/1000.0 * CONFIG_REFRESH_INTERVAL;
        const float peak_falloff = CONFIG_PEAK_FALLOFF/1000.0 * CONFIG_REFRESH_INTERVAL;
        const int bar_delay = ftoi (CONFIG_BAR_DELAY/CONFIG_REFRESH_INTERVAL);
        const int peak_delay = ftoi (CONFIG_PEAK_DELAY/CONFIG_REFRESH_INTERVAL);

        for (int i = 0; i < bands; i++) {
            // interpolate
            float x = spectrum_interpolate (w, bands, i);

            // TODO: get rid of hardcoding
            x += CONFIG_DB_RANGE - 63;
            x = CLAMP (x, 0, CONFIG_DB_RANGE);
            w->bars[i] = CLAMP (w->bars[i], 0, CONFIG_DB_RANGE);
            w->peaks[i] = CLAMP (w->peaks[i], 0, CONFIG_DB_RANGE);

            if (CONFIG_BAR_FALLOFF != -1) {
                if (w->delay_bars[i] < 0) {
                    w->bars[i] -= bar_falloff;
                }
                else {
                    w->delay_bars[i]--;
                }
            }
            else {
                w->bars[i] = 0;
            }
            if (CONFIG_PEAK_FALLOFF != -1) {
                if (w->delay_peaks[i] < 0) {
                    w->peaks[i] -= peak_falloff;
                }
                else {
                    w->delay_peaks[i]--;
                }
            }
            else {
                w->peaks[i] = 0;
            }

            if (x > w->bars[i])
            {
                w->bars[i] = x;
                w->delay_bars[i] = bar_delay;
            }
            if (x > w->peaks[i]) {
                w->peaks[i] = x;
                w->delay_peaks[i] = peak_delay;
            }
            if (w->peaks[i] < w->bars[i]) {
                w->peaks[i] = w->bars[i];
            }
        }
    }
    else if (output_state == OUTPUT_STATE_STOPPED) {
        for (int i = 0; i < bands; i++) {
                w->bars[i] = 0;
                w->delay_bars[i] = 0;
                w->peaks[i] = 0;
                w->delay_peaks[i] = 0;
        }
    }

    // start drawing
    int stride = 0;
    if (!w->surf || !w->surf_data || cairo_image_surface_get_width (w->surf) != a.width || cairo_image_surface_get_height (w->surf) != a.height) {
        need_redraw = 1;
        if (w->surf) {
            cairo_surface_destroy (w->surf);
            w->surf = NULL;
        }
        if (w->surf_data) {
            free (w->surf_data);
            w->surf_data = NULL;
        }
        w->surf = cairo_image_surface_create (CAIRO_FORMAT_RGB24, a.width, a.height);
        stride = cairo_image_surface_get_stride (w->surf);
        w->surf_data = malloc (stride * a.height);
    }
    const float base_s = (height / (float)CONFIG_DB_RANGE);

    cairo_surface_flush (w->surf);

    unsigned char *data = cairo_image_surface_get_data (w->surf);
    if (!data) {
        return FALSE;
    }

    stride = cairo_image_surface_get_stride (w->surf);
    if (need_redraw) {
        draw_static_content (data, stride, bands, a.width, a.height);
        memcpy (w->surf_data, data, stride * a.height);
        need_redraw = 0;
    }
    else {
        memcpy (data, w->surf_data, stride * a.height);
    }

    int barw;
    if (CONFIG_GAPS || CONFIG_BAR_W > 1)
        barw = CLAMP (width / bands, 2, 20);
    else
        barw = CLAMP (width / bands, 2, 20) - 1;

    const int left = get_align_pos (a.width, bands, barw);

    for (gint i = 0; i < bands; i++)
    {
        int x = left + barw * i;
        int y = a.height - ftoi (w->bars[i] * base_s);
        int bw;

        if (CONFIG_GAPS) {
            bw = barw -1;
            x += 1;
        }
        else {
            bw = barw;
        }

        if (x + bw >= a.width) {
            bw = a.width-x-1;
        }
        if (y > 0) {
            if (CONFIG_GRADIENT_ORIENTATION == 0) {
                if (CONFIG_ENABLE_BAR_MODE == 0) {
                    _draw_bar_gradient_v (w->colors, data, stride, x, y, bw, a.height-y, a.height);
                }
                else {
                    _draw_bar_gradient_bar_mode_v (w->colors, data, stride, x, y, bw, a.height-y, a.height);
                }
            }
            else {
                if (CONFIG_ENABLE_BAR_MODE == 0) {
                    _draw_bar_gradient_h (w->colors, data, stride, x, y, bw, a.height-y, a.width);
                }
                else {
                    _draw_bar_gradient_bar_mode_h (w->colors, data, stride, x, y, bw, a.height-y, a.width);
                }
            }
        }
        y = a.height - w->peaks[i] * base_s;
        if (y > 0 && y < a.height-1) {
            if (CONFIG_GRADIENT_ORIENTATION == 0) {
                _draw_bar_gradient_v (w->colors, data, stride, x, y, bw, 1, a.height);
            }
            else {
                _draw_bar_gradient_h (w->colors, data, stride, x, y, bw, 1, a.width);
            }
        }
    }

    cairo_surface_mark_dirty (w->surf);
    cairo_set_source_surface (cr, w->surf, 0, 0);
    cairo_paint (cr);

    return FALSE;
}

static gboolean
spectrum_set_refresh_interval (gpointer user_data, int interval)
{
    w_spectrum_t *w = user_data;
    if (!w || interval <= 0) {
        return FALSE;
    }
    if (w->drawtimer) {
        g_source_remove (w->drawtimer);
        w->drawtimer = 0;
    }
    w->drawtimer = g_timeout_add (interval, spectrum_draw_cb, w);
    return TRUE;
}

static gboolean
spectrum_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gboolean res = spectrum_draw (widget, cr, user_data);
    cairo_destroy (cr);
    return res;
}


static gboolean
spectrum_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    //w_spectrum_t *w = user_data;
    if (event->button == 3) {
      return TRUE;
    }
    return TRUE;
}

static gboolean
spectrum_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    if (event->button == 3) {
      gtk_menu_popup (GTK_MENU (w->popup), NULL, NULL, NULL, w->drawarea, 0, gtk_get_current_event_time ());
      return TRUE;
    }
    return TRUE;
}

static gboolean
spectrum_motion_notify_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);

    const int num_bars = get_num_bars ();
    int barw;

    if (CONFIG_GAPS)
        barw = CLAMP (a.width / num_bars, 2, 20);
    else
        barw = CLAMP (a.width / num_bars, 2, 20) - 1;

    const int left = get_align_pos (a.width, num_bars, barw);

    if (event->x > left && event->x < left + barw * num_bars) {
        int pos = CLAMP ((int)((event->x-1-left)/barw),0,num_bars-1);
        int npos = ftoi( pos * 132 / num_bars );
        char tooltip_text[20];
        snprintf (tooltip_text, sizeof (tooltip_text), "%5.0f Hz (%s)", w->freq[pos], notes[npos]);
        gtk_widget_set_tooltip_text (widget, tooltip_text);
        return TRUE;
    }
    return FALSE;
}

static int
spectrum_message (ddb_gtkui_widget_t *widget, uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2)
{
    w_spectrum_t *w = (w_spectrum_t *)widget;

    int samplerate_temp = w->samplerate;
    switch (id) {
        case DB_EV_SONGSTARTED:
            w->samplerate = deadbeef->get_output ()->fmt.samplerate;
            if (w->samplerate == 0) w->samplerate = 44100;
            if (samplerate_temp != w->samplerate) {
                create_frequency_table (w);
            }
            spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            break;
        case DB_EV_CONFIGCHANGED:
            on_config_changed (w, ctx);
            if (deadbeef->get_output ()->state () == OUTPUT_STATE_PLAYING) {
                spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            }
            break;
        case DB_EV_PAUSED:
            if (deadbeef->get_output ()->state () == OUTPUT_STATE_PLAYING) {
                spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            }
            else {
                if (w->drawtimer) {
                    g_source_remove (w->drawtimer);
                    w->drawtimer = 0;
                }
            }
            break;
        case DB_EV_STOP:
            if (w->drawtimer) {
                g_source_remove (w->drawtimer);
                w->drawtimer = 0;
            }
            deadbeef->mutex_lock (w->mutex);
            memset (w->spectrum_data, 0, sizeof (double) * MAX_FFT_SIZE);
            memset (w->samples, 0, sizeof (double) * MAX_FFT_SIZE);
            for (int i = 0; i < MAX_BARS; i++) {
                w->bars[i] = 0;
                w->peaks[i] = 0;
            }
            deadbeef->mutex_unlock (w->mutex);
            gtk_widget_queue_draw (w->drawarea);

            break;
    }
    return 0;
}

static void
spectrum_init (w_spectrum_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    load_config ();
    deadbeef->mutex_lock (s->mutex);
    s->samples = malloc (sizeof (double) * MAX_FFT_SIZE);
    memset (s->samples, 0, sizeof (double) * MAX_FFT_SIZE);
    s->spectrum_data = malloc (sizeof (double) * MAX_FFT_SIZE);
    memset (s->spectrum_data, 0, sizeof (double) * MAX_FFT_SIZE);

    s->fft_in = fftw_malloc (sizeof (double) * MAX_FFT_SIZE);
    memset (s->fft_in, 0, sizeof (double) * MAX_FFT_SIZE);
    s->fft_out = fftw_malloc (sizeof (fftw_complex) * MAX_FFT_SIZE);
    memset (s->fft_out, 0, sizeof (double) * MAX_FFT_SIZE);

    s->p_r2c = fftw_plan_dft_r2c_1d (CONFIG_FFT_SIZE, s->fft_in, s->fft_out, FFTW_ESTIMATE);

    s->buffered = 0;
    s->samplerate = deadbeef->get_output ()->fmt.samplerate;
    if (s->samplerate == 0) s->samplerate = 44100;

    create_window_table (s);
    create_frequency_table (s);
    create_gradient_table (s->colors, CONFIG_GRADIENT_COLORS, CONFIG_NUM_COLORS);

    spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
    deadbeef->vis_waveform_listen (w, spectrum_wavedata_listener);
    deadbeef->mutex_unlock (s->mutex);
}

static ddb_gtkui_widget_t *
w_musical_spectrum_create (void) {
    w_spectrum_t *w = malloc (sizeof (w_spectrum_t));
    memset (w, 0, sizeof (w_spectrum_t));

    w->base.widget = gtk_event_box_new ();
    w->base.destroy  = w_spectrum_destroy;
    w->base.message = spectrum_message;
    w->drawarea = gtk_drawing_area_new ();
    w->popup = gtk_menu_new ();
    w->popup_item = gtk_menu_item_new_with_mnemonic ("Configure");
    w->mutex = deadbeef->mutex_create ();

    gtk_container_add (GTK_CONTAINER (w->base.widget), w->drawarea);
    gtk_container_add (GTK_CONTAINER (w->popup), w->popup_item);
    gtk_widget_show (w->drawarea);
    gtk_widget_show (w->popup);
    gtk_widget_show (w->popup_item);

    gtk_widget_add_events (w->drawarea,
            GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK );

#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) w->drawarea, "expose_event", G_CALLBACK (spectrum_expose_event), w);
#else
    g_signal_connect_after ((gpointer) w->drawarea, "draw", G_CALLBACK (spectrum_expose_event), w);
#endif
    g_signal_connect_after ((gpointer) w->drawarea, "button_press_event", G_CALLBACK (spectrum_button_press_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "button_release_event", G_CALLBACK (spectrum_button_release_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "motion_notify_event", G_CALLBACK (spectrum_motion_notify_event), w);
    g_signal_connect_after ((gpointer) w->popup_item, "activate", G_CALLBACK (on_button_config), w);
    gtkui_plugin->w_override_signals (w->base.widget, w);

    spectrum_init (w);
    return (ddb_gtkui_widget_t *)w;
}

static int
musical_spectrum_connect (void)
{
    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);
    if (gtkui_plugin) {
        //trace("using '%s' plugin %d.%d\n", DDB_GTKUI_PLUGIN_ID, gtkui_plugin->gui.plugin.version_major, gtkui_plugin->gui.plugin.version_minor );
        if (gtkui_plugin->gui.plugin.version_major == 2) {
            //printf ("fb api2\n");
            // 0.6+, use the new widget API
            gtkui_plugin->w_reg_widget ("Musical Spectrum", DDB_WF_SINGLE_INSTANCE, w_musical_spectrum_create, "musical_spectrum", NULL);
            return 0;
        }
    }
    return -1;
}

static int
musical_spectrum_start (void)
{
    load_config ();
    return 0;
}

static int
musical_spectrum_stop (void)
{
    save_config ();
    return 0;
}

static int
musical_spectrum_disconnect (void)
{
    gtkui_plugin = NULL;
    return 0;
}

static const char settings_dlg[] =
    "property \"Refresh interval (ms): \"       spinbtn[10,1000,1] "        CONFSTR_MS_REFRESH_INTERVAL         " 25 ;\n"
    "property \"Number of bars: \"              spinbtn[2,2000,1] "         CONFSTR_MS_NUM_BARS                 " 132 ;\n"
    "property \"Bar width (0 - auto): \"        spinbtn[0,10,1] "           CONFSTR_MS_BAR_W                    " 0 ;\n"
    "property \"Gap between bars  \"            checkbox "                  CONFSTR_MS_GAPS                     " 1 ;\n"
    "property \"Bar falloff (dB/s): \"          spinbtn[-1,1000,1] "        CONFSTR_MS_BAR_FALLOFF              " -1 ;\n"
    "property \"Bar delay (ms): \"              spinbtn[0,10000,100] "      CONFSTR_MS_BAR_DELAY                " 0 ;\n"
    "property \"Peak falloff (dB/s): \"         spinbtn[-1,1000,1] "        CONFSTR_MS_PEAK_FALLOFF             " 90 ;\n"
    "property \"Peak delay (ms): \"             spinbtn[0,10000,100] "      CONFSTR_MS_PEAK_DELAY               " 500 ;\n"
;

DB_misc_t plugin = {
    //DB_PLUGIN_SET_API_VERSION
    .plugin.type            = DB_PLUGIN_MISC,
    .plugin.api_vmajor      = 1,
    .plugin.api_vminor      = 5,
    .plugin.version_major   = 0,
    .plugin.version_minor   = 2,
#if GTK_CHECK_VERSION(3,0,0)
    .plugin.id              = "musical_spectrum-gtk3",
#else
    .plugin.id              = "musical_spectrum",
#endif
    .plugin.name            = "Musical Spectrum",
    .plugin.descr           = "Musical Spectrum",
    .plugin.copyright       =
        "Copyright (C) 2015 Christian Boxdörfer <christian.boxdoerfer@posteo.de>\n"
        "\n"
        "Based on DeaDBeeFs stock spectrum.\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website         = "https://github.com/cboxdoerfer/ddb_musical_spectrum",
    .plugin.start           = musical_spectrum_start,
    .plugin.stop            = musical_spectrum_stop,
    .plugin.connect         = musical_spectrum_connect,
    .plugin.disconnect      = musical_spectrum_disconnect,
    .plugin.configdialog    = settings_dlg,
};

#if !GTK_CHECK_VERSION(3,0,0)
DB_plugin_t *
ddb_vis_musical_spectrum_GTK2_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return &plugin.plugin;
}
#else
DB_plugin_t *
ddb_vis_musical_spectrum_GTK3_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return &plugin.plugin;
}
#endif
