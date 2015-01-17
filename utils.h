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

#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <gtk/gtk.h>

extern int CALCULATED_NUM_BARS;

void
_memset_pattern (char *data, const void* pattern, size_t data_len, size_t pattern_len);

void
update_num_bars (gpointer user_data);

int
get_num_bars ();

void
create_gradient_table (uint32_t *dest, GdkColor *colors, int num_colors);

void
create_window_table (gpointer user_data);

void
create_frequency_table (gpointer user_data);

float
linear_interpolate (float y1, float y2, float mu);

float
lagrange_interpolate (float y0, float y1, float y2, float y3, float x);
#endif
