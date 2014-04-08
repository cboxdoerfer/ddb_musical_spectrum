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
#include <fftw3.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "fastftoi.h"

#define MAX_BANDS 126
#define VIS_DELAY 1
#define VIS_DELAY_PEAK 20
#define VIS_FALLOFF 60
#define VIS_FALLOFF_PEAK 3
#define BAND_WIDTH 5
//#define FFT_SIZE 16384
#define GRADIENT_TABLE_SIZE 1024
//#define FFT_SIZE 32768
#define FFT_SIZE 8192

#define     STR_GRADIENT_VERTICAL "Vertical"
#define     STR_GRADIENT_HORIZONTAL "Horizontal"

#define     CONFSTR_MS_GRADIENT_ORIENTATION   "musical_spectrum.gradient_orientation"
#define     CONFSTR_MS_COLOR_BG               "musical_spectrum.color.background"
#define     CONFSTR_MS_COLOR_GRADIENT_00      "musical_spectrum.color.gradient_00"
#define     CONFSTR_MS_COLOR_GRADIENT_01      "musical_spectrum.color.gradient_01"
#define     CONFSTR_MS_COLOR_GRADIENT_02      "musical_spectrum.color.gradient_02"
#define     CONFSTR_MS_COLOR_GRADIENT_03      "musical_spectrum.color.gradient_03"
#define     CONFSTR_MS_COLOR_GRADIENT_04      "musical_spectrum.color.gradient_04"
#define     CONFSTR_MS_COLOR_GRADIENT_05      "musical_spectrum.color.gradient_05"

/* Global variables */
static DB_misc_t            plugin;
static DB_functions_t *     deadbeef = NULL;
static ddb_gtkui_t *        gtkui_plugin = NULL;

typedef struct {
    ddb_gtkui_widget_t base;
    GtkWidget *drawarea;
    GtkWidget *popup;
    GtkWidget *popup_item;
    guint drawtimer;
    double *data;
    double window[FFT_SIZE];
    // keys: index of frequencies of musical notes (c0;d0;...;f10) in data
    int keys[MAX_BANDS];
    uint32_t colors[GRADIENT_TABLE_SIZE];
    double *samples;
    double *in;
    //double *out_real;
    fftw_complex *out_complex;
    //fftw_plan p_r2r;
    fftw_plan p_r2c;
    int resized;
    int buffered;
    int low_res_end;
    int bars[MAX_BANDS + 1];
    int delay[MAX_BANDS + 1];
    int peaks[MAX_BANDS + 1];
    int delay_peak[MAX_BANDS + 1];
    intptr_t mutex;
    intptr_t mutex_keys;
    cairo_surface_t *surf;
} w_spectrum_t;

static int CONFIG_GRADIENT_ORIENTATION = 0;
static int CONFIG_NUM_COLORS = 6;
static GdkColor CONFIG_COLOR_BG;
static GdkColor CONFIG_GRADIENT_COLORS[6];

static void
save_config (void)
{
    deadbeef->conf_set_int (CONFSTR_MS_GRADIENT_ORIENTATION,        CONFIG_GRADIENT_ORIENTATION);
    char color[100];
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_COLOR_BG.red, CONFIG_COLOR_BG.green, CONFIG_COLOR_BG.blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_BG, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[0].red, CONFIG_GRADIENT_COLORS[0].green, CONFIG_GRADIENT_COLORS[0].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_00, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[1].red, CONFIG_GRADIENT_COLORS[1].green, CONFIG_GRADIENT_COLORS[1].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_01, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[2].red, CONFIG_GRADIENT_COLORS[2].green, CONFIG_GRADIENT_COLORS[2].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_02, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[3].red, CONFIG_GRADIENT_COLORS[3].green, CONFIG_GRADIENT_COLORS[3].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_03, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[4].red, CONFIG_GRADIENT_COLORS[4].green, CONFIG_GRADIENT_COLORS[4].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_04, color);
    snprintf (color, sizeof (color), "%d %d %d", CONFIG_GRADIENT_COLORS[5].red, CONFIG_GRADIENT_COLORS[5].green, CONFIG_GRADIENT_COLORS[5].blue);
    deadbeef->conf_set_str (CONFSTR_MS_COLOR_GRADIENT_05, color);
}

