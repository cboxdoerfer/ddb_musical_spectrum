#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(2,14,0)
#define gtk_widget_get_window(widget) ((widget)->window)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#define gtk_dialog_get_action_area(dialog) (dialog->action_area)
#endif

#if !GTK_CHECK_VERSION(2,18,0)
void                gtk_widget_set_allocation           (GtkWidget *widget,
                                                         const GtkAllocation *allocation);

void                gtk_widget_get_allocation           (GtkWidget *widget,
                                                         GtkAllocation *allocation);

#define gtk_widget_set_can_default(widget, candefault) {if (candefault) GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT); else GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);}
#endif

#if !GTK_CHECK_VERSION(2,24,0)
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
typedef GtkComboBox GtkComboBoxText;
GtkWidget *gtk_combo_box_text_new ();
GtkWidget *gtk_combo_box_text_new_with_entry   (void);
void gtk_combo_box_text_append_text (GtkComboBoxText *combo_box, const gchar *text);
void gtk_combo_box_text_insert_text (GtkComboBoxText *combo_box, gint position, const gchar *text);
void gtk_combo_box_text_prepend_text (GtkComboBoxText *combo_box, const gchar *text);
gchar *gtk_combo_box_text_get_active_text  (GtkComboBoxText *combo_box);
#endif

