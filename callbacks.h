#include <gtk/gtk.h>

gboolean
on_color_changed (GtkWidget *widget, gpointer user_data);

void
on_color_add_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_color_remove_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_color_reverse_clicked               (GtkButton       *button,
                                        gpointer         user_data);


gboolean
on_notes_min_spin_output               (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_notes_max_spin_output               (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_notes_min_spin_value_changed        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_notes_max_spin_value_changed        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gint
on_notes_min_spin_input                (GtkSpinButton   *spinbutton,
                                        gdouble *new_value,
                                        gpointer         user_data);

gint
on_notes_max_spin_input                (GtkSpinButton   *spinbutton,
                                        gdouble *new_value,
                                        gpointer         user_data);

gint
on_fft_spin_input                      (GtkSpinButton   *spinbutton,
                                        gdouble *new_value,
                                        gpointer         user_data);

gboolean
on_fft_spin_output                     (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);
