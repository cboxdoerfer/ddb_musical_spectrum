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

#ifndef DRAW_UTILS_HEADER
#define DRAW_UTILS_HEADER

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

void
_draw_vline (uint8_t *data, int stride, int x0, int y0, int y1, uint32_t color);

void
_draw_hline (uint8_t *data, int stride, int x0, int y0, int x1, uint32_t color);

void
_draw_background (uint8_t *data, int w, int h, uint32_t color);

void
_draw_bar (uint8_t *data, int stride, int x0, int y0, int w, int h, uint32_t color);

void
_draw_bar_gradient_v (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h);

void
_draw_bar_gradient_h (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_w);

void
_draw_bar_gradient_bar_mode_v (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h);

void
_draw_bar_gradient_bar_mode_h (uint32_t *colors, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_w);

#endif