static void
load_config (void)
{
    deadbeef->conf_lock ();
    CONFIG_GRADIENT_ORIENTATION = deadbeef->conf_get_int (CONFSTR_MS_GRADIENT_ORIENTATION,          0);
    const char *color;
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_BG,                            "0 0 0");
    sscanf (color, "%hd %hd %hd", &CONFIG_COLOR_BG.red, &CONFIG_COLOR_BG.green, &CONFIG_COLOR_BG.blue);
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_00,        "65535 0 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[0].red), &(CONFIG_GRADIENT_COLORS[0].green), &(CONFIG_GRADIENT_COLORS[0].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_01,      "65535 32896 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[1].red), &(CONFIG_GRADIENT_COLORS[1].green), &(CONFIG_GRADIENT_COLORS[1].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_02,      "65535 65535 0");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[2].red), &(CONFIG_GRADIENT_COLORS[2].green), &(CONFIG_GRADIENT_COLORS[2].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_03,    "32896 65535 30840");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[3].red), &(CONFIG_GRADIENT_COLORS[3].green), &(CONFIG_GRADIENT_COLORS[3].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_04,      "0 38036 41120");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[4].red), &(CONFIG_GRADIENT_COLORS[4].green), &(CONFIG_GRADIENT_COLORS[4].blue));
    color = deadbeef->conf_get_str_fast (CONFSTR_MS_COLOR_GRADIENT_05,       "0 8224 25700");
    sscanf (color, "%hd %hd %hd", &(CONFIG_GRADIENT_COLORS[5].red), &(CONFIG_GRADIENT_COLORS[5].green), &(CONFIG_GRADIENT_COLORS[5].blue));
    deadbeef->conf_unlock ();
}

void
do_fft (w_spectrum_t *w)
{
    if (!w->samples || w->buffered < FFT_SIZE) {
        return;
    }
    deadbeef->mutex_lock (w->mutex);
    double real,imag;

    for (int i = 0; i < FFT_SIZE; i++) {
        w->in[i] = (double)w->samples[i] * w->window[i];
    }
    deadbeef->mutex_unlock (w->mutex);
    //fftw_execute (p_r2r);
    fftw_execute (w->p_r2c);
    for (int i = 0; i < FFT_SIZE/2; i++)
    {
        real = w->out_complex[i][0];
        imag = w->out_complex[i][1];
        w->data[i] = (real*real + imag*imag);
        //w->data[i] = out_real[i]*out_real[i] + out_real[FFT_SIZE-i-1]*out_real[FFT_SIZE-i-1];
    }
}

static inline void
_draw_vline (uint8_t *data, int stride, int x0, int y0, int y1) {
    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
        y1--;
    }
    else if (y0 < y1) {
        y0++;
    }
    while (y0 <= y1) {
        uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
        *ptr = 0xffffffff;
        y0++;
    }
}

static inline void
_draw_hline (uint8_t *data, int stride, int x0, int y0, int x1) {
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (x0 <= x1) {
        *ptr++ = 0xff666666;
        x0++;
    }
}

static inline void
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

static inline void
_draw_bar_gradient_v (gpointer user_data, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h) {
    w_spectrum_t *s = user_data;
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        int index = ftoi(((double)y0/(double)total_h) * (GRADIENT_TABLE_SIZE - 1));
        index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
        while (x++ <= x1) {
            *ptr++ = s->colors[index];
        }
        y0++;
        ptr += stride/4-w;
    }
}

static inline void
_draw_bar_gradient_h (gpointer user_data, uint8_t *data, int stride, int x0, int y0, int w, int h, int total_w) {
    w_spectrum_t *s = user_data;
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        while (x++ <= x1) {
            int index = ftoi(((double)x/(double)total_w) * (GRADIENT_TABLE_SIZE - 1));
            index = CLAMP (index, 0, GRADIENT_TABLE_SIZE - 1);
            *ptr++ = s->colors[index];
        }
        y0++;
        ptr += stride/4-w;
    }
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
            w->colors[i] = ((uint32_t)(colors[0].red*scale) & 0xFF) << 16 |
                ((uint32_t)(colors[0].green*scale) & 0xFF) << 8 |
                ((uint32_t)(colors[0].blue*scale) & 0xFF) << 0;
        }
        else if (n < num_colors) {
            w->colors[i] = ((uint32_t)((colors[n].red*scale) + f * ((colors[n+1].red*scale)-(colors[n].red*scale))) & 0xFF) << 16 |
                ((uint32_t)((colors[n].green*scale) + f * ((colors[n+1].green*scale)-(colors[n].green*scale))) & 0xFF) << 8 |
                ((uint32_t)((colors[n].blue*scale) + f * ((colors[n+1].blue*scale)-(colors[n].blue*scale))) & 0xFF) << 0;
        }
        else if (n == num_colors) {
            w->colors[i] = ((uint32_t)(colors[n].red*scale) & 0xFF) << 16 |
                ((uint32_t)(colors[n].green*scale) & 0xFF) << 8 |
                ((uint32_t)(colors[n].blue*scale) & 0xFF) << 0;
        }
        else {
            w->colors[i] = 0xFFFFFFFF;
        }
    }
}

