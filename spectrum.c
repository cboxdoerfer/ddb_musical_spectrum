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
#define VIS_FALLOFF 1
#define VIS_FALLOFF_PEAK 1
#define BAND_WIDTH 5
#define FFT_SIZE 16384
//#define FFT_SIZE 32768
//#define FFT_SIZE 8192

#define     CONFSTR_MS_DISPLAY_GRADIENT       "musical_spectrum.display_gradient"

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
    double hanning[FFT_SIZE];
    // keys: index of frequencies of musical notes (c0;d0;...;f10) in data
    float keys [126];
    int colors [1024];
    double *samples;
    int resized;
    int buffered;
    int bars[MAX_BANDS + 1];
    int delay[MAX_BANDS + 1];
    int peaks[MAX_BANDS + 1];
    int delay_peak[MAX_BANDS + 1];
    intptr_t mutex;
    cairo_surface_t *surf;
} w_spectrum_t;

static double *in, *out_real;
static fftw_complex *out_complex;
static fftw_plan p_r2r;
static fftw_plan p_r2c;

static gboolean CONFIG_GRADIENT_ENABLED = FALSE;

static void
save_config (void)
{
    deadbeef->conf_set_int (CONFSTR_MS_DISPLAY_GRADIENT,         CONFIG_GRADIENT_ENABLED);
}

static void
load_config (void)
{
    deadbeef->conf_lock ();
    CONFIG_GRADIENT_ENABLED = deadbeef->conf_get_int (CONFSTR_MS_DISPLAY_GRADIENT,             FALSE);
    deadbeef->conf_unlock ();
}

