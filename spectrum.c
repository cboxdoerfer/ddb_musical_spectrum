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

#define MAX_BANDS 126
#define VIS_DELAY 1
#define VIS_DELAY_PEAK 20
#define VIS_FALLOFF 1
#define VIS_FALLOFF_PEAK 1
#define BAND_WIDTH 5
#define FFT_SIZE 16384
//#define FFT_SIZE 32768
//#define FFT_SIZE 8192


/* Global variables */
static DB_misc_t            plugin;
static DB_functions_t *     deadbeef = NULL;
static ddb_gtkui_t *        gtkui_plugin = NULL;

typedef struct {
    ddb_gtkui_widget_t base;
    GtkWidget *drawarea;
    guint drawtimer;
#if USE_OPENGL
    GdkGLContext *glcontext;
#endif
    double *data;
    double hanning[FFT_SIZE];
    // keys: frequencies of musical notes, c0;d0;...;f10
    double keys [126];
    double *samples;
    int resized;
    int nsamples;
    int buffered;
    int bars[MAX_BANDS + 1];
    int delay[MAX_BANDS + 1];
    int peaks[MAX_BANDS + 1];
    int delay_peak[MAX_BANDS + 1];
    intptr_t mutex;
    cairo_surface_t *surf;
} w_spectrum_t;

void
do_fft (w_spectrum_t *w)
{
    if (!w->samples || w->buffered < FFT_SIZE) {
        return;
    }
    double *in;
    fftw_complex *out;
    fftw_plan p;
    double real, imag;

    in = fftw_malloc (sizeof (double) * w->nsamples);
    out = fftw_malloc (sizeof (fftw_complex) * w->nsamples);
    p = fftw_plan_dft_r2c_1d (w->nsamples, in, out, FFTW_ESTIMATE);
    for (int i = 0; i < w->nsamples; i++) {
        in[i] = (double)w->samples[i] * w->hanning[i];
        //in[i] = (double)w->samples[i] ;
    }
    fftw_execute (p);
    for (int i = 0; i < w->nsamples/2; i++)
    {
        real = out[i][0];
        imag = out[i][1];
        //w->data[i] = 10.0 * log10(real*real + imag*imag);
        w->data[i] = pow(real*real + imag*imag, 1.0/3.0);
        //w->data[i] = (real*real + imag*imag);
    }
    fftw_destroy_plan (p);
    fftw_free (in);
    fftw_free (out);
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
#if USE_OPENGL
    if (s->glcontext) {
        gdk_gl_context_destroy (s->glcontext);
        s->glcontext = NULL;
    }
#endif
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
    int nsamples = data->nframes/data->fmt->channels;
    int n;
    float ratio = data->fmt->samplerate / 44100.f;
    int size = nsamples / ratio;
    int sz = MIN (nsamples, size);
    if (w->buffered >= FFT_SIZE && w->samples) {
        memmove (w->samples, w->samples + sz, (w->nsamples - sz)*sizeof (double));
        n = w->nsamples - sz;
        do_fft (w);
    }

    float pos = 0;
    for (int i = 0; i < sz && pos < nsamples; i++, pos += ratio) {
        w->samples[n+i] = 0.0;
        for (int j = 0; j < data->fmt->channels; j++) {
            w->samples[n + i] += data->data[(int)(pos * data->fmt->channels) + j];
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

double
spectrum_interpolate (gpointer user_data, double bin0, double bin1)
{
    w_spectrum_t *w = user_data;
    double binmid = (bin0 + bin1) / 2.0;
    //printf ("%f\n",binmid);
    int ibin = (int) (binmid) - 1;
    if (ibin < 1)
        ibin = 1;
    if (ibin >= w->nsamples/2 - 3)
        ibin = w->nsamples/2 - 4;

    double value = CubicInterpolate(w->data[ibin],
            w->data[ibin + 1],
            w->data[ibin + 2],
            w->data[ibin + 3], binmid - (double)ibin);
    return value;
}

int
spectrum_lookup_index (gpointer user_data, float freq)
{
    w_spectrum_t *w = user_data;
    double freq_delta = 44100 / (double)w->nsamples;
    int index = 0;
    return index = (int)(freq/freq_delta);
}

static gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    w_spectrum_t *w = user_data;
    if (!w->samples) {
        return FALSE;
    }
//    do_fft (w);
    double *freq = w->data;
    double freq_delta = 44100.0 / w->nsamples;
    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);

    int width, height, bands;
    bands = MAX_BANDS;
    width = a.width;
    height = a.height;

    for (int i = 0; i < bands; i++)
    {
        double f = 0.0;
        //f = freq[(int)(w->keys[i]/freq_delta)];
        f = spectrum_interpolate (w, w->keys[i]/freq_delta,w->keys[i+1]/freq_delta);
        int x = 20 * log10 (f);
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

#if USE_OPENGL
    GdkGLDrawable *d = gtk_widget_get_gl_drawable (widget);
    gdk_gl_drawable_gl_begin (d, w->glcontext);

    glClear (GL_COLOR_B UFFER_BIT);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    gluOrtho2D(0,a.width,a.height,0);
    glMatrixMode (GL_MODELVIEW);
    glViewport (0, 0, a.width, a.height);

    glBegin (GL_QUADS);
    float base_s = (height / 50.f);

    for (gint i = 0; i <= bands; i++)
    {
        gint x = ((width / bands) * i) + 2;
        int y = a.height - w->bars[i] * base_s;
        glColor3f (0, 0.5, 1);
        glVertex2f (x + 1, y);
        glVertex2f (x + 1 + (width / bands) - 1, y);
        glVertex2f (x + 1 + (width / bands) - 1, a.height);
        glVertex2f (x + 1, a.height);

        // peak
        glColor3f (1, 1, 1);
        y = a.height - w->peaks[i] * base_s;
        glVertex2f (x + 1, y);
        glVertex2f (x + 1 + (width / bands) - 1, y);
        glVertex2f (x + 1 + (width / bands) - 1, y+1);
        glVertex2f (x + 1, y+1);
    }
    glEnd();
    gdk_gl_drawable_swap_buffers (d);

    gdk_gl_drawable_gl_end (d);
#else
    if (!w->surf || cairo_image_surface_get_width (w->surf) != a.width || cairo_image_surface_get_height (w->surf) != a.height) {
        if (w->surf) {
            cairo_surface_destroy (w->surf);
            w->surf = NULL;
        }
        w->surf = cairo_image_surface_create (CAIRO_FORMAT_RGB24, a.width, a.height);
    }
    float base_s = (height / 50.f);

    cairo_surface_flush (w->surf);

    cairo_pattern_t *pat;

    cairo_save (cr);
    cairo_set_source_surface (cr, w->surf, 0, 0);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);
    cairo_restore (cr);
    pat = cairo_pattern_create_linear (0.0, 0.0, 0.0 , a.height);
    cairo_pattern_add_color_stop_rgba (pat, 1, 0, 32.0/255, 100.0/255, 1);
    cairo_pattern_add_color_stop_rgba (pat, 5.0/6.0, 0, 148.0/255, 160.0/255, 1);
    cairo_pattern_add_color_stop_rgba (pat, 4.0/6.0, 0.5, 1, 120.0/255, 1);
    cairo_pattern_add_color_stop_rgba (pat, 3.0/6.0, 1, 1, 0, 1);
    cairo_pattern_add_color_stop_rgba (pat, 2.0/6.0, 1, 0.5, 0, 1);
    cairo_pattern_add_color_stop_rgba (pat, 1.0/6.0, 1, 0, 0, 1);

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
        //_draw_bar (data, stride, x+1, y, bw, a.height-y, 0xff007fff);
        cairo_rectangle (cr, x+1, y, bw, a.height-y);
        y = a.height - w->peaks[i] * base_s;
        if (y < a.height-1) {
            //_draw_bar (data, stride, x + 1, y, bw, 1, 0xffffffff);
            cairo_rectangle (cr, x+1, y, bw, 1);
        }
    }
    cairo_set_source (cr, pat);
    cairo_fill (cr);
    cairo_pattern_destroy (pat);
    cairo_surface_mark_dirty (w->surf);

#endif
    return FALSE;
}