static int
on_config_changed (gpointer user_data, uintptr_t ctx)
{
    create_gradient_table (user_data, CONFIG_GRADIENT_COLORS, CONFIG_NUM_COLORS);
    load_config ();
    return 0;
}

#if !GTK_CHECK_VERSION(2,12,0)
#define gtk_widget_get_window(widget) ((widget)->window)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#define gtk_dialog_get_action_area(dialog) (dialog->action_area)
#endif

#if !GTK_CHECK_VERSION(2,18,0)
void
gtk_widget_get_allocation (GtkWidget *widget, GtkAllocation *allocation) {
    (allocation)->x = widget->allocation.x;
    (allocation)->y = widget->allocation.y;
    (allocation)->width = widget->allocation.width;
    (allocation)->height = widget->allocation.height;
}
#define gtk_widget_set_can_default(widget, candefault) {if (candefault) GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT); else GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);}
#endif

static void
on_button_config (GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *spectrum_properties;
    GtkWidget *config_dialog;
    GtkWidget *vbox01;
    GtkWidget *vbox02;
    GtkWidget *hbox01;
    GtkWidget *hbox02;
    GtkWidget *color_label;
    GtkWidget *color_frame;
    GtkWidget *color_bg;
    GtkWidget *color_gradient_00;
    GtkWidget *color_gradient_01;
    GtkWidget *color_gradient_02;
    GtkWidget *color_gradient_03;
    GtkWidget *color_gradient_04;
    GtkWidget *color_gradient_05;
    GtkWidget *num_colors_label;
    GtkWidget *num_colors;
    GtkWidget *gradient_orientation;
    GtkWidget *dialog_action_area13;
    GtkWidget *applybutton1;
    GtkWidget *cancelbutton1;
    GtkWidget *okbutton1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    spectrum_properties = gtk_dialog_new ();
    gtk_widget_set_size_request (spectrum_properties, -1, 450);
    gtk_window_set_title (GTK_WINDOW (spectrum_properties), "Spectrum Properties");
    gtk_window_set_type_hint (GTK_WINDOW (spectrum_properties), GDK_WINDOW_TYPE_HINT_DIALOG);

    config_dialog = gtk_dialog_get_content_area (GTK_DIALOG (spectrum_properties));
    gtk_widget_show (config_dialog);

    hbox01 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox01);
    gtk_box_pack_start (GTK_BOX (config_dialog), hbox01, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox01), 12);

    color_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (color_label),"<b>Colors</b>");
    gtk_widget_show (color_label);

    color_frame = gtk_frame_new ("Colors");
    gtk_frame_set_label_widget ((GtkFrame *)color_frame, color_label);
    gtk_frame_set_shadow_type ((GtkFrame *)color_frame, GTK_SHADOW_IN);
    gtk_widget_show (color_frame);
    gtk_box_pack_start (GTK_BOX (hbox01), color_frame, TRUE, FALSE, 0);

    vbox02 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox02);
    gtk_container_add (GTK_CONTAINER (color_frame), vbox02);
    gtk_container_set_border_width (GTK_CONTAINER (vbox02), 12);

    color_bg = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_bg, TRUE);
    gtk_widget_show (color_bg);
    gtk_box_pack_start (GTK_BOX (vbox02), color_bg, TRUE, FALSE, 0);

    num_colors_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (num_colors_label),"Number of colors:");
    gtk_widget_show (num_colors_label);
    gtk_box_pack_start (GTK_BOX (vbox02), num_colors_label, FALSE, FALSE, 0);

    num_colors = gtk_spin_button_new_with_range (1,6,1);
    gtk_widget_show (num_colors);
    gtk_box_pack_start (GTK_BOX (vbox02), num_colors, FALSE, FALSE, 0);

    color_gradient_00 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_00, TRUE);
    gtk_widget_show (color_gradient_00);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_00, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_00, -1, 30);

    color_gradient_01 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_01, TRUE);
    gtk_widget_show (color_gradient_01);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_01, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_01, -1, 30);

    color_gradient_02 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_02, TRUE);
    gtk_widget_show (color_gradient_02);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_02, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_02, -1, 30);

    color_gradient_03 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_03, TRUE);
    gtk_widget_show (color_gradient_03);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_03, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_03, -1, 30);

    color_gradient_04 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_04, TRUE);
    gtk_widget_show (color_gradient_04);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_04, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_04, -1, 30);

    color_gradient_05 = gtk_color_button_new ();
    gtk_color_button_set_use_alpha ((GtkColorButton *)color_gradient_05, TRUE);
    gtk_widget_show (color_gradient_05);
    gtk_box_pack_start (GTK_BOX (vbox02), color_gradient_05, TRUE, FALSE, 0);
    gtk_widget_set_size_request (color_gradient_05, -1, 30);

    vbox01 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox01);
    gtk_box_pack_start (GTK_BOX (hbox01), vbox01, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox01), 12);

    hbox02 = gtk_hbox_new (FALSE, 8);
    gtk_widget_show (hbox02);
    gtk_box_pack_start (GTK_BOX (vbox01), hbox02, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox01), 12);

    gradient_orientation = gtk_combo_box_text_new ();
    gtk_widget_show (gradient_orientation);
    gtk_box_pack_start (GTK_BOX (vbox01), gradient_orientation, FALSE, FALSE, 0);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(gradient_orientation), STR_GRADIENT_VERTICAL);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gradient_orientation), STR_GRADIENT_HORIZONTAL);

    dialog_action_area13 = gtk_dialog_get_action_area (GTK_DIALOG (spectrum_properties));
    gtk_widget_show (dialog_action_area13);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area13), GTK_BUTTONBOX_END);

    applybutton1 = gtk_button_new_from_stock ("gtk-apply");
    gtk_widget_show (applybutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), applybutton1, GTK_RESPONSE_APPLY);
    gtk_widget_set_can_default (applybutton1, TRUE);

    cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
    gtk_widget_show (cancelbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), cancelbutton1, GTK_RESPONSE_CANCEL);
    gtk_widget_set_can_default (cancelbutton1, TRUE);

    okbutton1 = gtk_button_new_from_stock ("gtk-ok");
    gtk_widget_show (okbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (spectrum_properties), okbutton1, GTK_RESPONSE_OK);
    gtk_widget_set_can_default (okbutton1, TRUE);

    gtk_combo_box_set_active (GTK_COMBO_BOX (gradient_orientation), CONFIG_GRADIENT_ORIENTATION);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (num_colors), CONFIG_NUM_COLORS);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_bg), &CONFIG_COLOR_BG);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_00), &(CONFIG_GRADIENT_COLORS[0]));
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_01), &(CONFIG_GRADIENT_COLORS[1]));
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_02), &(CONFIG_GRADIENT_COLORS[2]));
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_03), &(CONFIG_GRADIENT_COLORS[3]));
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_04), &(CONFIG_GRADIENT_COLORS[4]));
    gtk_color_button_set_color (GTK_COLOR_BUTTON (color_gradient_05), &(CONFIG_GRADIENT_COLORS[5]));

    char *text;
    for (;;) {
        int response = gtk_dialog_run (GTK_DIALOG (spectrum_properties));
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_bg), &CONFIG_COLOR_BG);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_00), &CONFIG_GRADIENT_COLORS[0]);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_01), &CONFIG_GRADIENT_COLORS[1]);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_02), &CONFIG_GRADIENT_COLORS[2]);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_03), &CONFIG_GRADIENT_COLORS[3]);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_04), &CONFIG_GRADIENT_COLORS[4]);
            gtk_color_button_get_color (GTK_COLOR_BUTTON (color_gradient_05), &CONFIG_GRADIENT_COLORS[5]);

            CONFIG_NUM_COLORS = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (num_colors));
            switch (CONFIG_NUM_COLORS) {
                case 1:
                    gtk_widget_hide (color_gradient_01);
                    gtk_widget_hide (color_gradient_02);
                    gtk_widget_hide (color_gradient_03);
                    gtk_widget_hide (color_gradient_04);
                    gtk_widget_hide (color_gradient_05);
                    break;
                case 2:
                    gtk_widget_show (color_gradient_01);
                    gtk_widget_hide (color_gradient_02);
                    gtk_widget_hide (color_gradient_03);
                    gtk_widget_hide (color_gradient_04);
                    gtk_widget_hide (color_gradient_05);
                    break;
                case 3:
                    gtk_widget_show (color_gradient_01);
                    gtk_widget_show (color_gradient_02);
                    gtk_widget_hide (color_gradient_03);
                    gtk_widget_hide (color_gradient_04);
                    gtk_widget_hide (color_gradient_05);
                    break;
                case 4:
                    gtk_widget_show (color_gradient_01);
                    gtk_widget_show (color_gradient_02);
                    gtk_widget_show (color_gradient_03);
                    gtk_widget_hide (color_gradient_04);
                    gtk_widget_hide (color_gradient_05);
                    break;
                case 5:
                    gtk_widget_show (color_gradient_01);
                    gtk_widget_show (color_gradient_02);
                    gtk_widget_show (color_gradient_03);
                    gtk_widget_show (color_gradient_04);
                    gtk_widget_hide (color_gradient_05);
                    break;
                case 6:
                    gtk_widget_show (color_gradient_01);
                    gtk_widget_show (color_gradient_02);
                    gtk_widget_show (color_gradient_03);
                    gtk_widget_show (color_gradient_04);
                    gtk_widget_show (color_gradient_05);
                    break;
            }
            text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (gradient_orientation));
            if (strcmp (text, STR_GRADIENT_VERTICAL) == 0) {
                CONFIG_GRADIENT_ORIENTATION = 0;
            }
            else if (strcmp (text, STR_GRADIENT_HORIZONTAL) == 0) {
                CONFIG_GRADIENT_ORIENTATION = 1;
            }
            else {
                CONFIG_GRADIENT_ORIENTATION = -1;
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

///// spectrum vis
void
w_spectrum_destroy (ddb_gtkui_widget_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    deadbeef->vis_waveform_unlisten (w);
    if (s->data) {
        free (s->data);
        s->data = NULL;
    }
    if (s->samples) {
        free (s->samples);
        s->samples = NULL;
    }
    //if (s->p_r2r) {
    //    fftw_destroy_plan (s->p_r2r);
    //}
    if (s->p_r2c) {
        fftw_destroy_plan (s->p_r2c);
    }
    if (s->in) {
        fftw_free (s->in);
        s->in = NULL;
    }
    //if (s->out_real) {
    //    fftw_free (s->out_real);
    //    s->out_real = NULL;
    //}
    if (s->out_complex) {
        fftw_free (s->out_complex);
        s->out_complex = NULL;
    }
    if (s->drawtimer) {
        g_source_remove (s->drawtimer);
        s->drawtimer = 0;
    }
    if (s->surf) {
        cairo_surface_destroy (s->surf);
        s->surf = NULL;
    }
    if (s->mutex) {
        deadbeef->mutex_free (s->mutex);
        s->mutex = 0;
    }
    if (s->mutex_keys) {
        deadbeef->mutex_free (s->mutex_keys);
        s->mutex_keys = 0;
    }
}

gboolean
w_spectrum_draw_cb (void *data) {
    w_spectrum_t *s = data;
    gtk_widget_queue_draw (s->drawarea);
    return TRUE;
}

static void
spectrum_wavedata_listener (void *ctx, ddb_audio_data_t *data) {
    w_spectrum_t *w = ctx;
    if (!w->samples) {
        return;
    }
    deadbeef->mutex_lock (w->mutex);
    int nsamples = data->nframes;
    int sz = MIN (FFT_SIZE, nsamples);
    int n = FFT_SIZE - sz;
    memmove (w->samples, w->samples + sz, n * sizeof (double));

    int pos = 0;
    for (int i = 0; i < sz && pos < nsamples; i++, pos++ ) {
        w->samples[n+i] = -1000.0;
        for (int j = 0; j < data->fmt->channels; j++) {
            w->samples[n + i] = MAX (w->samples[n + i], data->data[ftoi (pos * data->fmt->channels) + j]);
        }
    }
    deadbeef->mutex_unlock (w->mutex);
    if (w->buffered < FFT_SIZE) {
        w->buffered += sz;
    }
}

static inline float
cosine_interpolate (float y1, float y2, float mu)
{
    float mu2;

    mu2 = (1 - cos (mu * M_PI))/2;
    return (y1 * (1 - mu2) + y2 * mu2);
}

static inline float
spectrum_get_value (gpointer user_data, int start, int end)
{
    w_spectrum_t *w = user_data;
    if (start >= end) {
        return w->data[end];
    }
    float value = 0.0;
    for (int i = start; i < end; i++) {
        value = MAX (w->data[i],value);
    }
    return value;
}

static gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;
    if (!w->samples) {
        return FALSE;
    }
    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);

    do_fft (w);
    int width, height, bands;
    bands = MAX_BANDS;
    width = a.width;
    height = a.height;

    deadbeef->mutex_lock (w->mutex_keys);
    if (deadbeef->get_output ()->state () == OUTPUT_STATE_PLAYING) {
        for (int i = 0; i < bands; i++)
        {
            float f;
            int start = 0;
            int end = 0;
            if (i > 0) {
                start = (w->keys[i] - w->keys[i-1])/2 + w->keys[i-1];
                if (start == w->keys[i-1]) start = w->keys[i];
            }
            else {
                start = w->keys[i];
            }
            if (i < bands-1) {
                end = (w->keys[i+1] - w->keys[i])/2 + w->keys[i];
                if (end == w->keys[i+1]) end = w->keys[i];
            }
            else {
                end = w->keys[i];
            }

            f = spectrum_get_value (w, start, end);
            int x = 10 * log10 (f);

            // interpolate
            if (i <= w->low_res_end+1) {
                int j = 0;
                // find index of next value
                while (i+j < MAX_BANDS && w->keys[i+j] == w->keys[i]) {
                    j++;
                }
                float v0 = x;
                float v1 = 10 * log10 (w->data[w->keys[i+j]]);

                int k = 0;
                while ((k+i) >= 0 && w->keys[k+i] == w->keys[i]) {
                    j++;
                    k--;
                }
                x = ftoi (cosine_interpolate (v0,v1,(1.0/(j-1)) * ((-1) * (k+1))));
            }

            // TODO: get rid of hardcoding
            x += 7;
            x = CLAMP (x, 0, 70);

            //w->bars[i] -= MAX (0, VIS_FALLOFF - w->delay[i]);
            w->bars[i] = 0;
            w->peaks[i] -= MAX (0, VIS_FALLOFF_PEAK - w->delay_peak[i]);;

            if (w->delay[i])
                w->delay[i]--;
            if (w->delay_peak[i])
                w->delay_peak[i]--;

            if (x > w->bars[i])
            {
                w->bars[i] = x;
                w->delay[i] = VIS_DELAY;
            }
            if (x > w->peaks[i]) {
                w->peaks[i] = x;
                w->delay_peak[i] = VIS_DELAY_PEAK;
            }
            if (w->peaks[i] < w->bars[i]) {
                w->peaks[i] = w->bars[i];
            }
        }
    }
    deadbeef->mutex_unlock (w->mutex_keys);

    // start drawing
    if (!w->surf || cairo_image_surface_get_width (w->surf) != a.width || cairo_image_surface_get_height (w->surf) != a.height) {
        if (w->surf) {
            cairo_surface_destroy (w->surf);
            w->surf = NULL;
        }
        w->surf = cairo_image_surface_create (CAIRO_FORMAT_RGB24, a.width, a.height);
    }
    float base_s = (height / 70.f);

    cairo_surface_flush (w->surf);

    unsigned char *data = cairo_image_surface_get_data (w->surf);
    if (!data) {
        return FALSE;
    }
    int stride = cairo_image_surface_get_stride (w->surf);
    memset (data, 0, a.height * stride);

    int barw = CLAMP (width / bands, 2, 20);

    //draw background
    _draw_bar (data, stride, 0, 0, a.width, a.height, 0xff222222);
    for (int i = 0; i <= bands; i++) {
        // draw vertical grid
        int x = barw * i;
        if (x < a.width) {
            _draw_bar (data, stride, x, 0, 1, height-1, 0xff000000);
        }
    }

    // draw horizontal grid
    for (int i = 1; i < 7; i++) {
        if (a.height <= 10 || a.width <= 10) {
            break;
        }
        _draw_hline (data, stride, 1, ftoi (i/7.0 * (a.height)), a.width);
    }

    for (gint i = 0; i <= bands; i++)
    {
        int x = barw * i;
        int y = a.height - w->bars[i] * base_s;
        if (y < 0) {
            y = 0;
        }
        int bw = barw-1;
        if (x + bw >= a.width) {
            bw = a.width-x-1;
        }
        if (CONFIG_GRADIENT_ORIENTATION == 0) {
            _draw_bar_gradient_v (user_data, data, stride, x+1, y, bw, a.height-y, a.height);
        }
        else {
            _draw_bar_gradient_h (user_data, data, stride, x+1, y, bw, a.height-y, a.width);
        }
        y = a.height - w->peaks[i] * base_s;
        if (y < a.height-1) {
            if (CONFIG_GRADIENT_ORIENTATION == 0) {
                _draw_bar_gradient_v (user_data, data, stride, x + 1, y, bw, 1, a.height);
            }
            else {
                _draw_bar_gradient_h (user_data, data, stride, x + 1, y, bw, 1, a.width);
            }
        }
    }
    cairo_surface_mark_dirty (w->surf);

    cairo_save (cr);
    cairo_set_source_surface (cr, w->surf, 0, 0);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);
    cairo_restore (cr);

    return FALSE;
}


