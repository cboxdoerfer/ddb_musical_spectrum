#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <fftw3.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

#include "render.h"
#include "support.h"
#include "config.h"
#include "config_dialog.h"
#include "utils.h"
#include "draw_utils.h"
#include "spectrum.h"

DB_functions_t *deadbeef = NULL;
ddb_gtkui_t *gtkui_plugin = NULL;

char *spectrum_notes[] =
{
    "C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
    "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
    "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
    "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
    "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
    "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
    "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
    "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
    "C8","C#8","D8","D#8","E8","F8","F#8","G8","G#8","A8","A#8","B8",
    "C9","C#9","D9","D#9","E9","F9","F#9","G9","G#9","A9","A#9","B9",
    "C10","C#10","D10","D#10","E10","F10","F#10","G10","G#10","A10","A#10","B10",
    "C11","C#11","D11","D#11","E11","F11","F#11","G11","G#11","A11","A#11","B11"
};

size_t spectrum_notes_size = sizeof (spectrum_notes)/sizeof (spectrum_notes[0]);

static void
spectrum_queue_spectrum_region (w_spectrum_t *s)
{
    cairo_rectangle_t r = s->spectrum_rectangle;
    gtk_widget_queue_draw_area (s->drawarea, r.x, r.y, r.width, r.height);
}

static gboolean
spectrum_draw_cb (void *data) {
    w_spectrum_t *s = data;

    spectrum_queue_spectrum_region (s);
    return TRUE;
}

static gboolean
spectrum_redraw_cb (void *data) {
    w_spectrum_t *s = data;
    s->need_redraw = 1;
    gtk_widget_queue_draw (s->drawarea);
    return FALSE;
}

static int
on_config_changed (gpointer user_data)
{
    w_spectrum_t *w = user_data;
    load_config ();
    deadbeef->mutex_lock (w->data->mutex);
    w->need_redraw = 1;
    if (w->data->fft_plan) {
        fftw_destroy_plan (w->data->fft_plan);
    }
    window_table_fill (w->data->window);
    update_gravity (w->render);

    w->data->fft_plan = fftw_plan_dft_r2c_1d (CLAMP (CONFIG_FFT_SIZE, 512, MAX_FFT_SIZE), w->data->fft_in, w->data->fft_out, FFTW_ESTIMATE);
    deadbeef->mutex_unlock (w->data->mutex);
    g_idle_add (spectrum_redraw_cb, w);
    return 0;
}

///// spectrum vis
static void
w_spectrum_destroy (ddb_gtkui_widget_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    deadbeef->vis_waveform_unlisten (w);
    if (CONFIG_GRADIENT_COLORS) {
        g_list_free_full (CONFIG_GRADIENT_COLORS, g_free);
        CONFIG_GRADIENT_COLORS = NULL;
    }
    if (s->data) {
        spectrum_data_free (s->data);
    }
    if (s->render) {
        spectrum_render_free (s->render);
    }
    if (s->drawtimer) {
        g_source_remove (s->drawtimer);
        s->drawtimer = 0;
    }
}

gboolean
spectrum_remove_refresh_interval (gpointer user_data)
{
    w_spectrum_t *w = user_data;
    if (w->drawtimer) {
        g_source_remove (w->drawtimer);
        w->drawtimer = 0;
    }
    return TRUE;
}

static gboolean
spectrum_set_refresh_interval (gpointer user_data, int interval)
{
    w_spectrum_t *w = user_data;
    g_return_val_if_fail (w && interval > 0, FALSE);

    spectrum_remove_refresh_interval (w);
    w->drawtimer = g_timeout_add (interval, spectrum_draw_cb, w);
    return TRUE;
}

