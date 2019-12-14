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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include "support.h"
#include "config.h"
#include "spectrum.h"
#include "utils.h"

static int
num_bars_for_width (int width)
{
    int num_bars = 136;
    if (config_get_int (IDX_DRAW_STYLE) == 1) {
        num_bars = CLAMP (width, 1, MAX_BARS);
    }
    else if (config_get_int (IDX_BAR_W) > 0) {
        int added_bar_w = config_get_int (IDX_BAR_W);
        if (config_get_int (IDX_GAPS)) {
            added_bar_w += 1;
        }
        num_bars = CLAMP (width/added_bar_w, 1, MAX_BARS);
    }
    return num_bars;
}

int
get_num_bars (int width)
{
    if (config_get_int (IDX_DRAW_STYLE) == 1 || config_get_int (IDX_BAR_W) > 0) {
        return num_bars_for_width (width);
    }

    return get_num_notes ();
}

void
window_table_fill (double *window)
{
    const int fft_size = config_get_int (IDX_FFT_SIZE);
    switch (config_get_int (IDX_WINDOW)) {
        case BLACKMAN_HARRIS_WINDOW:
            for (int i = 0; i < fft_size; i++) {
                // Blackman-Harris
                window[i] = 0.35875 - 0.48829 * cos(2 * M_PI * i / fft_size) + 0.14128 * cos(4 * M_PI * i / fft_size) - 0.01168 * cos(6 * M_PI * i / fft_size);
                window[i] *= 2.7;
            }
            break;
        case HANNING_WINDOW:
            for (int i = 0; i < fft_size; i++) {
                // Hanning
                window[i] = (0.5 * (1 - cos (2 * M_PI * i / fft_size)));
                window[i] *= 2;
            }
            break;
        case NO_WINDOW:
            for (int i = 0; i < fft_size; i++) {
                window[i] = 1;
            }
            break;
        default:
            break;
    }
}

void
update_gravity (struct spectrum_render_t *render)
{
    const int refresh_interval = config_get_int (IDX_REFRESH_INTERVAL);
    render->peak_delay = (int)ceil (config_get_int (IDX_PEAK_DELAY)/refresh_interval);
    render->bar_delay = (int)ceil (config_get_int (IDX_BAR_DELAY)/refresh_interval);

    const double peak_gravity = config_get_int (IDX_PEAK_FALLOFF)/(1000.0 * 1000.0);
    render->peak_velocity = peak_gravity * refresh_interval;

    const double bars_gravity = config_get_int (IDX_BAR_FALLOFF)/(1000.0 * 1000.0);
    render->bar_velocity = bars_gravity * refresh_interval;
}

int
get_num_notes ()
{
    return config_get_int (IDX_NOTE_MAX) - config_get_int (IDX_NOTE_MIN) + 1;
}

void
create_frequency_table (struct spectrum_data_t *s, int samplerate, int num_bars)
{
    s->low_res_end = 0;

    const double note_size = num_bars / (double)(get_num_notes ());
    const double a4pos = (57.0 + config_get_int (IDX_TRANSPOSE) - config_get_int (IDX_NOTE_MIN)) * note_size;
    const double octave = 12.0 * note_size;
    const double d_freq = config_get_int (IDX_FFT_SIZE)/(double)samplerate;

    for (int i = 0; i < num_bars; i++) {
        s->frequency[i] = (double)config_get_int (IDX_PITCH) * pow (2.0, (double)(i-a4pos)/octave);
        s->keys[i] = (int)floor (s->frequency[i] * d_freq);
        if (i > 0 && s->keys[i] > 0 && s->keys[i-1] == s->keys[i]) {
            s->low_res_end = i;
        }
    }

    int last_key = 0;
    s->low_res_indices_num = 1;
    s->low_res_indices[0] = 0;
    for (int i = 0, j = 1; i <= s->low_res_end + 1; i++) {
        int key = s->keys[i];
        if (key != last_key) {
            s->low_res_indices[j++] = i;
            s->low_res_indices_num++;
        }
        last_key = key;
    }
}

double
hermite_interpolate (double *y,
                     double mu,
                     int start,
                     double tension,
                     double bias)
{
    double m0,m1,mu2,mu3;
    double a0,a1,a2,a3;

    double y0;
    if (start < 0) {
        y0 = y[0] - (y[1] - y[0]);
        start = -1;
    }
    else {
        y0 = y[start];
    }
    double y1 = y[start + 1];
    double y2 = y[start + 2];
    double y3 = y[start + 3];

    mu2 = mu * mu;
    mu3 = mu2 * mu;
    m0  = (y1-y0)*(1+bias)*(1-tension)/2;
    m0 += (y2-y1)*(1-bias)*(1-tension)/2;
    m1  = (y2-y1)*(1+bias)*(1-tension)/2;
    m1 += (y3-y2)*(1-bias)*(1-tension)/2;
    a0 =  2*mu3 - 3*mu2 + 1;
    a1 =    mu3 - 2*mu2 + mu;
    a2 =    mu3 -   mu2;
    a3 = -2*mu3 + 3*mu2;

    return(a0*y1+a1*m0+a2*m1+a3*y2);
}