gboolean
spectrum_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gboolean res = spectrum_draw (widget, cr, user_data);
    cairo_destroy (cr);
    return res;
}


gboolean
spectrum_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    //w_spectrum_t *w = user_data;
    if (event->button == 3) {
      return TRUE;
    }
    return TRUE;
}

gboolean
spectrum_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    if (event->button == 3) {
      gtk_menu_popup (GTK_MENU (w->popup), NULL, NULL, NULL, w->drawarea, 0, gtk_get_current_event_time ());
      return TRUE;
    }
    return TRUE;
}

static int
spectrum_message (ddb_gtkui_widget_t *widget, uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2)
{
    w_spectrum_t *w = (w_spectrum_t *)widget;
    int samplerate = 44100;

    switch (id) {
        case DB_EV_SONGCHANGED:
            deadbeef->mutex_lock (w->mutex_keys);
            samplerate = deadbeef->get_output ()->fmt.samplerate;
            if (samplerate == 0) samplerate = 44100;
            for (int i = 0; i < MAX_BANDS; i++) {
                w->keys[i] = ftoi (440.0 * (pow (2.0, (double)(i-57)/12.0) * FFT_SIZE/(float)samplerate));
                if (i > 0 && w->keys[i-1] == w->keys[i])
                    w->low_res_end = i;
            }
            deadbeef->mutex_unlock (w->mutex_keys);
            break;
        case DB_EV_CONFIGCHANGED:
            on_config_changed (w, ctx);
            break;
    }
    return 0;
}