static void
spectrum_wavedata_listener (void *ctx, ddb_audio_data_t *data) {
    w_spectrum_t *w = ctx;
    g_return_if_fail (w->data->samples);

    const int channels = data->fmt->channels;
    const int nsamples = data->nframes;
    const int sz = channels * MIN (CONFIG_FFT_SIZE, nsamples);
    const int n = channels * CONFIG_FFT_SIZE - sz;
    memmove (w->data->samples, w->data->samples + sz, n * sizeof (double));

    w->data->num_channels = channels;
    int sample_index = n;
    for (int i = 0; i < sz; i++, sample_index++) {
        w->data->samples[sample_index] = data->data[i];
    }
}

static gboolean
spectrum_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));
    gboolean res = spectrum_draw (widget, cr, user_data);
    cairo_destroy (cr);
    return res;
}

static gboolean
spectrum_configure_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    g_idle_add (spectrum_redraw_cb, w);
    return FALSE;
}

static gboolean
spectrum_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    //w_spectrum_t *w = user_data;
    return TRUE;
}

static gboolean
spectrum_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    if (event->button == 3) {
      gtk_menu_popup (GTK_MENU (w->popup), NULL, NULL, NULL, w->drawarea, 0, gtk_get_current_event_time ());
    }
    return TRUE;
}

static gboolean
spectrum_enter_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    w->motion_ctx.entered = 1;
    return FALSE;
}

static gboolean
spectrum_leave_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    w->motion_ctx.entered = 0;
    if (CONFIG_ENABLE_TOOLTIP) {
        spectrum_queue_spectrum_region (w);
    }
    return FALSE;
}

static gboolean
spectrum_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
    w_spectrum_t *w = user_data;
    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);

    if (CONFIG_ENABLE_TOOLTIP) {
        w->motion_ctx.x = event->x - 1;
        w->motion_ctx.y = event->y - 1;
        spectrum_queue_spectrum_region (w);
    }

    return FALSE;
}

static int
spectrum_message (ddb_gtkui_widget_t *widget, uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2)
{
    w_spectrum_t *w = (w_spectrum_t *)widget;

    const int samplerate_temp = w->samplerate;
    switch (id) {
        case DB_EV_SONGSTARTED:
            w->samplerate = deadbeef->get_output ()->fmt.samplerate;
            if (w->samplerate == 0) w->samplerate = 44100;
            if (samplerate_temp != w->samplerate) {
                w->need_redraw = 1;
            }
            spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            break;
        case DB_EV_CONFIGCHANGED:
            on_config_changed (w);
            if (deadbeef->get_output ()->state () == OUTPUT_STATE_PLAYING) {
                spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            }
            break;
        case DB_EV_PAUSED:
            if (p1 == 0) {
                spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
            }
            break;
        case DB_EV_STOP:
            g_idle_add (spectrum_redraw_cb, w);
            break;
    }
    return 0;
}

static void
spectrum_init (w_spectrum_t *w) {
    w_spectrum_t *s = (w_spectrum_t *)w;
    load_config ();

    s->data = spectrum_data_new ();
    s->render = spectrum_render_new ();

    s->samplerate = deadbeef->get_output ()->fmt.samplerate;
    if (s->samplerate == 0) s->samplerate = 44100;

    window_table_fill (s->data->window);
    update_gravity (s->render);

    if (deadbeef->get_output ()->state () == OUTPUT_STATE_PLAYING) {
        w->playback_status = PLAYING;
        spectrum_set_refresh_interval (w, CONFIG_REFRESH_INTERVAL);
    }
    deadbeef->vis_waveform_listen (w, spectrum_wavedata_listener);
    s->need_redraw = 1;
    s->prev_width = -1;
    s->prev_height = -1;
}

static ddb_gtkui_widget_t *
w_musical_spectrum_create (void) {
    w_spectrum_t *w = malloc (sizeof (w_spectrum_t));
    memset (w, 0, sizeof (w_spectrum_t));

    w->base.widget = gtk_event_box_new ();
    w->base.destroy  = w_spectrum_destroy;
    w->base.message = spectrum_message;
    w->drawarea = gtk_drawing_area_new ();
    w->popup = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (w->popup), w->base.widget, NULL);
    w->popup_item = gtk_menu_item_new_with_mnemonic ("Configure");

    gtk_container_add (GTK_CONTAINER (w->base.widget), w->drawarea);
    gtk_container_add (GTK_CONTAINER (w->popup), w->popup_item);
    gtk_widget_show (w->drawarea);
    gtk_widget_show (w->popup);
    gtk_widget_show (w->popup_item);

    gtk_widget_add_events (w->drawarea,
            GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK );

