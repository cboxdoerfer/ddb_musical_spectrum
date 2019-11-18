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
#include "config.h"
#include "config_dialog.h"
#include "draw_utils.h"
#include "utils.h"
#include "spectrum.h"
#include "interface.h"
#include "callbacks.h"

static const char *window_functions[] = {"Blackmann-Harris", "Hanning"};
static size_t window_functions_size = 2;
static const char *alignment_title[] = {"Left", "Right", "Center"};
static size_t alignment_title_size = 3;
static const char *grad_orientation[] = {"Vertical", "Horizontal"};
static size_t grad_orientation_size = 2;
static const char *visual_mode[] = {"Musical", "Solid", "Bars"};
static size_t visual_mode_size = 3;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void
gradient_draw_generic_event (GtkWidget *widget, cairo_t *cr)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (dialog, "color_box"));
    GList *children = gtk_container_get_children (color_box);

    if (!children) {
        return;
    }

    GList *colors = NULL;
    for (GList *c = children; c != NULL; c = c->next) {
        GtkColorButton *button = GTK_COLOR_BUTTON (c->data);
        GdkColor *clr = malloc (sizeof (GdkColor));
        gtk_color_button_get_color (button, clr);
        colors = g_list_append (colors, clr);
    }
    g_list_free (children);

    if (!colors) {
        return;
    }


    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);
    spectrum_gradient_list_set (cr, colors, a.width, a.height);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);

    g_list_free_full (colors, free);

    return;
}

#if !GTK_CHECK_VERSION(3,0,0)
static gboolean
on_gradient_preview_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gradient_draw_generic_event (widget, cr);
    cairo_destroy (cr);

    return FALSE;
}
#else
static gboolean
on_gradient_preview_draw_event (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    gradient_draw_generic_event (widget, cr);
    return FALSE;
}
#endif

static int
get_spin_button (GtkWidget *w, const char *w_name) {
    GtkSpinButton *button = GTK_SPIN_BUTTON (lookup_widget (w, w_name));
    return gtk_spin_button_get_value_as_int (button);
}

static void
set_spin_button (GtkWidget *w, const char *w_name, int value) {
    GtkSpinButton *button = GTK_SPIN_BUTTON (lookup_widget (w, w_name));
    gtk_spin_button_set_value (button, value);
}

static int
get_toggle_button (GtkWidget *w, const char *w_name) {
    GtkToggleButton *button = GTK_TOGGLE_BUTTON (lookup_widget (w, w_name));
    return gtk_toggle_button_get_active (button);
}

static void
set_toggle_button (GtkWidget *w, const char *w_name, int value) {
    GtkToggleButton *button = GTK_TOGGLE_BUTTON (lookup_widget (w, w_name));
    gtk_toggle_button_set_active (button, value);
}

static int
get_combo_box (GtkWidget *w, const char *w_name)
{
    GtkComboBox *box = GTK_COMBO_BOX (lookup_widget (w, w_name));
    return gtk_combo_box_get_active (box);
}

static void
set_combo_box (GtkWidget *w, const char *w_name, const char **values, size_t n_values, int index)
{
    GtkComboBoxText *box = GTK_COMBO_BOX_TEXT (lookup_widget (w, w_name));
    for (int i = 0; i < n_values; i++) {
        gtk_combo_box_text_append_text (box, values[i]);
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (box), index);
}

static void
get_color_button (GtkWidget *w, const char *w_name, GdkColor *color)
{
    GtkColorButton *button = GTK_COLOR_BUTTON (lookup_widget (w, w_name));
    gtk_color_button_get_color (button, color);
}

static void
set_color_button (GtkWidget *w, const char *w_name, GdkColor *color)
{
    GtkColorButton *button = GTK_COLOR_BUTTON (lookup_widget (w, w_name));
    gtk_color_button_set_color (button, color);
}