void
w_spectrum_init (ddb_gtkui_widget_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    deadbeef->mutex_lock (s->mutex);
    s->samples = malloc (sizeof (double) * FFT_SIZE);
    memset (s->samples, 0, sizeof (double) * FFT_SIZE);
    s->data = malloc (sizeof (double) * FFT_SIZE);
    memset (s->data, 0, sizeof (double) * FFT_SIZE);
    if (s->drawtimer) {
        g_source_remove (s->drawtimer);
        s->drawtimer = 0;
    }
    for (int i = 0; i < FFT_SIZE; i++) {
        // Hanning
        //s->window[i] = (0.5 * (1 - cos (2 * M_PI * i/(FFT_SIZE))));
        // Blackman-Harris
        s->window[i] = 0.35875 - 0.48829 * cos(2 * M_PI * i /(FFT_SIZE)) + 0.14128 * cos(4 * M_PI * i/(FFT_SIZE)) - 0.01168 * cos(6 * M_PI * i/(FFT_SIZE));;
    }
    s->low_res_end = 0;
    for (int i = 0; i < MAX_BANDS; i++) {
        s->keys[i] = ftoi (440.0 * (pow (2.0, (double)(i-57)/12.0) * FFT_SIZE/44100.0));
        if (i > 0 && s->keys[i-1] == s->keys[i])
            s->low_res_end = i;
    }
    create_gradient_table (s, CONFIG_GRADIENT_COLORS, CONFIG_NUM_COLORS);
    s->in = fftw_malloc (sizeof (double) * FFT_SIZE);
    memset (s->in, 0, sizeof (double) * FFT_SIZE);
    //s->out_real = fftw_malloc (sizeof (double) * FFT_SIZE);
    s->out_complex = fftw_malloc (sizeof (fftw_complex) * FFT_SIZE);
    //s->p_r2r = fftw_plan_r2r_1d (FFT_SIZE, s->in, s->out_real, FFTW_R2HC, FFTW_ESTIMATE);
    s->p_r2c = fftw_plan_dft_r2c_1d (FFT_SIZE, s->in, s->out_complex, FFTW_ESTIMATE);
    s->drawtimer = g_timeout_add (33, w_spectrum_draw_cb, w);
    deadbeef->mutex_unlock (s->mutex);
}

