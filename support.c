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

#include <gtk/gtk.h>

#include "support.h"

#if !GTK_CHECK_VERSION(2,24,0)
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
GtkWidget *
gtk_combo_box_text_new () {
    return gtk_combo_box_new_text ();
}

GtkWidget *
gtk_combo_box_text_new_with_entry   (void) {
    return gtk_combo_box_entry_new ();
}

void
gtk_combo_box_text_append_text (GtkComboBoxText *combo_box, const gchar *text) {
    gtk_combo_box_append_text (combo_box, text);
}

void
gtk_combo_box_text_insert_text (GtkComboBoxText *combo_box, gint position, const gchar *text) {
    gtk_combo_box_insert_text (combo_box, position, text);
}

void
gtk_combo_box_text_prepend_text (GtkComboBoxText *combo_box, const gchar *text) {
    gtk_combo_box_prepend_text (combo_box, text);
}
gchar *
gtk_combo_box_text_get_active_text  (GtkComboBoxText *combo_box) {
    return gtk_combo_box_get_active_text (combo_box);
}

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

