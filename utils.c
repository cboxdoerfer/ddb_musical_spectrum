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
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include "support.h"
#include "fastftoi.h"
#include "config.h"
#include "spectrum.h"
#include "utils.h"

int CALCULATED_NUM_BARS = 136;

int
get_num_bars ()
{
    int bar_num = CALCULATED_NUM_BARS;
    if (CONFIG_DRAW_STYLE == 1) {
        bar_num = CALCULATED_NUM_BARS;
    }
    else if (CONFIG_BAR_W > 0) {
        bar_num = CALCULATED_NUM_BARS;
    }
    else {
        bar_num = CONFIG_NUM_BARS;
    }
    return bar_num;
}

void
_memset_pattern (char *data, const void* pattern, size_t data_len, size_t pattern_len)
{
    memmove ((char *)data, pattern, pattern_len);
    char *start = (char *)data;
    char *current = (char *)data + pattern_len;
    char *end = start + data_len;
    while(current + pattern_len < end) {
        memmove (current, start, pattern_len);
        current += pattern_len;
        pattern_len *= 2;
    }
    memmove (current, start, end-current);
}

void
create_gradient_table (uint32_t *dest, GdkColor *colors, int num_colors)
{
    if (!dest) {
        return;
    }
    num_colors -= 1;

    for (int i = 0; i < GRADIENT_TABLE_SIZE; i++) {
        double position = (double)i/GRADIENT_TABLE_SIZE;
        /* if position > 1 then we have repetition of colors it maybe useful    */
        if (position > 1.0) {
            if (position - ftoi (position) == 0.0) {
                position = 1.0;
            }
            else {
                position = position - ftoi (position);
            }
        }

        double m= num_colors * position;
        int n=(int)m; // integer of m
        double f=m-n;  // fraction of m

        dest[i] = 0xFF000000;
        float scale = 255/65535.f;
        if (num_colors == 0) {
            dest[i] = (uint32_t)(colors[0].red*scale) << 16 |
                (uint32_t)(colors[0].green*scale) << 8 |
                (uint32_t)(colors[0].blue*scale) << 0;
        }
        else if (n < num_colors) {
            dest[i] = (uint32_t)((colors[n].red*scale) + f * ((colors[n+1].red*scale)-(colors[n].red*scale))) << 16 |
                (uint32_t)((colors[n].green*scale) + f * ((colors[n+1].green*scale)-(colors[n].green*scale))) << 8 |
                (uint32_t)((colors[n].blue*scale) + f * ((colors[n+1].blue*scale)-(colors[n].blue*scale))) << 0;
        }
        else if (n == num_colors) {
            dest[i] = (uint32_t)(colors[n].red*scale) << 16 |
                (uint32_t)(colors[n].green*scale) << 8 |
                (uint32_t)(colors[n].blue*scale) << 0;
        }
        else {
            dest[i] = 0xFFFFFFFF;
        }
    }
}

void
create_window_table (gpointer user_data)
{
    w_spectrum_t *w = user_data;

    switch (CONFIG_WINDOW) {
        case BLACKMAN_HARRIS:
            for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
                // Blackman-Harris
                w->window[i] = 0.35875 - 0.48829 * cos(2 * M_PI * i / CONFIG_FFT_SIZE) + 0.14128 * cos(4 * M_PI * i / CONFIG_FFT_SIZE) - 0.01168 * cos(6 * M_PI * i / CONFIG_FFT_SIZE);
            }
            break;
        case HANNING:
            for (int i = 0; i < CONFIG_FFT_SIZE; i++) {
                // Hanning
                w->window[i] = (0.5 * (1 - cos (2 * M_PI * i / CONFIG_FFT_SIZE)));
            }
            break;
        default:
            break;
    }
}

void
update_num_bars (gpointer user_data)
{
    w_spectrum_t *w = user_data;

    GtkAllocation a;
    gtk_widget_get_allocation (w->drawarea, &a);

    CALCULATED_NUM_BARS = 136;
    if (CONFIG_DRAW_STYLE == 1) {
        CALCULATED_NUM_BARS = CLAMP (a.width, 1, MAX_BARS);
    }
    else if (CONFIG_BAR_W > 0) {
        int added_bar_w = CONFIG_BAR_W;
        if (CONFIG_GAPS)
            added_bar_w += 1;
        CALCULATED_NUM_BARS = CLAMP (a.width/added_bar_w, 1, MAX_BARS);
    }
}

void
create_frequency_table (gpointer user_data)
{
    w_spectrum_t *w = user_data;
    w->low_res_end = 0;

    update_num_bars (w);
    const int num_bars = get_num_bars ();
    const double ratio = num_bars / 132.0;
    const double a4pos = 57.0 * ratio;
    const double octave = 12.0 * ratio;

    for (int i = 0; i < num_bars; i++) {
        w->freq[i] = 440.0 * pow (2.0, (double)(i-a4pos)/octave);
        w->keys[i] = ftoi (w->freq[i] * CONFIG_FFT_SIZE/(float)w->samplerate);
        if (i > 0 && w->keys[i-1] == w->keys[i])
            w->low_res_end = i;
    }

    int last_key = 0;
    w->low_res_indices_num = 1;
    w->low_res_indices[0] = 0;
    for (int i = 0, j = 1; i < num_bars; i++) {
        int key = w->keys[i];
        if (key != last_key) {
            w->low_res_indices[j++] = i;
            w->low_res_indices_num++;
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