ddb_gtkui_widget_t *
w_musical_spectrum_create (void) {
    w_spectrum_t *w = malloc (sizeof (w_spectrum_t));
    memset (w, 0, sizeof (w_spectrum_t));

    w->base.widget = gtk_event_box_new ();
    w->base.init = w_spectrum_init;
    w->base.destroy  = w_spectrum_destroy;
    w->base.message = spectrum_message;
    w->drawarea = gtk_drawing_area_new ();
    w->popup = gtk_menu_new ();
    w->popup_item = gtk_menu_item_new_with_mnemonic ("Configure");
    w->mutex = deadbeef->mutex_create ();
    w->mutex_keys = deadbeef->mutex_create ();
    gtk_widget_show (w->drawarea);
    gtk_container_add (GTK_CONTAINER (w->base.widget), w->drawarea);
    gtk_widget_show (w->popup);
    //gtk_container_add (GTK_CONTAINER (w->drawarea), w->popup);
    gtk_widget_show (w->popup_item);
    gtk_container_add (GTK_CONTAINER (w->popup), w->popup_item);
#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) w->drawarea, "expose_event", G_CALLBACK (spectrum_expose_event), w);
#else
    g_signal_connect_after ((gpointer) w->drawarea, "draw", G_CALLBACK (spectrum_draw), w);
#endif
    g_signal_connect_after ((gpointer) w->base.widget, "button_press_event", G_CALLBACK (spectrum_button_press_event), w);
    g_signal_connect_after ((gpointer) w->base.widget, "button_release_event", G_CALLBACK (spectrum_button_release_event), w);
    g_signal_connect_after ((gpointer) w->popup_item, "activate", G_CALLBACK (on_button_config), w);
    gtkui_plugin->w_override_signals (w->base.widget, w);
    deadbeef->vis_waveform_listen (w, spectrum_wavedata_listener);
    return (ddb_gtkui_widget_t *)w;
}

