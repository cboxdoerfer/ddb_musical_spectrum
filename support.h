
#if !GTK_CHECK_VERSION(2,12,0)
#define gtk_widget_get_window(widget) ((widget)->window)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#define gtk_dialog_get_action_area(dialog) (dialog->action_area)
#endif

