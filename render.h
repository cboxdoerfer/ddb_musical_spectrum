#pragma once

#include <gtk/gtk.h>

struct spectrum_render_t {
    double *bars;
    double *bars_peak;
    double *peaks;
    int *delay_bars;
    int *delay_peaks;
    double *v_bars;
    double *v_peaks;
    int bar_delay;
    int peak_delay;
    double bar_velocity;
    double peak_velocity;
    cairo_pattern_t *pattern;
};

gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);

struct spectrum_data_t *
spectrum_data_new (void);

void
spectrum_data_free (struct spectrum_data_t *data);

struct spectrum_render_t *
spectrum_render_new (void);

void
spectrum_render_free (struct spectrum_render_t *render);