int
musical_spectrum_connect (void)
{
    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);
    if (gtkui_plugin) {
        //trace("using '%s' plugin %d.%d\n", DDB_GTKUI_PLUGIN_ID, gtkui_plugin->gui.plugin.version_major, gtkui_plugin->gui.plugin.version_minor );
        if (gtkui_plugin->gui.plugin.version_major == 2) {
            //printf ("fb api2\n");
            // 0.6+, use the new widget API
            gtkui_plugin->w_reg_widget ("Musical Spectrum", 0, w_musical_spectrum_create, "musical_spectrum", NULL);
            return 0;
        }
    }
    return -1;
}

int
musical_spectrum_start (void)
{
    load_config ();
    return 0;
}

int
musical_spectrum_stop (void)
{
    save_config ();
    return 0;
}

int
musical_spectrum_startup (GtkWidget *cont)
{
    return 0;
}

int
musical_spectrum_shutdown (GtkWidget *cont)
{
    return 0;
}
int
musical_spectrum_disconnect (void)
{
    gtkui_plugin = NULL;
    return 0;
}

// static const char settings_dlg[] =
//     "property \"Ignore files longer than x minutes "
//                 "(-1 scans every file): \"          spinbtn[-1,9999,1] "      CONFSTR_WF_MAX_FILE_LENGTH        " 180 ;\n"
// ;