static void
get_gradient_colors (GtkWidget *w)
{
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (w, "color_box"));
    GList *children = gtk_container_get_children (color_box);

    int i = 0;
    for (GList *c = children; c != NULL; c = c->next, i++) {
        GtkColorButton *button = GTK_COLOR_BUTTON (c->data);
        gtk_color_button_get_color (button, &CONFIG_GRADIENT_COLORS[i]);
    }
    CONFIG_NUM_COLORS = i;
    g_list_free (children);
}

static void
set_gradient_colors (GtkWidget *w)
{
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (w, "color_box"));

    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        GtkWidget *button = gtk_color_button_new ();
        gtk_color_button_set_use_alpha (GTK_COLOR_BUTTON (button), TRUE);
        gtk_box_pack_start (GTK_BOX (color_box), button, TRUE, TRUE, 0);
        gtk_widget_show (button);
        gtk_widget_set_size_request (button, -1, 30);
        gtk_color_button_set_color (GTK_COLOR_BUTTON (button), &(CONFIG_GRADIENT_COLORS[i]));
        g_signal_connect_after ((gpointer)button, "color-set", G_CALLBACK (on_color_changed), w);
    }
}

void
set_config_values (GtkWidget *w)
{
    set_toggle_button (w, "llabel_check", CONFIG_ENABLE_LEFT_LABELS);
    set_toggle_button (w, "rlabel_check", CONFIG_ENABLE_RIGHT_LABELS);
    set_toggle_button (w, "tlabel_check", CONFIG_ENABLE_TOP_LABELS);
    set_toggle_button (w, "blabel_check", CONFIG_ENABLE_BOTTOM_LABELS);

    set_toggle_button (w, "hgrid_check", CONFIG_ENABLE_HGRID);
    set_toggle_button (w, "vgrid_check", CONFIG_ENABLE_VGRID);
    set_toggle_button (w, "ogrid_check", CONFIG_ENABLE_OGRID);

    set_toggle_button (w, "fill_spectrum_check", CONFIG_FILL_SPECTRUM);

    set_toggle_button (w, "tooltip_check", CONFIG_ENABLE_TOOLTIP);
    set_toggle_button (w, "spacing_check", CONFIG_GAPS);

    set_toggle_button (w, "peaks_check", CONFIG_ENABLE_PEAKS);
    set_toggle_button (w, "amplitudes_check", CONFIG_ENABLE_AMPLITUDES);

    set_spin_button (w, "notes_max_spin", CONFIG_NOTE_MAX);
    set_spin_button (w, "notes_min_spin", CONFIG_NOTE_MIN);
    set_spin_button (w, "amp_max_spin", CONFIG_AMPLITUDE_MAX);
    set_spin_button (w, "amp_min_spin", CONFIG_AMPLITUDE_MIN);
    set_spin_button (w, "interval_spin", CONFIG_REFRESH_INTERVAL);
    set_spin_button (w, "peaks_htime_spin", CONFIG_PEAK_DELAY);
    set_spin_button (w, "amplitudes_htime_spin", CONFIG_BAR_DELAY);
    set_spin_button (w, "peaks_gravity_spin", CONFIG_PEAK_FALLOFF);
    set_spin_button (w, "amplitudes_gravity_spin", CONFIG_BAR_FALLOFF);
    set_spin_button (w, "fft_spin", FFT_INDEX);

    set_combo_box (w, "window_combo", window_functions, window_functions_size, CONFIG_WINDOW);
    set_combo_box (w, "alignment_combo", alignment_title, alignment_title_size, CONFIG_ALIGNMENT);
    set_combo_box (w, "gradient_combo", grad_orientation, grad_orientation_size, CONFIG_GRADIENT_ORIENTATION);
    set_combo_box (w, "mode_combo", visual_mode, visual_mode_size, CONFIG_DRAW_STYLE);

    set_color_button (w, "background_color", &CONFIG_COLOR_BG);
    set_color_button (w, "vgrid_color", &CONFIG_COLOR_VGRID);
    set_color_button (w, "hgrid_color", &CONFIG_COLOR_HGRID);
    set_color_button (w, "ogrid_color", &CONFIG_COLOR_OGRID);
    set_color_button (w, "text_color", &CONFIG_COLOR_TEXT);

    set_gradient_colors (w);
}

