#include <stdio.h>
#include <gtk/gtk.h>
#include "interface.h"

const gchar *ui_path = "res/app.glade";
const gchar *app_id = "org.gtk.hough-transition";

int main(int argc, char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new(app_id, G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "startup",
                   G_CALLBACK(on_startup),
                   NULL);
  g_signal_connect(app, "activate",
                   G_CALLBACK(on_activate),
                   NULL);
  g_signal_connect(app, "shutdown",
                   G_CALLBACK(on_shutdown),
                   NULL);
  status = g_application_run(G_APPLICATION(app),
                             argc, argv);

  return status;
}