static DB_misc_t plugin = {
    //DB_PLUGIN_SET_API_VERSION
    .plugin.type            = DB_PLUGIN_MISC,
    .plugin.api_vmajor      = 1,
    .plugin.api_vminor      = 5,
    .plugin.version_major   = 0,
    .plugin.version_minor   = 1,
#if GTK_CHECK_VERSION(3,0,0)
    .plugin.id              = "musical_spectrum-gtk3",
#else
    .plugin.id              = "musical_spectrum",
#endif
    .plugin.name            = "Musical Spectrum",
    .plugin.descr           = "Musical Spectrum",
    .plugin.copyright       =
        "Copyright (C) 2013 Christian Boxdörfer <christian.boxdoerfer@posteo.de>\n"
        "\n"
        "Based on DeaDBeeFs stock spectrum.\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website         = "https://github.com/cboxdoerfer/ddb_musical_spectrum",
    .plugin.start           = musical_spectrum_start,
    .plugin.stop            = musical_spectrum_stop,
    .plugin.connect         = musical_spectrum_connect,
    .plugin.disconnect      = musical_spectrum_disconnect,
    //.plugin.configdialog    = settings_dlg,
};

#if !GTK_CHECK_VERSION(3,0,0)
DB_plugin_t *
ddb_vis_musical_spectrum_GTK2_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return &plugin.plugin;
}
#else
DB_plugin_t *
ddb_vis_musical_spectrum_GTK3_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return &plugin.plugin;
}
#endif
