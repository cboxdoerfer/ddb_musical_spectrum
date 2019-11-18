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

int CALCULATED_NUM_BARS = 136;

int
get_num_bars ()
{
    return spectrum_notes_size;

    if (CONFIG_DRAW_STYLE == 1 || CONFIG_BAR_W > 0) {
        return CALCULATED_NUM_BARS;
    }

    return CONFIG_NUM_BARS;
}

void
window_table_fill (double *window)
{
    switch (CONFIG_WINDOW) {
        case BLACKMAN_HARRIS:
            for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
                // Blackman-Harris
                window[i] = 0.35875 - 0.48829 * cos(2 * M_PI * i / CONFIG_FFT_SIZE) + 0.14128 * cos(4 * M_PI * i / CONFIG_FFT_SIZE) - 0.01168 * cos(6 * M_PI * i / CONFIG_FFT_SIZE);
            }
            break;
        case HANNING:
            for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
                // Hanning
                window[i] = (0.5 * (1 - cos (2 * M_PI * i / CONFIG_FFT_SIZE)));
            }
            break;
        default:
            break;
    }
}

void
update_num_bars (int width)
{
    CALCULATED_NUM_BARS = 136;
    if (CONFIG_DRAW_STYLE == 1) {
        CALCULATED_NUM_BARS = CLAMP (width, 1, MAX_BARS);
    }
    else if (CONFIG_BAR_W > 0) {
        int added_bar_w = CONFIG_BAR_W;
        if (CONFIG_GAPS)
            added_bar_w += 1;
        CALCULATED_NUM_BARS = CLAMP (width/added_bar_w, 1, MAX_BARS);
    }
}

void
update_gravity (struct spectrum_render_t *render)
{
    render->peak_delay = (int)ceil (CONFIG_PEAK_DELAY/CONFIG_REFRESH_INTERVAL);
    render->bar_delay = (int)ceil (CONFIG_BAR_DELAY/CONFIG_REFRESH_INTERVAL);

    const double peak_gravity = CONFIG_PEAK_FALLOFF/(1000.0 * 1000.0);
    render->peak_velocity = peak_gravity * CONFIG_REFRESH_INTERVAL;

    const double bars_gravity = CONFIG_BAR_FALLOFF/(1000.0 * 1000.0);
    render->bar_velocity = bars_gravity * CONFIG_REFRESH_INTERVAL;
}

void
create_frequency_table (struct spectrum_data_t *s, int samplerate, int width)
{
    s->low_res_end = 0;

    update_num_bars (width);
    //const int num_bars = get_num_bars ();
    const int num_bars = get_num_bars ();
    const double ratio = num_bars / (double)spectrum_notes_size;
    const double a4pos = (57.0 + CONFIG_TRANSPOSE) * ratio;
    const double octave = 12.0 * ratio;

    for (int i = 0; i < spectrum_notes_size; i++) {
        s->frequency[i] = (double)CONFIG_PITCH * pow (2.0, (double)(i-a4pos)/octave);
        s->keys[i] = (int)floor (s->frequency[i] * CONFIG_FFT_SIZE/(double)samplerate);
        if (i > 0 && s->keys[i-1] == s->keys[i])
            s->low_res_end = i;
    }

    int last_key = 0;
    s->low_res_indices_num = 1;
    s->low_res_indices[0] = 0;
    for (int i = 0, j = 1; i < spectrum_notes_size; i++) {
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

