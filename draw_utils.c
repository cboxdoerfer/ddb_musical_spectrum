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
#include <assert.h>
#include <math.h>
#include <gtk/gtk.h>

#include "draw_utils.h"
#include "config.h"

cairo_pattern_t *
spectrum_gradient_pattern_get (GList *colors, int orientation, double width, double height)
{
    if (!colors) {
        return NULL;
    }

    cairo_pattern_t *pat = NULL;

    if (orientation == 0) {
        pat = cairo_pattern_create_linear (0, 0, 0, height);
    }
    else {
        pat = cairo_pattern_create_linear (0, 0, width, 0);
    }

    const int num_colors = g_list_length (colors);
    const double step = 1.0/(num_colors - 1);
    double grad_pos = 0;
    for (GList *c = colors; c != NULL; c= c->next) {
        GdkColor *color = c->data;
        cairo_pattern_add_color_stop_rgb (pat,
                                          grad_pos,
                                          color->red/65535.0,
                                          color->green/65535.0,
                                          color->blue/65535.0);
        grad_pos += step;
    }
    return pat;
}

void
spectrum_gradient_set (cairo_t *cr, GList *colors, int orientation, double width, double height)
{
    const int num_colors = g_list_length (colors);
    if (num_colors > 1) {
        cairo_pattern_t *pat = spectrum_gradient_pattern_get (colors, orientation, width, height);
        cairo_set_source (cr, pat);
        cairo_pattern_destroy (pat);
        pat = NULL;
    }
    else {
        gdk_cairo_set_source_color (cr, colors->data);
    }
}

