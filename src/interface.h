#ifndef INTERFACE_H
#define INTERFACE_H

#include <gtk/gtk.h>

//extern GtkBuilder *builder;
extern const gchar *ui_path;

void on_activate(GtkApplication *app,
                 gpointer data);
void on_startup(GtkApplication *app,
                gpointer data);
void on_shutdown(GtkApplication *app,
                 gpointer data);

#endif // INTERFACE_H
