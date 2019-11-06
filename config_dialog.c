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

#define     STR_GRADIENT_VERTICAL "Vertical"
#define     STR_GRADIENT_HORIZONTAL "Horizontal"
#define     STR_DRAW_STYLE_SOLID "Solid"
#define     STR_DRAW_STYLE_BARS "Bars"
#define     STR_ALIGNMENT_LEFT "Left"
#define     STR_ALIGNMENT_RIGHT "Right"
#define     STR_ALIGNMENT_CENTER "Center"
#define     STR_WINDOW_BLACKMANN_HARRIS "Blackmann-Harris"
#define     STR_WINDOW_HANNING "Hanning"

static char *fft_sizes[] = {"512", "1024", "2048", "4096", "8192", "16384", "32768"};
static int fft_sizses_size = 7;
static GdkColor gradient_colors_temp[MAX_NUM_COLORS];

static void
init_gradient_colors ()
{
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        gradient_colors_temp[i].red = CONFIG_GRADIENT_COLORS[i].red;
        gradient_colors_temp[i].green = CONFIG_GRADIENT_COLORS[i].green;
        gradient_colors_temp[i].blue = CONFIG_GRADIENT_COLORS[i].blue;
    }
}

static gboolean
draw_gradient_preview (GtkWidget *widget, cairo_t *cr)
{
    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);
    spectrum_gradient_set (cr, a.width, a.height);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);

    return TRUE;
}

static GtkWidget *color_gradients[MAX_NUM_COLORS];

static gboolean
on_num_colors_changed (GtkSpinButton *spin, gpointer user_data)
{
    GtkWidget *w = (GtkWidget *)user_data;

    int value = gtk_spin_button_get_value_as_int (spin);
    for (int i = 0; i < MAX_NUM_COLORS; i++) {
        if (i < value) {
            gtk_widget_show (color_gradients[i]);
        }
        else {
            gtk_widget_hide (color_gradients[i]);
        }
    }

    gtk_widget_queue_draw (w);
    return TRUE;
}

static gboolean
gradient_preview_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gboolean res = draw_gradient_preview (widget, cr);
    cairo_destroy (cr);
    return res;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static gboolean
on_color_changed (GtkWidget *widget, gpointer user_data)
{
    GtkWidget *w = (GtkWidget *)user_data;
    for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
        gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradients[i]), &gradient_colors_temp[i]);
    }
    gtk_widget_queue_draw (w);
    return TRUE;
}

static GtkWidget *vbox_draw_style_bars;
static GtkWidget *vbox_draw_style_solid;

static void
on_draw_style_bars_toggled (GtkToggleButton *togglebutton,
                            gpointer user_data)
{
    int active = gtk_toggle_button_get_active (togglebutton);
    if (active == 0) {
        gtk_widget_show (vbox_draw_style_solid);
        gtk_widget_hide (vbox_draw_style_bars);
    }
    else {
        gtk_widget_show (vbox_draw_style_bars);
        gtk_widget_hide (vbox_draw_style_solid);
    }
}

static void
on_draw_style_solid_toggled (GtkToggleButton *togglebutton,
                             gpointer user_data)
{
    int active = gtk_toggle_button_get_active (togglebutton);
    if (active == 0) {
        gtk_widget_show (vbox_draw_style_bars);
        gtk_widget_hide (vbox_draw_style_solid);
    }
    else {
        gtk_widget_show (vbox_draw_style_solid);
        gtk_widget_hide (vbox_draw_style_bars);
    }
}

static gboolean
on_bar_width_changed (GtkSpinButton *spin, gpointer user_data)
{
    GtkWidget *hbox_num_bars = GTK_WIDGET (user_data);
    int value = gtk_spin_button_get_value_as_int (spin);
    if (value > 0) {
        gtk_widget_set_sensitive (hbox_num_bars, FALSE);
    }
    else {
        gtk_widget_set_sensitive (hbox_num_bars, TRUE);
    }
    return TRUE;
}