gboolean
spectrum_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gboolean res = spectrum_draw (widget, cr, user_data);
    cairo_destroy (cr);
    return res;
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
    s->nsamples = FFT_SIZE;
    for (int i = 0; i < s->nsamples; i++) {
        s->hanning[i] = (0.5 * (1 - cos (2 * M_PI * i/(s->nsamples/2))));
    }
    for (int i = 0; i < 126; i++) {
        s->keys[i] = 440.0 * pow (2.0, (double)(i-57)/12.0);
    }
#if USE_OPENGL
    if (!gtkui_gl_init ()) {
      :  s->drawtimer = g_timeout_add (33, w_spectrum_draw_cb, w);
    }
#else
    s->drawtimer = g_timeout_add (33, w_spectrum_draw_cb, w);
#endif
    deadbeef->mutex_unlock (s->mutex);
}

void
musical_spectrum_realize (GtkWidget *widget, gpointer data) {
    w_spectrum_t *w = data;
#if USE_OPENGL
    w->glcontext = gtk_widget_create_gl_context (w->drawarea, NULL, TRUE, GDK_GL_RGBA_TYPE);
#endif
}

ddb_gtkui_widget_t *
w_musical_spectrum_create (void) {
    w_spectrum_t *w = malloc (sizeof (w_spectrum_t));
    memset (w, 0, sizeof (w_spectrum_t));

    w->base.widget = gtk_event_box_new ();
    w->base.init = w_spectrum_init;
    w->base.destroy  = w_spectrum_destroy;
    w->drawarea = gtk_drawing_area_new ();
    w->mutex = deadbeef->mutex_create ();
    w->nsamples = FFT_SIZE;
#if USE_OPENGL
    int attrlist[] = {GDK_GL_ATTRIB_LIST_NONE};
    GdkGLConfig *conf = gdk_gl_config_new_by_mode ((GdkGLConfigMode)(GDK_GL_MODE_RGB |
                        GDK_GL_MODE_DOUBLE));
    gboolean cap = gtk_widget_set_gl_capability (w->drawarea, conf, NULL, TRUE, GDK_GL_RGBA_TYPE);
#endif
    gtk_widget_show (w->drawarea);
    gtk_container_add (GTK_CONTAINER (w->base.widget), w->drawarea);
#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) w->drawarea, "expose_event", G_CALLBACK (spectrum_expose_event), w);
#else
    g_signal_connect_after ((gpointer) w->drawarea, "draw", G_CALLBACK (spectrum_draw), w);
#endif
    g_signal_connect_after (G_OBJECT (w->drawarea), "realize", G_CALLBACK (musical_spectrum_realize), w);
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
