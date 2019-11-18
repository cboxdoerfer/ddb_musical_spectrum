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
#include "render.h"
#include "spectrum.h"

extern int CALCULATED_NUM_BARS;

void
update_num_bars (int width);

int
get_num_bars ();

void
update_gravity (struct spectrum_render_t *render);

void
window_table_fill (double *window);

void
create_frequency_table (struct spectrum_data_t *s, int samplerate, int width);

double
hermite_interpolate (double *y,
                     double mu,
                     int start,
                     double tension,
                     double bias);

