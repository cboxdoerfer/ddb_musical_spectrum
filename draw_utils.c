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
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include "fastftoi.h"
#include "draw_utils.h"
#include "spectrum.h"
#include "utils.h"

void
_draw_vline (uint8_t *data, int stride, int x0, int y0, int y1, uint32_t color) {
    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
        y1--;
    }
    else if (y0 < y1) {
        y0++;
    }
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    int line_size = stride/4;
    while (y0 <= y1) {
        *ptr = color;
        ptr += line_size;
        y0++;
    }
}

void
_draw_hline (uint8_t *data, int stride, int x0, int y0, int x1, uint32_t color) {
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (x0 <= x1) {
        *ptr++ = color;
        x0++;
    }
}

void
_draw_background (uint8_t *data, int w, int h, uint32_t color)
{
    //uint32_t *ptr = (uint32_t*)&data[0];
    //int i = 0;
    //int size = w * h;
    //while (i++ < size) {
    //    *ptr++ = color;
    //}
    size_t fillLen = w * h * sizeof (uint32_t);
    _memset_pattern ((char *)data, &color, fillLen, sizeof (uint32_t));
}

void
_draw_bar (uint8_t *data, int stride, int x0, int y0, int w, int h, uint32_t color) {
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        while (x++ <= x1) {
            *ptr++ = color;
        }
        y0++;
        ptr += stride/4-w;
    }
}

void
_draw_bar_gradient_v (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h) {
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        int index = ftoi(((double)y0/(double)total_h) * (GRADIENT_TABLE_SIZE - 1));
        index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
        while (x++ <= x1) {
            *ptr++ = colors[index];
        }
        y0++;
        ptr += stride/4-w;
    }
}

void
_draw_bar_gradient_h (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_w) {
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        while (x++ <= x1) {
            int index = ftoi(((double)x/(double)total_w) * (GRADIENT_TABLE_SIZE - 1));
            index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
            *ptr++ = colors[index];
        }
        y0++;
        ptr += stride/4-w;
    }
}

void
_draw_bar_gradient_bar_mode_v (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h) {
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    y0 -= y0 % 2;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        int index = ftoi(((double)y0/(double)total_h) * (GRADIENT_TABLE_SIZE - 1));
        index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
        while (x++ <= x1) {
            *ptr++ = colors[index];
        }
        y0 += 2;
        ptr += stride/2-w;
    }
}

void
_draw_bar_gradient_bar_mode_h (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_w) {
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    y0 -= y0 % 2;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        while (x++ <= x1) {
            int index = ftoi(((double)x/(double)total_w) * (GRADIENT_TABLE_SIZE - 1));
            index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
            *ptr++ = colors[index];
        }
        y0 += 2;
        ptr += stride/2-w;
    }
}

