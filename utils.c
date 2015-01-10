/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2014 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    Based on DeaDBeeFs stock spectrum.
    Copyright (c) 2009-2014 Alexey Yakovenko <waker@users.sourceforge.net>
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
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include "fastftoi.h"
#include "config.h"
#include "spectrum.h"

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

/* based on Delphi function by Witold J.Janik */
void
create_gradient_table (gpointer user_data, GdkColor *colors, int num_colors)
{
    w_spectrum_t *w = user_data;

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

        w->colors[i] = 0xFF000000;
        float scale = 255/65535.f;
        if (num_colors == 0) {
            w->colors[i] = (uint32_t)(colors[0].red*scale) << 16 |
                (uint32_t)(colors[0].green*scale) << 8 |
                (uint32_t)(colors[0].blue*scale) << 0;
        }
        else if (n < num_colors) {
            w->colors[i] = (uint32_t)((colors[n].red*scale) + f * ((colors[n+1].red*scale)-(colors[n].red*scale))) << 16 |
                (uint32_t)((colors[n].green*scale) + f * ((colors[n+1].green*scale)-(colors[n].green*scale))) << 8 |
                (uint32_t)((colors[n].blue*scale) + f * ((colors[n+1].blue*scale)-(colors[n].blue*scale))) << 0;
        }
        else if (n == num_colors) {
            w->colors[i] = (uint32_t)(colors[n].red*scale) << 16 |
                (uint32_t)(colors[n].green*scale) << 8 |
                (uint32_t)(colors[n].blue*scale) << 0;
        }
        else {
            w->colors[i] = 0xFFFFFFFF;
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
create_frequency_table (gpointer user_data)
{
    w_spectrum_t *w = user_data;

    w->low_res_end = 0;
    for (int i = 0; i < MAX_BANDS; i++) {
        w->freq[i] = 440.0 * pow (2.0, (double)(i-57)/12.0);
        w->keys[i] = ftoi (w->freq[i] * CONFIG_FFT_SIZE/(float)w->samplerate);
        if (i > 0 && w->keys[i-1] == w->keys[i])
            w->low_res_end = i;
    }
}

