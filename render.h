#pragma once

#include <gtk/gtk.h>

struct spectrum_render_bin_t {
    double amp;
    double amp_vel;
    double peak;
    double peak_vel;
};

struct spectrum_render_data_t {
    struct spectrum_render_bin_t **bins;
};

gboolean
spectrum_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);

struct spectrum_data_t *
spectrum_data_new (void);

void
spectrum_data_free (struct spectrum_data_t *data);
