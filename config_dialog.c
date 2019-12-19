/*
    Musical Spectrum plugin for the DeaDBeeF audio player

    Copyright (C) 2019 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

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

#define ARRAY_LEN(x)  (sizeof(x) / sizeof((x)[0]))

static const char *window_functions[NUM_WINDOW] = {"Blackmann-Harris", "Hanning", "None"};
static const char *alignment_title[NUM_ALIGNMENT] = {"Left", "Right", "Center"};
static const char *grad_orientation[NUM_ORIENTATION] = {"Vertical", "Horizontal"};
static const char *visual_mode[NUM_STYLE] = {"Musical", "Solid"};

static GtkWidget *channel_button = NULL;

struct config_dialog_entry_t {
    const char *name;
    int id;
    const char **values;
    size_t n_values;
};

static struct config_dialog_entry_t toggle_button_entries[] = {
    {"llabel_check", ID_ENABLE_LEFT_LABELS},
    {"rlabel_check", ID_ENABLE_RIGHT_LABELS},
    {"tlabel_check", ID_ENABLE_TOP_LABELS},
    {"blabel_check", ID_ENABLE_BOTTOM_LABELS},
    {"hgrid_check", ID_ENABLE_HGRID},
    {"vgrid_check", ID_ENABLE_VGRID},
    {"ogrid_check", ID_ENABLE_OGRID},
    {"white_keys_check", ID_ENABLE_WHITE_KEYS},
    {"black_keys_check", ID_ENABLE_BLACK_KEYS},
    {"interpolate_check", ID_INTERPOLATE},
    {"fill_spectrum_check", ID_FILL_SPECTRUM},
    {"tooltip_check", ID_ENABLE_TOOLTIP},
    {"spacing_check", ID_SPACING},
    {"gaps_check", ID_GAPS},
    {"led_check", ID_ENABLE_BAR_MODE},
    {"peaks_check", ID_ENABLE_PEAKS},
    {"amplitudes_check", ID_ENABLE_AMPLITUDES},
    {"peaks_color_check", ID_ENABLE_PEAKS_COLOR},
};

static struct config_dialog_entry_t spin_button_entries[] = {
    {"barw_spin", ID_BAR_W},
    {"pitch_spin", ID_PITCH},
    {"transpose_spin", ID_TRANSPOSE},
    {"notes_min_spin", ID_NOTE_MIN},
    {"notes_max_spin", ID_NOTE_MAX},
    {"amp_min_spin", ID_AMPLITUDE_MIN},
    {"amp_max_spin", ID_AMPLITUDE_MAX},
    {"interval_spin", ID_REFRESH_INTERVAL},
    {"peaks_htime_spin", ID_PEAK_DELAY},
    {"amplitudes_htime_spin", ID_BAR_DELAY},
    {"peaks_gravity_spin", ID_PEAK_FALLOFF},
    {"amplitudes_gravity_spin", ID_BAR_FALLOFF},
};

static struct config_dialog_entry_t color_button_entries[] = {
    {"background_color", ID_COLOR_BG},
    {"vgrid_color", ID_COLOR_VGRID},
    {"hgrid_color", ID_COLOR_HGRID},
    {"ogrid_color", ID_COLOR_OGRID},
    {"text_color", ID_COLOR_TEXT},
    {"wkeys_color", ID_COLOR_WHITE_KEYS},
    {"bkeys_color", ID_COLOR_BLACK_KEYS},
    {"peaks_color", ID_COLOR_PEAKS},
};

static struct config_dialog_entry_t font_button_entries[] = {
    {"font_button", ID_STRING_FONT},
    {"font_tooltip_button", ID_STRING_FONT_TOOLTIP},
};

static struct config_dialog_entry_t combo_button_entries[] = {
    {"window_combo", ID_WINDOW, window_functions, NUM_WINDOW}, 
    {"alignment_combo", ID_ALIGNMENT, alignment_title, NUM_ALIGNMENT}, 
    {"gradient_combo", ID_GRADIENT_ORIENTATION, grad_orientation, NUM_ORIENTATION}, 
    {"mode_combo", ID_DRAW_STYLE, visual_mode, NUM_STYLE}, 
};

static int num_channel_buttons = 18;
const char *channel_buttons[18][2] = {
    {"front_left", "FL "},
    {"front_right", "FR "},
    {"front_center", "FC "},
    {"low_frequency", "LF "},
    {"back_left", "BL "},
    {"back_right", "BR "},
    {"front_left_of_center", "FLC "},
    {"front_right_of_center", "FRC "},
    {"back_center", "BC "},
    {"side_left", "SL "},
    {"side_right", "SR "},
    {"top_center", "TC "},
    {"top_front_left", "TFL "},
    {"top_front_center", "TFC "},
    {"top_front_right", "TFR "},
    {"top_back_left", "TBL "},
    {"top_back_center", "TBC "},
    {"top_back_right", "TBR "},
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void
gradient_draw_generic_event (GtkWidget *widget, cairo_t *cr)
{
    GtkWidget *dialog = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (dialog, "color_box"));
    GtkComboBox *grad_combo = GTK_COMBO_BOX (lookup_widget (dialog, "gradient_combo"));
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
    spectrum_gradient_set (cr, colors, gtk_combo_box_get_active (grad_combo), a.width, a.height);
    cairo_rectangle (cr, 0, 0, a.width, a.height);
    cairo_fill (cr);

    g_list_free_full (colors, free);

    return;
}

static void
on_channel_button_clicked (GtkButton *button,
                           gpointer   user_data)
{
    GtkWidget *popup = GTK_WIDGET (user_data);
    gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time ());
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
get_color_button (GtkWidget *w, const char *w_name, const int index)
{
    GtkColorButton *button = GTK_COLOR_BUTTON (lookup_widget (w, w_name));
    gtk_color_button_get_color (button, &spectrum_config_color[index].val);
}

static void
set_color_button (GtkWidget *w, const char *w_name, GdkColor *color)
{
    GtkColorButton *button = GTK_COLOR_BUTTON (lookup_widget (w, w_name));
    gtk_color_button_set_color (button, color);
}

static const char *
get_font_button (GtkWidget *w, const char *w_name)
{
    GtkFontButton *button = GTK_FONT_BUTTON (lookup_widget (w, w_name));
    return gtk_font_button_get_font_name (button);
}

static void
set_font_button (GtkWidget *w, const char *w_name, const char *font)
{
    GtkFontButton *button = GTK_FONT_BUTTON (lookup_widget (w, w_name));
    gtk_font_button_set_font_name (button, font);
}

static void
get_gradient_colors (GtkWidget *w)
{
    g_list_free_full (CONFIG_GRADIENT_COLORS, g_free);
    CONFIG_GRADIENT_COLORS = NULL;

    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (w, "color_box"));
    GList *children = gtk_container_get_children (color_box);

    int i = 0;
    for (GList *c = children; c != NULL; c = c->next, i++) {
        GtkColorButton *button = GTK_COLOR_BUTTON (c->data);
        GdkColor *clr = g_new0 (GdkColor, 1);
        gtk_color_button_get_color (button, clr);
        CONFIG_GRADIENT_COLORS = g_list_append (CONFIG_GRADIENT_COLORS, clr);
    }
    config_set_int (i, ID_NUM_COLORS);
    g_list_free (children);
}

static void
set_gradient_colors (GtkWidget *w)
{
    GtkContainer *color_box = GTK_CONTAINER (lookup_widget (w, "color_box"));

    for (GList *c = CONFIG_GRADIENT_COLORS; c != NULL; c = c->next) {

        GtkWidget *button = gtk_color_button_new ();
        gtk_color_button_set_use_alpha (GTK_COLOR_BUTTON (button), TRUE);
        gtk_box_pack_start (GTK_BOX (color_box), button, TRUE, TRUE, 0);
        gtk_widget_show (button);
        gtk_widget_set_size_request (button, -1, 30);
        GdkColor *clr = c->data;
        gtk_color_button_set_color (GTK_COLOR_BUTTON (button), clr);
        g_signal_connect_after ((gpointer)button, "color-set", G_CALLBACK (on_color_changed), w);
    }
}

static void
set_channel_menu_button_title (GtkWidget *popup, const char *title)
{
    GtkButton *button = GTK_BUTTON (channel_button);

    if (title) {
        gtk_button_set_label (button, title);
        return;
    }

    GString *button_label = g_string_new (NULL);
    int n_set = 0;
    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, name));
        gboolean active = gtk_check_menu_item_get_active (check);
        if (active) {
            n_set++;
            g_string_append (button_label, channel_buttons[ch][1]);
        }
    }

    char *label = g_string_free (button_label, FALSE);
    if (label) {
        if (n_set == 0) {
            gtk_button_set_label (button, "-");
        }
        else if (n_set == 18) {
            gtk_button_set_label (button, "All Channels");
        }
        else {
            gtk_button_set_label (button, label);
        }
        g_free (label);
        label = NULL;
    }
}

static void
on_channel_check_button_toggled (GtkCheckMenuItem *checkmenuitem,
                                 gpointer          user_data);

static void
set_channel_menu_item_silent (GtkCheckMenuItem *check, GtkWidget *popup, gboolean value)
{
    g_signal_handlers_block_by_func ((gpointer)check, on_channel_check_button_toggled, popup);
    gtk_check_menu_item_set_active (check, value);
    g_signal_handlers_unblock_by_func ((gpointer)check, on_channel_check_button_toggled, popup);
}

static void
on_all_channel_check_button_toggled (GtkCheckMenuItem *checkmenuitem,
                                     gpointer          user_data);

static void
set_all_channel_menu_item_silent (GtkCheckMenuItem *check, GtkWidget *popup, gboolean value)
{
    g_signal_handlers_block_by_func ((gpointer)check, on_all_channel_check_button_toggled, popup);
    gtk_check_menu_item_set_active (check, value);
    g_signal_handlers_unblock_by_func ((gpointer)check, on_all_channel_check_button_toggled, popup);
}

static void
on_channel_check_button_toggled (GtkCheckMenuItem *checkmenuitem,
                                 gpointer          user_data)
{
    GtkWidget *popup = GTK_WIDGET (user_data);
    GtkCheckMenuItem *ch_all_check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, "all_channels"));

    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, name));
        gboolean active = gtk_check_menu_item_get_active (check);
        if (!active) {
            set_all_channel_menu_item_silent (ch_all_check, popup, FALSE);
            set_channel_menu_button_title (popup, NULL); 
            return;
        }
    }
    set_all_channel_menu_item_silent (ch_all_check, popup, TRUE);
    set_channel_menu_button_title (popup, NULL); 
}

void
set_channel_config_values (GtkWidget *popup)
{
    int set_all = 1;
    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, name));
        if (config_get_int (ID_CHANNEL) & 1 << ch) {
            set_channel_menu_item_silent (check, popup, TRUE);
        }
        else {
            set_channel_menu_item_silent (check, popup, FALSE);
            set_all = 0;
        }
    }

    GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, "all_channels"));
    set_all_channel_menu_item_silent (check, popup, set_all);
    set_channel_menu_button_title (popup, NULL); 
}

void
set_config_values (GtkWidget *w)
{
    for (int i = 0; i < ARRAY_LEN (toggle_button_entries); i++) {
        struct config_dialog_entry_t entry = toggle_button_entries[i];
        set_toggle_button (w, entry.name, config_get_int (entry.id));
    }

    for (int i = 0; i < ARRAY_LEN (spin_button_entries); i++) {
        struct config_dialog_entry_t entry = spin_button_entries[i];
        set_spin_button (w, entry.name, config_get_int (entry.id));
    }

    for (int i = 0; i < ARRAY_LEN (color_button_entries); i++) {
        struct config_dialog_entry_t entry = color_button_entries[i];
        set_color_button (w, entry.name, config_get_color (entry.id));
    }
    const int fft_index = log2 (spectrum_config_int[ID_FFT_SIZE].val) - 9;
    set_spin_button (w, "fft_spin", fft_index);

    for (int i = 0; i < ARRAY_LEN (combo_button_entries); i++) {
        struct config_dialog_entry_t entry = combo_button_entries[i];
        set_combo_box (w, entry.name, entry.values, entry.n_values, config_get_int (entry.id));
    }

    for (int i = 0; i < ARRAY_LEN (font_button_entries); i++) {
        struct config_dialog_entry_t entry = font_button_entries[i];
        set_font_button (w, entry.name, config_get_string (entry.id));
    }

    set_gradient_colors (w);
}

static void
get_channel_config_values (GtkWidget *w)
{
    config_set_int (0, ID_CHANNEL);
    uint32_t channel = 0;
    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (w, name));
        gboolean active = gtk_check_menu_item_get_active (check);
        if (active) {
            channel |= 1 << ch;
        }
    }
    config_set_int (channel, ID_CHANNEL);
}

static void
get_config_values (GtkWidget *w)
{
    for (int i = 0; i < ARRAY_LEN (toggle_button_entries); i++) {
        struct config_dialog_entry_t entry = toggle_button_entries[i];
        config_set_int (get_toggle_button (w, entry.name), entry.id);
    }

    for (int i = 0; i < ARRAY_LEN (spin_button_entries); i++) {
        struct config_dialog_entry_t entry = spin_button_entries[i];
        config_set_int (get_spin_button (w, entry.name), entry.id);
    }

    for (int i = 0; i < ARRAY_LEN (color_button_entries); i++) {
        struct config_dialog_entry_t entry = color_button_entries[i];
        get_color_button (w, entry.name, entry.id);
    }

    const int fft_index = get_spin_button (w, "fft_spin");
    config_set_int ((int)exp2 (fft_index + 9), ID_FFT_SIZE); 

    for (int i = 0; i < ARRAY_LEN (combo_button_entries); i++) {
        struct config_dialog_entry_t entry = combo_button_entries[i];
        config_set_int (get_combo_box (w, entry.name), entry.id); 
    }

    for (int i = 0; i < ARRAY_LEN (font_button_entries); i++) {
        struct config_dialog_entry_t entry = font_button_entries[i];
        config_set_string (get_font_button (w, entry.name), entry.id);
    }

    get_gradient_colors (w);
}

static void
on_all_channel_check_button_toggled (GtkCheckMenuItem *checkmenuitem,
                                     gpointer          user_data)
{
    GtkWidget *popup = GTK_WIDGET (user_data);
    gboolean active = gtk_check_menu_item_get_active (checkmenuitem);

    if (active) {
        set_channel_menu_button_title (popup, "All Channels"); 
    }
    else {
        set_channel_menu_button_title (popup, "-"); 
    }

    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM (lookup_widget (popup, name));
        g_signal_handlers_block_by_func ((gpointer)check, on_channel_check_button_toggled, popup);
        gtk_check_menu_item_set_active (check, active);
        g_signal_handlers_unblock_by_func ((gpointer)check, on_channel_check_button_toggled, popup);
    }
}

static void
set_callbacks (GtkWidget *w, GtkWidget *popup)
{
    GtkWidget *gradient_preview = GTK_WIDGET (lookup_widget (w, "gradient_preview"));
#if !GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after ((gpointer) gradient_preview, "expose_event", G_CALLBACK (on_gradient_preview_expose_event), NULL);
#else
    g_signal_connect_after ((gpointer) gradient_preview, "draw", G_CALLBACK (on_gradient_preview_draw_event), NULL);
#endif

    channel_button = GTK_WIDGET (lookup_widget (w, "channel_button"));
    g_signal_connect_after ((gpointer) channel_button, "clicked", G_CALLBACK (on_channel_button_clicked), popup);

    for (int ch = 0; ch < num_channel_buttons; ch++) {
        const char *name = channel_buttons[ch][0];
        GtkWidget *ch_check = GTK_WIDGET (lookup_widget (popup, name));
        g_signal_connect_after ((gpointer) ch_check, "toggled", G_CALLBACK (on_channel_check_button_toggled), popup);
    }

    GtkWidget *ch_check = GTK_WIDGET (lookup_widget (popup, "all_channels"));
    g_signal_connect_after ((gpointer) ch_check, "toggled", G_CALLBACK (on_all_channel_check_button_toggled), popup);
}

void
on_button_config (GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *dialog = create_config_dialog ();
    GtkWidget *popup = create_channel_menu ();

    set_callbacks (dialog, popup);
    set_config_values (dialog);
    set_channel_config_values (popup);

    while (TRUE) {
        int response = gtk_dialog_run (GTK_DIALOG (dialog));
        if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
            get_config_values (dialog);
            get_channel_config_values (popup);
            save_config ();
            deadbeef->sendmessage (DB_EV_CONFIGCHANGED, 0, 0, 0);
        }
        if (response == GTK_RESPONSE_APPLY) {
            continue;
        }
        break;
    }
    gtk_widget_destroy (popup);
    gtk_widget_destroy (dialog);
    return;
}
#pragma GCC diagnostic pop