void
on_button_config (GtkMenuItem *menuitem, gpointer user_data)
{
    init_gradient_colors ();

    GtkWidget *spectrum_properties = gtk_dialog_new ();
    gtk_window_set_transient_for (GTK_WINDOW (spectrum_properties), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET(menuitem))));
    gtk_window_set_title (GTK_WINDOW (spectrum_properties), "Spectrum Properties");
    gtk_window_set_type_hint (GTK_WINDOW (spectrum_properties), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable (GTK_WINDOW (spectrum_properties), FALSE);

    GtkWidget *config_dialog = gtk_dialog_get_content_area (GTK_DIALOG (spectrum_properties));
    gtk_widget_show (config_dialog);

    GtkWidget *hbox01 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox01);
    gtk_box_pack_start (GTK_BOX (config_dialog), hbox01, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox01), 8);

    GtkWidget *color_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_label),"<b>Colors</b>");
    gtk_widget_show (color_label);

    GtkWidget *color_frame = gtk_frame_new ("Colors");
    gtk_frame_set_label_widget ((GtkFrame *)color_frame, color_label);
    gtk_frame_set_shadow_type ((GtkFrame *)color_frame, GTK_SHADOW_IN);
    gtk_widget_show (color_frame);
    gtk_box_pack_start (GTK_BOX (hbox01), color_frame, TRUE, TRUE, 0);

    GtkWidget *vbox01 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox01);
    gtk_container_add (GTK_CONTAINER (color_frame), vbox01);
    gtk_container_set_border_width (GTK_CONTAINER (vbox01), 12);

    GtkWidget *hbox02 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox02);
    gtk_box_pack_start (GTK_BOX (vbox01), hbox02, FALSE, FALSE, 0);

    GtkWidget *vbox03 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox03);
    gtk_box_pack_start (GTK_BOX (hbox02), vbox03, TRUE, TRUE, 0);

    GtkWidget *vbox04 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox04);
    gtk_box_pack_start (GTK_BOX (hbox02), vbox04, TRUE, TRUE, 0);

    GtkWidget *valign_01 = gtk_alignment_new(0, 1, 0, 1);
    gtk_container_add(GTK_CONTAINER(vbox03), valign_01);
    gtk_widget_show (valign_01);

    GtkWidget *color_bg_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_bg_label),"Background:");
    gtk_widget_show (color_bg_label);
    gtk_container_add(GTK_CONTAINER(valign_01), color_bg_label);

    GtkWidget *color_bg = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_bg, TRUE);
    gtk_widget_show (color_bg);
    gtk_box_pack_start (GTK_BOX (vbox04), color_bg, TRUE, TRUE, 0);

    GtkWidget *valign_02 = gtk_alignment_new(0, 1, 0, 1);
    gtk_container_add(GTK_CONTAINER(vbox03), valign_02);
    gtk_widget_show (valign_02);

    GtkWidget *color_vgrid_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_vgrid_label),"Vertical grid:");
    gtk_widget_show (color_vgrid_label);
    gtk_container_add(GTK_CONTAINER(valign_02), color_vgrid_label);

    GtkWidget *color_vgrid = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_vgrid, TRUE);
    gtk_widget_show (color_vgrid);
    gtk_box_pack_start (GTK_BOX (vbox04), color_vgrid, TRUE, TRUE, 0);

    GtkWidget *valign_03 = gtk_alignment_new(0, 1, 0, 1);
    gtk_container_add(GTK_CONTAINER(vbox03), valign_03);
    gtk_widget_show (valign_03);

    GtkWidget *color_hgrid_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_hgrid_label),"Horizontal grid:");
    gtk_widget_show (color_hgrid_label);
    gtk_container_add(GTK_CONTAINER(valign_03), color_hgrid_label);

    GtkWidget *color_hgrid = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_hgrid, TRUE);
    gtk_widget_show (color_hgrid);
    gtk_box_pack_start (GTK_BOX (vbox04), color_hgrid, TRUE, TRUE, 0);

    GtkWidget *valign_04 = gtk_alignment_new(0, 1, 0, 1);
    gtk_container_add(GTK_CONTAINER(vbox03), valign_04);
    gtk_widget_show (valign_04);

    GtkWidget *color_ogrid_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_ogrid_label),"Octave grid:");
    gtk_widget_show (color_ogrid_label);
    gtk_container_add(GTK_CONTAINER(valign_04), color_ogrid_label);

    GtkWidget *color_ogrid = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_ogrid, TRUE);
    gtk_widget_show (color_ogrid);
    gtk_box_pack_start (GTK_BOX (vbox04), color_ogrid, TRUE, TRUE, 0);

    GtkWidget *hseparator_01 = gtk_hseparator_new ();
    gtk_widget_show (hseparator_01);
    gtk_box_pack_start (GTK_BOX (vbox01), hseparator_01, FALSE, FALSE, 0);

    GtkWidget *num_colors_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (num_colors_label),"Number of colors:");
    gtk_widget_show (num_colors_label);
    gtk_box_pack_start (GTK_BOX (vbox01), num_colors_label, FALSE, FALSE, 0);

    GtkWidget *num_colors = gtk_spin_button_new_with_range (1,MAX_NUM_COLORS,1);
    gtk_widget_show (num_colors);
    gtk_box_pack_start (GTK_BOX (vbox01), num_colors, FALSE, FALSE, 0);

    GtkWidget *hbox08 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox08);
    gtk_box_pack_start (GTK_BOX (vbox01), hbox08, TRUE, TRUE, 0);

    GtkWidget *gradient_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type ((GtkFrame *)gradient_frame, GTK_SHADOW_ETCHED_IN);
    gtk_widget_show (gradient_frame);
    gtk_box_pack_start (GTK_BOX (hbox08), gradient_frame, FALSE, FALSE, 0);
    GtkWidget *gradient_preview = gtk_drawing_area_new ();