void
get_config_values (GtkWidget *w)
{
    CONFIG_ENABLE_LEFT_LABELS =   get_toggle_button (w, "llabel_check");
    CONFIG_ENABLE_RIGHT_LABELS =  get_toggle_button (w, "rlabel_check");
    CONFIG_ENABLE_TOP_LABELS =    get_toggle_button (w, "tlabel_check");
    CONFIG_ENABLE_BOTTOM_LABELS = get_toggle_button (w, "blabel_check");

    CONFIG_ENABLE_HGRID = get_toggle_button (w, "hgrid_check");
    CONFIG_ENABLE_VGRID = get_toggle_button (w, "vgrid_check");
    CONFIG_ENABLE_OGRID = get_toggle_button (w, "ogrid_check");

    CONFIG_FILL_SPECTRUM = get_toggle_button (w, "fill_spectrum_check");
    CONFIG_ENABLE_TOOLTIP = get_toggle_button (w, "tooltip_check");
    CONFIG_GAPS = get_toggle_button (w, "spacing_check");

    CONFIG_ENABLE_PEAKS = get_toggle_button (w, "peaks_check");
    CONFIG_ENABLE_AMPLITUDES = get_toggle_button (w, "amplitudes_check");

    CONFIG_NOTE_MIN = get_spin_button (w, "notes_min_spin");
    CONFIG_NOTE_MAX = get_spin_button (w, "notes_max_spin");
    CONFIG_AMPLITUDE_MIN = get_spin_button (w, "amp_min_spin");
    CONFIG_AMPLITUDE_MAX = get_spin_button (w, "amp_max_spin");
    CONFIG_REFRESH_INTERVAL = get_spin_button (w, "interval_spin");
    CONFIG_PEAK_DELAY = get_spin_button (w, "peaks_htime_spin");
    CONFIG_BAR_DELAY = get_spin_button (w, "amplitudes_htime_spin");
    CONFIG_PEAK_FALLOFF = get_spin_button (w, "peaks_gravity_spin");
    CONFIG_BAR_FALLOFF = get_spin_button (w, "amplitudes_gravity_spin");
    FFT_INDEX = get_spin_button (w, "fft_spin");
    CONFIG_FFT_SIZE = (int)exp2 (FFT_INDEX + 9);

    CONFIG_WINDOW = get_combo_box (w, "window_combo");
    CONFIG_ALIGNMENT = get_combo_box (w, "alignment_combo");
    CONFIG_GRADIENT_ORIENTATION = get_combo_box (w, "gradient_combo");
    CONFIG_DRAW_STYLE = get_combo_box (w, "mode_combo");

    get_color_button (w, "background_color", &CONFIG_COLOR_BG);
    get_color_button (w, "vgrid_color", &CONFIG_COLOR_VGRID);
    get_color_button (w, "hgrid_color", &CONFIG_COLOR_HGRID);
    get_color_button (w, "ogrid_color", &CONFIG_COLOR_OGRID);
    get_color_button (w, "text_color", &CONFIG_COLOR_TEXT);

    get_gradient_colors (w);
}

static void
set_callbacks (GtkWidget *w)
{
    GtkWidget *gradient_preview = GTK_WIDGET (lookup_widget (w, "gradient_preview"));
#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) gradient_preview, "expose_event", G_CALLBACK (on_gradient_preview_expose_event), NULL);
#else
    g_signal_connect_after ((gpointer) gradient_preview, "draw", G_CALLBACK (on_gradient_preview_draw_event), NULL);
#endif
}

void
on_button_config (GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *dialog = create_config_dialog ();

    set_callbacks (dialog);
    set_config_values (dialog);

    while (TRUE) {
        int response = gtk_dialog_run (GTK_DIALOG (dialog));
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
            get_config_values (dialog);
            save_config ();
            deadbeef->sendmessage (DB_EV_CONFIGCHANGED, 0, 0, 0);
        }
        if (response == GTK_RESPONSE_APPLY) {
            continue;
        }
        break;
    }
    gtk_widget_destroy (dialog);
    return;
}
#pragma GCC diagnostic pop