#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) w->drawarea, "expose_event", G_CALLBACK (spectrum_expose_event), w);
#else
    g_signal_connect_after ((gpointer) w->drawarea, "draw", G_CALLBACK (spectrum_expose_event), w);
#endif
    g_signal_connect_after ((gpointer) w->drawarea, "configure_event", G_CALLBACK (spectrum_configure_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "button_press_event", G_CALLBACK (spectrum_button_press_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "button_release_event", G_CALLBACK (spectrum_button_release_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "motion_notify_event", G_CALLBACK (spectrum_motion_notify_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "enter_notify_event", G_CALLBACK (spectrum_enter_notify_event), w);
    g_signal_connect_after ((gpointer) w->drawarea, "leave_notify_event", G_CALLBACK (spectrum_leave_notify_event), w);
    g_signal_connect_after ((gpointer) w->popup_item, "activate", G_CALLBACK (on_button_config), w);
    gtkui_plugin->w_override_signals (w->base.widget, w);

    spectrum_init (w);
    return (ddb_gtkui_widget_t *)w;
}

static int
musical_spectrum_connect (void)
{
    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);
    if (gtkui_plugin) {
        //trace("using '%s' plugin %d.%d\n", DDB_GTKUI_PLUGIN_ID, gtkui_plugin->gui.plugin.version_major, gtkui_plugin->gui.plugin.version_minor );
        if (gtkui_plugin->gui.plugin.version_major == 2) {
            // 0.6+, use the new widget API
            gtkui_plugin->w_reg_widget ("Musical Spectrum", DDB_WF_SINGLE_INSTANCE, w_musical_spectrum_create, "musical_spectrum", NULL);
            return 0;
        }
    }
    return -1;
}

static int
musical_spectrum_start (void)
{
    load_config ();
    return 0;
}

static int
musical_spectrum_stop (void)
{
    save_config ();
    return 0;
}

static int
musical_spectrum_disconnect (void)
{
    gtkui_plugin = NULL;
    return 0;
}

static const char settings_dlg[] =
    "property \"Refresh interval (ms): \"       spinbtn[10,1000,1] "        CONFSTR_MS_REFRESH_INTERVAL         " 25 ;\n"
    "property \"Bar gravity (dB/s²): \"          spinbtn[-1,1000,1] "        CONFSTR_MS_BAR_FALLOFF              " -1 ;\n"
    "property \"Bar delay (ms): \"              spinbtn[0,10000,100] "      CONFSTR_MS_BAR_DELAY                " 0 ;\n"
    "property \"Peak gravity (dB/s²): \"         spinbtn[-1,1000,1] "        CONFSTR_MS_PEAK_FALLOFF             " 50 ;\n"
    "property \"Peak delay (ms): \"             spinbtn[0,10000,100] "      CONFSTR_MS_PEAK_DELAY               " 500 ;\n"
;

DB_misc_t plugin = {
    //DB_PLUGIN_SET_API_VERSION
    .plugin.type            = DB_PLUGIN_MISC,
    .plugin.api_vmajor      = 1,
    .plugin.api_vminor      = 5,
    .plugin.version_major   = 0,
    .plugin.version_minor   = 3,
#if GTK_CHECK_VERSION(3,0,0)
    .plugin.id              = "musical_spectrum-gtk3",
#else
    .plugin.id              = "musical_spectrum",
#endif
    .plugin.name            = "Musical Spectrum",
    .plugin.descr           = "Musical Spectrum",
    .plugin.copyright       =
        "Copyright (C) 2019 Christian Boxdörfer <christian.boxdoerfer@posteo.de>\n"
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
    .plugin.configdialog    = settings_dlg,
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