#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) gradient_preview, "expose_event", G_CALLBACK (gradient_preview_expose_event), user_data);
#else
    g_signal_connect_after ((gpointer) gradient_preview, "draw", G_CALLBACK (gradient_preview_expose_event), user_data);
#endif
    g_signal_connect_after ((gpointer) num_colors, "value-changed", G_CALLBACK (on_num_colors_changed), gradient_preview);
    gtk_container_add (GTK_CONTAINER (gradient_frame), gradient_preview);
    gtk_widget_set_size_request (gradient_preview, 16, -1);
    gtk_widget_show (gradient_preview);

    GtkWidget* scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *viewport = gtk_viewport_new (NULL, NULL);
    GtkWidget *vbox05 = gtk_vbox_new (FALSE, 0);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (hbox08), scrolledwindow, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(viewport), vbox05);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    gtk_widget_show (scrolledwindow);
    gtk_widget_show (viewport);
    gtk_widget_show (vbox05);

    for (int i = 0; i < MAX_NUM_COLORS; i++) {
        color_gradients[i] = gtk_color_button_new ();
        gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradients[i], TRUE);
        if (i < CONFIG_NUM_COLORS) {
            gtk_widget_show (color_gradients[i]);
        }
        else {
            gtk_widget_hide (color_gradients[i]);
        }
        gtk_box_pack_start (GTK_BOX (vbox05), color_gradients[i], TRUE, FALSE, 0);
        gtk_widget_set_size_request (color_gradients[i], -1, 30);
        gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradients[i]), &(CONFIG_GRADIENT_COLORS[i]));
        g_signal_connect_after ((gpointer) color_gradients[i], "color-set", G_CALLBACK (on_color_changed), gradient_preview);
    }

    GtkWidget *vbox02 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox02);
    gtk_box_pack_start (GTK_BOX (hbox01), vbox02, FALSE, FALSE, 0);

    GtkWidget *processing_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (processing_label),"<b>Processing</b>");
    gtk_widget_show (processing_label);

    GtkWidget *processing_frame = gtk_frame_new ("Colors");
    gtk_frame_set_label_widget ((GtkFrame *)processing_frame, processing_label);
    gtk_frame_set_shadow_type ((GtkFrame *)processing_frame, GTK_SHADOW_IN);
    gtk_widget_show (processing_frame);
    gtk_box_pack_start (GTK_BOX (vbox02), processing_frame, TRUE, TRUE, 0);

    GtkWidget *vbox06 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox06);
    gtk_container_add (GTK_CONTAINER (processing_frame), vbox06);
    gtk_container_set_border_width (GTK_CONTAINER (vbox06), 12);

    GtkWidget *hbox09 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox09);
    gtk_box_pack_start (GTK_BOX (vbox06), hbox09, FALSE, FALSE, 0);

    GtkWidget *db_range_label0 = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (db_range_label0),"dB range:");
    gtk_widget_show (db_range_label0);
    gtk_box_pack_start (GTK_BOX (hbox09), db_range_label0, FALSE, TRUE, 0);

    GtkWidget *db_range = gtk_spin_button_new_with_range (50,120,10);
    gtk_widget_show (db_range);
    gtk_box_pack_start (GTK_BOX (hbox09), db_range, TRUE, TRUE, 0);

    GtkWidget *hbox07 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox07);
    gtk_box_pack_start (GTK_BOX (vbox06), hbox07, FALSE, FALSE, 0);

    GtkWidget *fft_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (fft_label),"FFT size:");
    gtk_widget_show (fft_label);
    gtk_box_pack_start (GTK_BOX (hbox07), fft_label, FALSE, TRUE, 0);

    GtkWidget *fft = gtk_combo_box_text_new ();
    gtk_widget_show (fft);
    gtk_box_pack_start (GTK_BOX (hbox07), fft, TRUE, TRUE, 0);
    for (int i = 0; i < fft_sizses_size; i++) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(fft), fft_sizes[i]);
    }

    GtkWidget *hbox04 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox04);
    gtk_box_pack_start (GTK_BOX (vbox06), hbox04, FALSE, FALSE, 0);

    GtkWidget *window_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (window_label),"Window function:");
    gtk_widget_show (window_label);
    gtk_box_pack_start (GTK_BOX (hbox04), window_label, FALSE, TRUE, 0);

    GtkWidget *window = gtk_combo_box_text_new ();
    gtk_widget_show (window);
    gtk_box_pack_start (GTK_BOX (hbox04), window, TRUE, TRUE, 0);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(window), STR_WINDOW_BLACKMANN_HARRIS);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window), STR_WINDOW_HANNING);

    GtkWidget *style_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (style_label),"<b>Style</b>");
    gtk_widget_show (style_label);

    GtkWidget *style_frame = gtk_frame_new ("Style");
    gtk_frame_set_label_widget ((GtkFrame *)style_frame, style_label);
    gtk_frame_set_shadow_type ((GtkFrame *)style_frame, GTK_SHADOW_IN);
    gtk_widget_show (style_frame);
    gtk_box_pack_start (GTK_BOX (vbox02), style_frame, TRUE, TRUE, 0);

    GtkWidget *vbox07 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox07);
    gtk_container_add (GTK_CONTAINER (style_frame), vbox07);
    gtk_container_set_border_width (GTK_CONTAINER (vbox07), 12);

    GtkWidget *hbox_draw_style = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox_draw_style);

    GtkWidget *draw_style_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (draw_style_label),"Draw style:");
    gtk_widget_show (draw_style_label);
    gtk_box_pack_start (GTK_BOX (hbox_draw_style), draw_style_label, FALSE, TRUE, 0);

    GtkWidget *draw_style_bars_radio = gtk_radio_button_new_with_label (NULL, "Bars");
    gtk_widget_show (draw_style_bars_radio);
    gtk_box_pack_start (GTK_BOX (hbox_draw_style), draw_style_bars_radio, TRUE, TRUE, 0);

    GtkWidget *draw_style_solid_radio = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (draw_style_bars_radio), "Solid");
    gtk_widget_show (draw_style_solid_radio);
    gtk_box_pack_start (GTK_BOX (hbox_draw_style), draw_style_solid_radio, TRUE, TRUE, 0);

    g_signal_connect_after ((gpointer) draw_style_bars_radio, "toggled", G_CALLBACK (on_draw_style_bars_toggled), NULL);
    g_signal_connect_after ((gpointer) draw_style_solid_radio, "toggled", G_CALLBACK (on_draw_style_solid_toggled), NULL);

    GtkWidget *draw_style_frame = gtk_frame_new ("Draw style");
    gtk_frame_set_label_widget ((GtkFrame *)draw_style_frame, hbox_draw_style);
    gtk_frame_set_shadow_type ((GtkFrame *)draw_style_frame, GTK_SHADOW_IN);
    gtk_widget_show (draw_style_frame);
    gtk_box_pack_start (GTK_BOX (vbox07), draw_style_frame, TRUE, TRUE, 0);

    GtkWidget *vbox_draw_styles = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox_draw_styles);
    gtk_container_add (GTK_CONTAINER (draw_style_frame), vbox_draw_styles);

    vbox_draw_style_bars = gtk_vbox_new (FALSE, 8);
    if (!CONFIG_DRAW_STYLE) {
        gtk_widget_show (vbox_draw_style_bars);
    }
    gtk_container_add (GTK_CONTAINER (vbox_draw_styles), vbox_draw_style_bars);
    gtk_container_set_border_width (GTK_CONTAINER (vbox_draw_style_bars), 12);

    GtkWidget *hbox_bar_width = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox_bar_width);
    gtk_container_add (GTK_CONTAINER (vbox_draw_style_bars), hbox_bar_width);

    GtkWidget *bar_width_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (bar_width_label),"Bar width (0 = auto):");
    gtk_widget_show (bar_width_label);
    gtk_box_pack_start (GTK_BOX (hbox_bar_width), bar_width_label, FALSE, TRUE, 0);

    GtkWidget *hbox_num_bars = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox_num_bars);
    gtk_container_add (GTK_CONTAINER (vbox_draw_style_bars), hbox_num_bars);
    if (CONFIG_BAR_W > 0) {
        gtk_widget_set_sensitive (hbox_num_bars, FALSE);
    }
    else {
        gtk_widget_set_sensitive (hbox_num_bars, TRUE);
    }

    GtkWidget *bar_width = gtk_spin_button_new_with_range (0,10,1);
    gtk_widget_show (bar_width);
    gtk_box_pack_start (GTK_BOX (hbox_bar_width), bar_width, TRUE, TRUE, 0);
    g_signal_connect_after ((gpointer) bar_width, "value-changed", G_CALLBACK (on_bar_width_changed), hbox_num_bars);

    GtkWidget *num_bars_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (num_bars_label),"Number of bars:");
    gtk_widget_show (num_bars_label);
    gtk_box_pack_start (GTK_BOX (hbox_num_bars), num_bars_label, FALSE, TRUE, 0);

    GtkWidget *num_bars = gtk_spin_button_new_with_range (2,2000,1);
    gtk_widget_show (num_bars);
    gtk_box_pack_start (GTK_BOX (hbox_num_bars), num_bars, TRUE, TRUE, 0);

    GtkWidget *bar_mode = gtk_check_button_new_with_label ("Bar mode");
    gtk_widget_show (bar_mode);
    gtk_box_pack_start (GTK_BOX (vbox_draw_style_bars), bar_mode, FALSE, FALSE, 0);

    GtkWidget *gap_between_bars = gtk_check_button_new_with_label ("Gap between bars");
    gtk_widget_show (gap_between_bars);
    gtk_box_pack_start (GTK_BOX (vbox_draw_style_bars), gap_between_bars, FALSE, FALSE, 0);

    vbox_draw_style_solid = gtk_vbox_new (FALSE, 8);
    if (CONFIG_DRAW_STYLE) {
        gtk_widget_show (vbox_draw_style_solid);
    }
    gtk_container_add (GTK_CONTAINER (vbox_draw_styles), vbox_draw_style_solid);
    gtk_container_set_border_width (GTK_CONTAINER (vbox_draw_style_solid), 12);

    GtkWidget *fill_spectrum = gtk_check_button_new_with_label ("Fill spectrum");
    gtk_widget_show (fill_spectrum);
    gtk_box_pack_start (GTK_BOX (vbox_draw_style_solid), fill_spectrum, FALSE, FALSE, 0);

    GtkWidget *hbox05 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox05);
    gtk_box_pack_start (GTK_BOX (vbox07), hbox05, FALSE, FALSE, 0);

    GtkWidget *gradient_orientation_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (gradient_orientation_label),"Gradient orientation:");
    gtk_widget_show (gradient_orientation_label);
    gtk_box_pack_start (GTK_BOX (hbox05), gradient_orientation_label, FALSE, TRUE, 0);

    GtkWidget *gradient_orientation = gtk_combo_box_text_new ();
    gtk_widget_show (gradient_orientation);
    gtk_box_pack_start (GTK_BOX (hbox05), gradient_orientation, TRUE, TRUE, 0);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(gradient_orientation), STR_GRADIENT_VERTICAL);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gradient_orientation), STR_GRADIENT_HORIZONTAL);

    GtkWidget *hbox06 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox06);
    gtk_box_pack_start (GTK_BOX (vbox07), hbox06, FALSE, FALSE, 0);

    GtkWidget *alignment_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (alignment_label),"Alignment:");
    gtk_widget_show (alignment_label);
    gtk_box_pack_start (GTK_BOX (hbox06), alignment_label, FALSE, TRUE, 0);

    GtkWidget *alignment = gtk_combo_box_text_new ();
    gtk_widget_show (alignment);
    gtk_box_pack_start (GTK_BOX (hbox06), alignment, TRUE, TRUE, 0);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(alignment), STR_ALIGNMENT_LEFT);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(alignment), STR_ALIGNMENT_RIGHT);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(alignment), STR_ALIGNMENT_CENTER);

    GtkWidget *peaks = gtk_check_button_new_with_label ("Show peaks");
    gtk_widget_show (peaks);
    gtk_box_pack_start (GTK_BOX (vbox07), peaks, FALSE, FALSE, 0);

    GtkWidget *label_l = gtk_check_button_new_with_label ("Show left labels");
    gtk_widget_show (label_l);
    gtk_box_pack_start (GTK_BOX (vbox07), label_l, FALSE, FALSE, 0);

    GtkWidget *label_r = gtk_check_button_new_with_label ("Show right labels");
    gtk_widget_show (label_r);
    gtk_box_pack_start (GTK_BOX (vbox07), label_r, FALSE, FALSE, 0);

    GtkWidget *label_t = gtk_check_button_new_with_label ("Show top labels");
    gtk_widget_show (label_t);
    gtk_box_pack_start (GTK_BOX (vbox07), label_t, FALSE, FALSE, 0);

    GtkWidget *label_b = gtk_check_button_new_with_label ("Show bottom labels");
    gtk_widget_show (label_b);
    gtk_box_pack_start (GTK_BOX (vbox07), label_b, FALSE, FALSE, 0);

    GtkWidget *hgrid = gtk_check_button_new_with_label ("Horizontal grid");
    gtk_widget_show (hgrid);
    gtk_box_pack_start (GTK_BOX (vbox07), hgrid, FALSE, FALSE, 0);

    GtkWidget *vgrid = gtk_check_button_new_with_label ("Vertical grid");
    gtk_widget_show (vgrid);
    gtk_box_pack_start (GTK_BOX (vbox07), vgrid, FALSE, FALSE, 0);

    GtkWidget *ogrid = gtk_check_button_new_with_label ("Octave grid");
    gtk_widget_show (ogrid);
    gtk_box_pack_start (GTK_BOX (vbox07), ogrid, FALSE, FALSE, 0);

    GtkWidget *display_octaves = gtk_check_button_new_with_label ("Highlight octaves on hover");
    gtk_widget_show (display_octaves);
    gtk_box_pack_start (GTK_BOX (vbox07), display_octaves, FALSE, FALSE, 0);

    GtkWidget *tooltip = gtk_check_button_new_with_label ("Show tooltips");
    gtk_widget_show (tooltip);
    gtk_box_pack_start (GTK_BOX (vbox07), tooltip, FALSE, FALSE, 0);

    GtkWidget *dialog_action_area13 = gtk_dialog_get_action_area (GTK_DIALOG (spectrum_properties));
    gtk_widget_show (dialog_action_area13);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area13), GTK_BUTTONBOX_END);

    GtkWidget *applybutton1 = gtk_button_new_from_stock ("gtk-apply");
    gtk_widget_show (applybutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), applybutton1, GTK_RESPONSE_APPLY);
    gtk_widget_set_can_default (applybutton1, TRUE);

    GtkWidget *cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
    gtk_widget_show (cancelbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), cancelbutton1, GTK_RESPONSE_CANCEL);
    gtk_widget_set_can_default (cancelbutton1, TRUE);

    GtkWidget *okbutton1 = gtk_button_new_from_stock ("gtk-ok");
    gtk_widget_show (okbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), okbutton1, GTK_RESPONSE_OK);
    gtk_widget_set_can_default (okbutton1, TRUE);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_b), CONFIG_ENABLE_BOTTOM_LABELS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_t), CONFIG_ENABLE_TOP_LABELS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_l), CONFIG_ENABLE_LEFT_LABELS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_r), CONFIG_ENABLE_RIGHT_LABELS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (peaks), CONFIG_ENABLE_PEAKS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (hgrid), CONFIG_ENABLE_HGRID);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vgrid), CONFIG_ENABLE_VGRID);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ogrid), CONFIG_ENABLE_OCTAVE_GRID);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tooltip), CONFIG_ENABLE_TOOLTIP);

    switch (CONFIG_DRAW_STYLE) {
    case 0:
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (draw_style_bars_radio), TRUE);
        break;
    case 1:
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (draw_style_solid_radio), TRUE);
        break;
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar_mode), CONFIG_ENABLE_BAR_MODE);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (num_bars), CONFIG_NUM_BARS);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (bar_width), CONFIG_BAR_W);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gap_between_bars), CONFIG_GAPS);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fill_spectrum), CONFIG_FILL_SPECTRUM);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (display_octaves), CONFIG_DISPLAY_OCTAVES);
    gtk_combo_box_set_active (GTK_COMBO_BOX (window), CONFIG_WINDOW);
    gtk_combo_box_set_active (GTK_COMBO_BOX (fft), FFT_INDEX);
    gtk_combo_box_set_active (GTK_COMBO_BOX (gradient_orientation), CONFIG_GRADIENT_ORIENTATION);
    gtk_combo_box_set_active (GTK_COMBO_BOX (alignment), CONFIG_ALIGNMENT);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (num_colors), CONFIG_NUM_COLORS);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (db_range), CONFIG_DB_RANGE);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_bg), &CONFIG_COLOR_BG);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_vgrid), &CONFIG_COLOR_VGRID);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_hgrid), &CONFIG_COLOR_HGRID);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_ogrid), &CONFIG_COLOR_OCTAVE_GRID);

    char text[100];
    for (;;) {
        int response = gtk_dialog_run (GTK_DIALOG (spectrum_properties));
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_bg), &CONFIG_COLOR_BG);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_vgrid), &CONFIG_COLOR_VGRID);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_hgrid), &CONFIG_COLOR_HGRID);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_ogrid), &CONFIG_COLOR_OCTAVE_GRID);
            for (int i = 0; i < CONFIG_NUM_COLORS; i++) {
                gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradients[i]), &CONFIG_GRADIENT_COLORS[i]);
            }
            CONFIG_ENABLE_BOTTOM_LABELS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_b));
            CONFIG_ENABLE_TOP_LABELS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_t));
            CONFIG_ENABLE_RIGHT_LABELS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_r));
            CONFIG_ENABLE_LEFT_LABELS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_l));
            CONFIG_ENABLE_PEAKS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peaks));
            CONFIG_ENABLE_HGRID = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hgrid));
            CONFIG_ENABLE_VGRID = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vgrid));
            CONFIG_ENABLE_OCTAVE_GRID = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ogrid));
            CONFIG_ENABLE_TOOLTIP = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tooltip));
            CONFIG_ENABLE_BAR_MODE = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bar_mode));
            CONFIG_GAPS = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gap_between_bars));
            CONFIG_FILL_SPECTRUM = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (fill_spectrum));
            CONFIG_NUM_BARS = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (num_bars));
            CONFIG_BAR_W = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (bar_width));
            CONFIG_DISPLAY_OCTAVES = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (display_octaves));
            CONFIG_DB_RANGE = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (db_range));
            CONFIG_NUM_COLORS = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (num_colors));

            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (draw_style_bars_radio)) == TRUE) {
                CONFIG_DRAW_STYLE = 0;
            }
            else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (draw_style_solid_radio)) == TRUE) {
                CONFIG_DRAW_STYLE = 1;
            }

            for (int i = 0; i < MAX_NUM_COLORS && color_gradients[i]; i++) {
                if (i < CONFIG_NUM_COLORS) {
                    gtk_widget_show (color_gradients[i]);
                }
                else if (color_gradients[i]) {
                    gtk_widget_hide (color_gradients[i]);
                }
            }

            snprintf (text, sizeof (text), "%s", gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (gradient_orientation)));
            if (strcmp (text, STR_GRADIENT_VERTICAL) == 0) {
                CONFIG_GRADIENT_ORIENTATION = 0;
            }
            else if (strcmp (text, STR_GRADIENT_HORIZONTAL) == 0) {
                CONFIG_GRADIENT_ORIENTATION = 1;
            }
            else {
                CONFIG_GRADIENT_ORIENTATION = -1;
            }

            snprintf (text, sizeof (text), "%s", gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (alignment)));
            if (strcmp (text, STR_ALIGNMENT_LEFT) == 0) {
                CONFIG_ALIGNMENT = LEFT;
            }
            else if (strcmp (text, STR_ALIGNMENT_RIGHT) == 0) {
                CONFIG_ALIGNMENT = RIGHT;
            }
            else if (strcmp (text, STR_ALIGNMENT_CENTER) == 0) {
                CONFIG_ALIGNMENT = CENTER;
            }
            else {
                CONFIG_ALIGNMENT = -1;
            }

            snprintf (text, sizeof (text), "%s", gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (window)));
            if (strcmp (text, STR_WINDOW_BLACKMANN_HARRIS) == 0) {
                CONFIG_WINDOW = BLACKMAN_HARRIS;
            }
            else if (strcmp (text, STR_WINDOW_HANNING) == 0) {
                CONFIG_WINDOW = HANNING;
            }
            else {
                CONFIG_GRADIENT_ORIENTATION = -1;
            }

            snprintf (text, sizeof (text), "%s", gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (fft)));
            for (int i = 0; i < fft_sizses_size; i++) {
                if (strcmp (text, fft_sizes[i]) == 0) {
                    FFT_INDEX = i;
                    CONFIG_FFT_SIZE = (int)exp2 (FFT_INDEX + 9);
                }
            }

            save_config ();
            deadbeef->sendmessage (DB_EV_CONFIGCHANGED, 0, 0, 0);
        }
        if (response == GTK_RESPONSE_APPLY) {
            continue;
        }
        break;
    }
    gtk_widget_destroy (spectrum_properties);
#pragma GCC diagnostic pop
    return;
}

