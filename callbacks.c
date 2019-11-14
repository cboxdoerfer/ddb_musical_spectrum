#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "draw_utils.h"
#include "spectrum.h"

gboolean
on_color_changed (GtkWidget *widget, gpointer user_data)
{
    GtkWidget *w = (GtkWidget *)user_data;
    gtk_widget_queue_draw (lookup_widget (w, "gradient_preview"));
    return TRUE;
}

gboolean
on_gradient_preview_expose_event       (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (dialog, "color_box"));
    GList *children = gtk_container_get_children (color_box);

    if (!children) {
        return FALSE;
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
        return FALSE;
    }

    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (widget));

    GtkAllocation a;
    gtk_widget_get_allocation (widget, &a);
    spectrum_gradient_list_set (cr, colors, a.width, a.height);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);

    cairo_destroy (cr);
    g_list_free_full (colors, free);

    return FALSE;
}


void
on_color_add_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (button));
    GtkBox *color_box = GTK_BOX (lookup_widget (dialog, "color_box"));
    if (!color_box) {
        return;
    }
    GtkWidget *color_button = gtk_color_button_new ();
    gtk_box_pack_start (color_box, color_button, TRUE, TRUE, 0);
    gtk_widget_show (color_button);
    gtk_widget_set_size_request (color_button, -1, 30);
    gtk_widget_queue_draw (lookup_widget (dialog, "gradient_preview"));
}


void
on_color_remove_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (button));
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (dialog, "color_box"));
    GList *children = gtk_container_get_children (color_box);

    if (!children) {
        return;
    }

    for (GList *c = children; c != NULL; c = c->next) {
        if (c->next == NULL) {
            gtk_container_remove (color_box, GTK_WIDGET (c->data));
        }
    }
    g_list_free (children);
    gtk_widget_queue_draw (lookup_widget (dialog, "gradient_preview"));
}


void
on_color_reverse_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (button));
    GtkBox *color_box = GTK_BOX (lookup_widget (dialog, "color_box"));
    GList *children = gtk_container_get_children (GTK_CONTAINER (color_box));

    if (!children) {
        return;
    }

    int i = 0;
    int n = g_list_length (children) - 1;
    for (GList *c = children; c != NULL; c = c->next, i++) {
        GtkWidget *child = GTK_WIDGET (c->data);
        gtk_box_reorder_child (color_box, child, n - i);
    }
    g_list_free (children);
    gtk_widget_queue_draw (lookup_widget (dialog, "gradient_preview"));
}

static int
notes_limit_max (GtkSpinButton *spin_max, int max)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (spin_max));
    GtkSpinButton *spin_min = GTK_SPIN_BUTTON (lookup_widget (dialog, "notes_min_spin"));

    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spin_min);
    int value = gtk_adjustment_get_value (adj);

    return CLAMP (max, value + 12, spectrum_notes_size - 1);
}

static int
notes_limit_min (GtkSpinButton *spin_min, int min)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (spin_min));
    GtkSpinButton *spin_max = GTK_SPIN_BUTTON (lookup_widget (dialog, "notes_max_spin"));

    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spin_max);
    int value = gtk_adjustment_get_value (adj);

    return CLAMP (min, 0, value - 12);
}

gboolean
on_notes_min_spin_output               (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spinbutton);
    int value = gtk_adjustment_get_value (adj);
    value = notes_limit_min (spinbutton, value);
    gtk_entry_set_text (GTK_ENTRY (spinbutton), spectrum_notes[value]);
    return TRUE;
}


gboolean
on_notes_max_spin_output               (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spinbutton);
    int value = gtk_adjustment_get_value (adj);
    value = notes_limit_max (spinbutton, value);
    gtk_entry_set_text (GTK_ENTRY (spinbutton), spectrum_notes[value]);
    return TRUE;
}


static gboolean
notes_set_input (GtkSpinButton *spinbutton, gdouble *new_value, int min, int max)
{
    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spinbutton);
    int value = gtk_adjustment_get_value (adj);
    if (value < 0 || value >= spectrum_notes_size) {
        return FALSE;
    }
    if (value < min) {
        value = min;
    }
    else if (value > max) {
        value = max;
    }
    *new_value = (gdouble)value;
    return TRUE;
}

gint
on_notes_min_spin_input                (GtkSpinButton   *spinbutton,
                                        gdouble *new_value,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (spinbutton));
    GtkSpinButton *spinbutton2 = GTK_SPIN_BUTTON (lookup_widget (dialog, "notes_max_spin"));

    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spinbutton2);
    int value = gtk_adjustment_get_value (adj);
    return notes_set_input (spinbutton, new_value, 0, value - 12);
}


gint
on_notes_max_spin_input                (GtkSpinButton   *spinbutton,
                                        gdouble *new_value,
                                        gpointer         user_data)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (spinbutton));
    GtkSpinButton *spinbutton2 = GTK_SPIN_BUTTON (lookup_widget (dialog, "notes_min_spin"));

    GtkAdjustment *adj = gtk_spin_button_get_adjustment (spinbutton2);
    int value = gtk_adjustment_get_value (adj);
    return notes_set_input (spinbutton, new_value, value + 12, spectrum_notes_size - 1);
}