void
do_fft (w_spectrum_t *w)
{
    if (!w->samples || w->buffered < FFT_SIZE) {
        return;
    }
    //double real,imag;

    for (int i = 0; i < FFT_SIZE; i++) {
        in[i] = (double)w->samples[i] * w->hanning[i];
    }
    fftw_execute (p_r2r);
    //fftw_execute (p_r2c);
    for (int i = 0; i < FFT_SIZE/2; i++)
    {
        //real = out[i][0];
        //imag = out[i][1];
        //w->data[i] = (real*real + imag*imag);
        w->data[i] = out_real[i]*out_real[i] + out_real[FFT_SIZE-i-1]*out_real[FFT_SIZE-i-1];
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
_draw_bar (uint8_t *data, int stride, int x0, int y0, int w, int h, int total_h, gpointer user_data) {
    w_spectrum_t *s = user_data;
    int y1 = y0+h-1;
    int x1 = x0+w-1;
    uint32_t *ptr = (uint32_t*)&data[y0*stride+x0*4];
    while (y0 <= y1) {
        int x = x0;
        int index = ftoi(((double)y0/(double)total_h) * 1023);
        index = CLAMP (index, 0, 1023);
        while (x++ <= x1) {
            *ptr++ = s->colors[index];
        }
        y0++;
        ptr += stride/4-w;
    }
}

/* based on Delphi function by Witold J.Janik */
int
create_gradient (double position)
{
    /* if position > 1 then we have repetition of colors it maybe useful    */
    if (position>1.0){if (position-(int)position==0.0)position=1.0; else position=position-(int)position;}

    unsigned char nmax=5; /* number of color segments */
    double m=nmax* position;

    int n=(int)m; // integer of m

    double f=m-n;  // fraction of m
    unsigned char t=(int)(f*255);

    char c[3];

    switch(n){
        case 0: {
                    c[0] = 255;
                    c[1] = t/2;
                    c[2] = 0;
                    break;
                };
        case 1: {
                    c[0] = 255;
                    c[1] = 128+(t/2);
                    c[2] = 0;
                    break;
                };
        case 2: {
                    c[0] = 255-(t/2);
                    c[1] = 255;
                    c[2] = 0+(t*120/255);
                    break;
                };
        case 3: {
                    c[0] = 128-(t/2);
                    c[1] = 255-(t*107/255);
                    c[2] = 120+(t*40/255);
                    break;
                };
        case 4: {
                    c[0] = 0;
                    c[1] = 148-(t*112/255);
                    c[2] = 160-(t*60/255);
                    break;
                };
        case 5: {
                    c[0] = 0;
                    c[1] = 32;
                    c[2] = 100;
                    break;
                };
        default: {
                     c[0] = 255;
                     c[1] = 0;
                     c[2] = 0;
                     break;
                };
    }; // case

    return ((255 & 0xFF) << 24) | //alpha
        (((int)c[0] & 0xFF) << 16) | //red
        (((int)c[1] & 0xFF) << 8)  | //green
        (((int)c[2] & 0xFF) << 0); //blue
}

static int
on_config_changed (uintptr_t ctx)
{
    //load_config ();
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
    GtkWidget *display_gradient;
    GtkWidget *dialog_action_area13;
    GtkWidget *applybutton1;
    GtkWidget *cancelbutton1;
    GtkWidget *okbutton1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    spectrum_properties = gtk_dialog_new ();
    gtk_widget_set_size_request (spectrum_properties, -1, 350);
    gtk_window_set_title (GTK_WINDOW (spectrum_properties), "Spectrum Properties");
    gtk_window_set_type_hint (GTK_WINDOW (spectrum_properties), GDK_WINDOW_TYPE_HINT_DIALOG);

    config_dialog = gtk_dialog_get_content_area (GTK_DIALOG (spectrum_properties));
    gtk_widget_show (config_dialog);

    vbox01 = gtk_vbox_new (FALSE, 8);
    gtk_widget_show (vbox01);
    gtk_box_pack_start (GTK_BOX (config_dialog), vbox01, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox01), 12);

    display_gradient = gtk_check_button_new_with_label ("Display gradient");
    gtk_widget_show (display_gradient);
    gtk_box_pack_start (GTK_BOX (vbox01), display_gradient, FALSE, FALSE, 0);

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

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (display_gradient), CONFIG_GRADIENT_ENABLED);

    for (;;) {
        int response = gtk_dialog_run (GTK_DIALOG (spectrum_properties));
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
            CONFIG_GRADIENT_ENABLED = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (display_gradient));
            //save_config ();
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
    if (p_r2r) {
        fftw_destroy_plan (p_r2r);
    }
    if (p_r2c) {
        fftw_destroy_plan (p_r2c);
    }
    if (in) {
        fftw_free (in);
        in = NULL;
    }
    if (out_real) {
        fftw_free (out_real);
        out_real = NULL;
    }
    if (out_complex) {
        fftw_free (out_complex);
        out_complex = NULL;
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
    int nsamples = data->nframes/data->fmt->channels;
    float ratio = data->fmt->samplerate / 44100.f;
    int size = nsamples / ratio;
    int sz = MIN (FFT_SIZE, size);
    int n = FFT_SIZE - sz;
    if (w->buffered >= FFT_SIZE && w->samples) {
        memmove (w->samples, w->samples + sz, (FFT_SIZE - sz)*sizeof (double));
    }

    float pos = 0;
    for (int i = 0; i < sz && pos < nsamples; i++, pos += ratio) {
        w->samples[n+i] = 0.0;
        for (int j = 0; j < data->fmt->channels; j++) {
            w->samples[n + i] += data->data[ftoi (pos * data->fmt->channels) + j];
        }
        w->samples[n + i] /= data->fmt->channels;
    }
    if (w->buffered < FFT_SIZE) {
        w->buffered += sz;
    }
}
// Copied from audacity
double CubicInterpolate(double y0, double y1, double y2, double y3, double x)
{
    double a, b, c, d;

    a = y0 / -6.0 + y1 / 2.0 - y2 / 2.0 + y3 / 6.0;
    b = y0 - 5.0 * y1 / 2.0 + 2.0 * y2 - y3 / 2.0;
    c = -11.0 * y0 / 6.0 + 3.0 * y1 - 3.0 * y2 / 2.0 + y3 / 3.0;
    d = y0;

    double xx = x * x;
    double xxx = xx * x;

    return (a * xxx + b * xx + c * x + d);
}

float
spectrum_interpolate (gpointer user_data, float bin0, float bin1)
{
    w_spectrum_t *w = user_data;
    double binmid = (bin0 + bin1) / 2.0;
    int ibin = ftoi (binmid) - 1;
    if (ibin < 1)
        ibin = 1;
    if (ibin >= FFT_SIZE/2 - 3)
        ibin = FFT_SIZE/2 - 4;

    return CubicInterpolate(w->data[ibin],
            w->data[ibin + 1],
            w->data[ibin + 2],
            w->data[ibin + 3], binmid - (double)ibin);
}

int
spectrum_lookup_index (gpointer user_data, float freq)
{
    w_spectrum_t *w = user_data;
    double freq_delta = 44100 / (double)FFT_SIZE;
    int index = 0;
    return index = ftoi (freq/freq_delta);
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

    for (int i = 0; i < bands; i++)
    {
        float f = w->data[ftoi (w->keys[i])];
        //float f = spectrum_interpolate (w, w->keys[i], w->keys[i+1]);
        int x = 10 * log10 (f);
        x = CLAMP (x, 0, 50);

        w->bars[i] -= MAX (0, VIS_FALLOFF - w->delay[i]);
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

    // start drawing
    if (!w->surf || cairo_image_surface_get_width (w->surf) != a.width || cairo_image_surface_get_height (w->surf) != a.height) {
        if (w->surf) {
            cairo_surface_destroy (w->surf);
            w->surf = NULL;
        }
        w->surf = cairo_image_surface_create (CAIRO_FORMAT_RGB24, a.width, a.height);
    }
    float base_s = (height / 50.f);

    cairo_surface_flush (w->surf);


    cairo_save (cr);
    cairo_set_source_surface (cr, w->surf, 0, 0);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);
    cairo_restore (cr);
    unsigned char *data = cairo_image_surface_get_data (w->surf);
    int stride = 0;
    cairo_pattern_t *pat;
    if (!data) {
        return FALSE;
    }
    stride = cairo_image_surface_get_stride (w->surf);
    memset (data, 0, a.height * stride);

    if (CONFIG_GRADIENT_ENABLED) {
        pat = cairo_pattern_create_linear (0.0, 0.0, 0.0 , a.height);
        cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0.125, 0.255, 1);
        cairo_pattern_add_color_stop_rgba (pat, 0.83, 0, 0.58, 0.627, 1);
        cairo_pattern_add_color_stop_rgba (pat, 0.67, 0.5, 1, 0.47, 1);
        cairo_pattern_add_color_stop_rgba (pat, 0.5, 1, 1, 0, 1);
        cairo_pattern_add_color_stop_rgba (pat, 0.33, 1, 0.5, 0, 1);
        cairo_pattern_add_color_stop_rgba (pat, 0.17, 1, 0, 0, 1);
    }

    int barw = CLAMP (width / bands, 2, 20);
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
        if (!CONFIG_GRADIENT_ENABLED) {
            _draw_bar (data, stride, x+1, y, bw, a.height-y, a.height, user_data);
        }
        else {
            cairo_rectangle (cr, x+1, y, bw, a.height-y);
        }
        y = a.height - w->peaks[i] * base_s;
        if (y < a.height-1) {
            if (!CONFIG_GRADIENT_ENABLED) {
                _draw_bar (data, stride, x + 1, y, bw, 1, a.height, user_data);
            }
            else {
                cairo_rectangle (cr, x+1, y, bw, 1);
            }
        }
    }
    if (CONFIG_GRADIENT_ENABLED && pat) {
        cairo_set_source (cr, pat);
        cairo_fill (cr);
        cairo_pattern_destroy (pat);
    }
    cairo_surface_mark_dirty (w->surf);

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
    w_spectrum_t *w = user_data;
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

    switch (id) {
    case DB_EV_CONFIGCHANGED:
        on_config_changed (ctx);
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
        s->hanning[i] = (0.5 * (1 - cos (2 * M_PI * i/FFT_SIZE)));
    }
    for (int i = 0; i < 126; i++) {
        s->keys[i] = (float)(440.0 * (pow (2.0, (double)(i-57)/12.0) * FFT_SIZE/44100.0));
    }
    for (int i = 0; i < 1024; i++) {
        s->colors[i] = create_gradient ((double)i/1024);
    }
    in = fftw_malloc (sizeof (double) * FFT_SIZE);
    out_real = fftw_malloc (sizeof (double) * FFT_SIZE);
    out_complex = fftw_malloc (sizeof (fftw_complex) * FFT_SIZE);
    p_r2r = fftw_plan_r2r_1d (FFT_SIZE, in, out_real, FFTW_R2HC, FFTW_ESTIMATE);
    p_r2c = fftw_plan_dft_r2c_1d (FFT_SIZE, in, out_complex, FFTW_ESTIMATE);
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
            gtkui_plugin->w_reg_widget ("Musical Spectrum", DDB_WF_SINGLE_INSTANCE, w_musical_spectrum_create, "musical_spectrum", NULL);
            return 0;
        }
    }
    return -1;
}

int
musical_spectrum_start (void)
{
    //load_config ();
    return 0;
}

int
musical_spectrum_stop (void)
{
    //save_config ();
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
