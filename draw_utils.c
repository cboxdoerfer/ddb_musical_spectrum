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
#include <gtk/gtk.h>

#include "draw_utils.h"
#include "config.h"

static cairo_pattern_t *
spectrum_gradient_pattern_get (double width, double height)
{
    cairo_pattern_t *pat = NULL;

    if (CONFIG_GRADIENT_ORIENTATION == 0) {
        pat = cairo_pattern_create_linear (0, 0, 0, height);
    }
    else {
        pat = cairo_pattern_create_linear (0, 0, width, 0);
    }

    const double step = 1.0/(CONFIG_NUM_COLORS - 1);
    double grad_pos = 0;
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        cairo_pattern_add_color_stop_rgb (pat,
                                          grad_pos,
                                          CONFIG_GRADIENT_COLORS[i].red/65535.0,
                                          CONFIG_GRADIENT_COLORS[i].green/65535.0,
                                          CONFIG_GRADIENT_COLORS[i].blue/65535.0);
        grad_pos += step;
    }
    return pat;
}

void
spectrum_gradient_set (cairo_t *cr, double width, double height)
{
    if (CONFIG_NUM_COLORS > 1) {
        cairo_pattern_t *pat = spectrum_gradient_pattern_get (width, height);
        cairo_set_source (cr, pat);
        cairo_pattern_destroy (pat);
    }
    else {
        gdk_cairo_set_source_color (cr, &CONFIG_GRADIENT_COLORS[0]);
    }
}

