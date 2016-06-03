#include "interface.h"
#include "imgproc.h"
#include "hough-recog.h"
#include <math.h>
#include <stdio.h>
#include <cairo.h>

#define MAX_STRING_SIZE 100
#define MOUSE_DOWN 4
#define MOUSE_UP 8

static int mouse_flag = MOUSE_UP;
static int roi_is_set = 0;
static GdkPixbuf *clear_img = NULL;
struct point
{
  int x,y;
} start_pos = {0,0};
struct rect
{
  int x, y, width, height;
} roi;
struct rect img_size;

#define INIT_RECT(rec, imx, imy, imwidth, imheight)\
{\
  rec.x = imx;\
  rec.y = imy;\
  rec.width = imwidth;\
  rec.height = imheight;\
}

#define ROI_SET (roi_is_set = 1)
#define ROI_UNSET (roi_is_set = 0)


static GtkBuilder *builder;

static void
on_open_image(GtkWidget *button, gpointer data)
{
  GtkBuilder *builder;
  GtkFileFilter *filter;
  GtkImage *image;
  GtkWidget *parent, *fdialog, *recog_button;
  gint response;

  builder = GTK_BUILDER(data);
  recog_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "recognize"));
  image = GTK_IMAGE(gtk_builder_get_object(builder,
                                           "image"));
  filter = gtk_file_filter_new();
  parent = GTK_WIDGET(gtk_builder_get_object(builder,
                                             "mainwindow"));
  fdialog = gtk_file_chooser_dialog_new("Выбор изображения",
                                        GTK_WINDOW(parent),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_OK", GTK_RESPONSE_ACCEPT,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        NULL);
  gtk_file_filter_add_pixbuf_formats(filter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fdialog), filter);
  switch(response = gtk_dialog_run(GTK_DIALOG(fdialog)))
    {
    case GTK_RESPONSE_ACCEPT:
      {
        gchar *fname = gtk_file_chooser_get_filename(
              GTK_FILE_CHOOSER(fdialog));
        gtk_image_set_from_file(image, fname);
        gtk_widget_set_sensitive(recog_button, TRUE);
        break;
      }
    default:
      break;
    }
  gtk_widget_destroy(fdialog);
}

static int
draw_and_save_10_digits(const gchar* path)
{
  GdkPixbuf *image;
  GError *error;
  gchar *fname;

  fname = malloc(MAX_STRING_SIZE);
  error = NULL;
  for(int i = 0; i < 10; ++i)
    {
      sprintf(fname, "%s/%i.jpg", path, i);
      image = draw_digit(i);
      gdk_pixbuf_save(image, fname, "jpeg", &error, NULL);
      g_object_unref(image);
      if(error != NULL)
        return 0;
    }
  free(fname);
  return 1;
}

static int
classify(const GdkPixbuf *image)
{
  int *matrix, width, height;
  GHashTable *filtered;
  int number;
  GdkPixbuf *binary, *croped;

  binary = toBinary(image);
  croped = cropImage(binary);

  matrix = accum_matrix_from_image_with_length(croped,
                                               &width,
                                               &height);
  filtered = filter_accum_matrix(matrix, width, height);
  number = identify_number(croped, filtered);

  g_object_unref(binary);
  g_object_unref(croped);
  g_hash_table_destroy(filtered);
  free(matrix);

  return number;
}

static void
show_message_box(GtkBuilder *builder,
                 const gchar *msg,
                 GtkMessageType type)
{
  GtkWidget *parent, *dialog;

  parent = GTK_WIDGET(gtk_builder_get_object(builder,
                                             "mainwindow"));
  dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type,
                                  GTK_BUTTONS_CLOSE,
                                  msg, NULL);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

static void
on_click(GtkButton *button,
         gpointer data)
{
  GtkImage *image;
  GtkBuilder *builder;
  GdkPixbuf *pbuf;
  int number;
  gchar *message;

  builder = GTK_BUILDER(data);
  image = GTK_IMAGE(gtk_builder_get_object(builder,
                                           "image"));
  pbuf = gtk_image_get_pixbuf(image);
  number = classify(pbuf);

  message = malloc(MAX_STRING_SIZE);
  if(number != -1)
    {
      sprintf(message, "Распознана цифра %i", number);
      show_message_box(builder, message, GTK_MESSAGE_INFO);
    }
  else
    show_message_box(builder,
                     "Ошибка: не удалось распознать цифру",
                     GTK_MESSAGE_ERROR);
  free(message);
}

static void
generate_digits_activated(GSimpleAction *action,
                         GVariant *variant,
                         gpointer data)
{
  GtkWidget *parent;
  GtkWidget *dialog;
  const gchar *folder;
  int response, result;

  parent = GTK_WIDGET(data);
  dialog = gtk_file_chooser_dialog_new("Выберите папку для сохранения",
                                       GTK_WINDOW(parent),
                                       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                       "OK", GTK_RESPONSE_ACCEPT,
                                       "Отмена", GTK_RESPONSE_CANCEL,
                                       NULL);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  if(response == GTK_RESPONSE_ACCEPT)
    {
      folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      result = draw_and_save_10_digits(folder);
    }
  gtk_widget_destroy(dialog);

  if(!result)
    show_message_box(builder,
                     "Ошибка: не удалось сгенерировать изображения",
                     GTK_MESSAGE_ERROR);
  else
    show_message_box(builder,
                     "Изображения успешно сгенерированы!",
                     GTK_MESSAGE_INFO);
}

static void
put_noise (GSimpleAction *action,
           GVariant *variant,
           gpointer data)
{
  GtkImage *image;
  GdkPixbuf *pbuf, *modified;

  image = GTK_IMAGE(data);
  pbuf = gtk_image_get_pixbuf(image);
  modified = noise(pbuf);
//  modified = breach(pbuf);
  gtk_image_set_from_pixbuf(image, modified);
}

static GdkPixbuf *draw_rect(GdkPixbuf *image,
                            struct rect *roi)
{
  cairo_surface_t *surface;
  int s_width, s_height;
  GdkPixbuf *res;
  cairo_t *context;
  double dashes[] = {4, 8};

  surface = gdk_cairo_surface_create_from_pixbuf(image, 0, NULL);
  context = cairo_create(surface);
  cairo_set_line_width (context, 2.0);
  cairo_set_line_cap(context, CAIRO_LINE_CAP_ROUND);
  cairo_set_source_rgb (context, 255, 255, 255);
  cairo_set_dash(context, dashes, 2, 0);
  cairo_rectangle (context, roi->x, roi->y,
                   roi->width, roi->height);
  cairo_fill(context);
  cairo_destroy(context);

  s_width = cairo_image_surface_get_width(surface);
  s_height = cairo_image_surface_get_height(surface);
  res = gdk_pixbuf_get_from_surface(surface,
                                    0, 0,
                                    s_width,
                                    s_height);
  cairo_surface_destroy(surface);
  return res;
}

static void redraw(GtkImage *image,
                 struct rect *roi)
{
  GdkPixbuf *src_img, *res_img;
  src_img = gtk_image_get_pixbuf(image);
  res_img = draw_rect(src_img, roi);
  gtk_image_set_from_pixbuf(image, res_img);

  g_object_unref(res_img);
}

static void
on_mouse_down(GtkWidget *widget,
              GdkEvent *event,
              gpointer data)
{
  GdkEventButton *btn_event;
  GtkAllocation alloc;
  GtkWidget  *image;
  GtkBuilder *builder;
  struct point center, img_top_left;
  GdkPixbuf *pbuf;
  int img_width, img_height;
  gboolean image_empty;

  btn_event = (GdkEventButton*)event;
  builder = GTK_BUILDER(data);
  image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));
  image_empty = gtk_image_get_storage_type(GTK_IMAGE(image)) == GTK_IMAGE_EMPTY;
//GABE HElp ME
  if(btn_event->button == GDK_BUTTON_PRIMARY && !image_empty)
    {
      pbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));

      mouse_flag = MOUSE_DOWN;
      gtk_widget_get_allocation(image, &alloc);
      center.x = alloc.width / 2;
      center.y = alloc.height / 2;
      img_width = gdk_pixbuf_get_width(pbuf);
      img_height = gdk_pixbuf_get_height(pbuf);
      img_top_left.x = center.x - img_width / 2;
      img_top_left.y = center.y - img_height / 2;

      start_pos.x = btn_event->x - img_top_left.x;
      start_pos.y = btn_event->y - img_top_left.y;
      ROI_UNSET;
    }
}

static void
on_mouse_move(GtkWidget *widget,
              GdkEvent *event,
              gpointer data)
{
  GdkEventMotion *m_event;
  struct point current_pos;
  GtkBuilder *builder;
  GtkWidget *image;
  GdkPixbuf *pbuf;
  int img_width, img_height, diff;
  struct point center, img_top_left;
  GtkAllocation alloc;
  struct rect cairo_roi;

  m_event = (GdkEventMotion*)event;
  builder = GTK_BUILDER(data);
  image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));
  if(mouse_flag == MOUSE_DOWN)
    {
      pbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));

      gtk_widget_get_allocation(image, &alloc);
      center.x = alloc.width / 2 + alloc.x;
      center.y = alloc.height / 2 + alloc.y;
      img_width = gdk_pixbuf_get_width(pbuf);
      img_height = gdk_pixbuf_get_height(pbuf);
      img_top_left.x = center.x - img_width / 2;
      img_top_left.y = center.y - img_height / 2;

      current_pos.x = m_event->x - img_top_left.x;
      current_pos.y = m_event->y - img_top_left.y;

      cairo_roi.x = (current_pos.x - start_pos.x) > 0 ? start_pos.x :
                                                  current_pos.x;
      cairo_roi.y = (current_pos.y - start_pos.y) > 0 ? start_pos.y :
                                                  current_pos.y;
      cairo_roi.width = abs(current_pos.x - start_pos.x);
      cairo_roi.height = abs(current_pos.y - start_pos.y);

      cairo_roi.x  = cairo_roi.x < 0 ? 0 : cairo_roi.x;
      cairo_roi.width = (diff = (cairo_roi.x + cairo_roi.width - img_width)) > 0 ? cairo_roi.width - diff :
                                                                                   cairo_roi.width;
      cairo_roi.y  = cairo_roi.y < 0 ? 0 : cairo_roi.y;
      cairo_roi.height = (diff = (cairo_roi.y + cairo_roi.height - img_height)) > 0 ? cairo_roi.height - diff :
                                                                                      cairo_roi.height;

      INIT_RECT(roi, cairo_roi.x, cairo_roi.y, cairo_roi.width, cairo_roi.height);
      ROI_SET;
      redraw(GTK_IMAGE(image), &cairo_roi);
    }
}

static void
on_mouse_up(GtkWidget *widget,
            GdkEvent *event,
            gpointer data)
{
  GdkEventButton *b_event;

  b_event = (GdkEventButton*)event;
  if(b_event->button == GDK_BUTTON_PRIMARY)
      mouse_flag = MOUSE_UP;
}


static GActionEntry win_entries[] =
{
  {"generate", generate_digits_activated, NULL, NULL, NULL}
};

static GActionEntry img_entries[] =
{
  {"noise",     put_noise,                 NULL, NULL, NULL}
};

static void
setup_button_signals(GtkBuilder *builder)
{
  GtkWidget *open_button;
  GtkWidget *recog_button;

  open_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "openbutton"));
  g_signal_connect(GTK_BUTTON(open_button), "clicked",
                   G_CALLBACK(on_open_image), builder);

  recog_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "recognize"));
  g_signal_connect(GTK_BUTTON(recog_button), "clicked",
                   G_CALLBACK(on_click), builder);
  gtk_widget_set_sensitive(recog_button, FALSE);
}

static void
add_action_entries (GtkBuilder *builder)
{
  GtkApplicationWindow *window;
  GtkImage *image;

  image = GTK_IMAGE(gtk_builder_get_object(builder, "image"));
  window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder,
                                                         "mainwindow"));

  g_action_map_add_action_entries(G_ACTION_MAP(window),
                                  win_entries, G_N_ELEMENTS(win_entries),
                                  window);
  g_action_map_add_action_entries(G_ACTION_MAP(window),
                                  img_entries, G_N_ELEMENTS(img_entries),
                                  image);
}

static void
setup_image_widget(GtkBuilder *builder)
{
  GtkWidget *image, *eventbox;
  gint events, new_events;

  image = GTK_WIDGET(gtk_builder_get_object(builder,
                                            "image"));
  eventbox = GTK_WIDGET(gtk_builder_get_object(builder,
                                               "eventbox"));

  new_events = 0;
  events= gtk_widget_get_events(eventbox);
  if((events & GDK_BUTTON_PRESS_MASK) == 0)
    new_events |= GDK_BUTTON_PRESS_MASK;
  if((events & GDK_BUTTON_RELEASE_MASK) == 0)
    new_events |= GDK_BUTTON_RELEASE_MASK;
  if((events & GDK_POINTER_MOTION_MASK) == 0)
    new_events |= GDK_POINTER_MOTION_MASK;
  gtk_widget_add_events(eventbox, new_events);

  g_signal_connect(eventbox, "button-press-event",
                   G_CALLBACK(on_mouse_down), builder);
  g_signal_connect(eventbox, "button-release-event",
                   G_CALLBACK(on_mouse_up), NULL);
  g_signal_connect(eventbox, "motion-notify-event",
                   G_CALLBACK(on_mouse_move), builder);
}

static void
setup_menu (GtkBuilder *builder)
{
  GtkWidget *menu_button;
  GMenu *menu;

  menu_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "menubutton"));
  menu = g_menu_new();
  g_menu_append(menu, "Генерировать образцы", "win.generate");
  g_menu_append(menu, "Добавить шум", "win.noise");
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button),
                                 G_MENU_MODEL(menu));
}

void
on_startup(GtkApplication *app,
           gpointer data)
{
  builder = gtk_builder_new_from_file(ui_path);
  add_action_entries(builder);
  setup_menu (builder);
  setup_button_signals(builder);
  setup_image_widget(builder);
}

void
on_shutdown(GtkApplication *app,
            gpointer data)
{
  g_object_unref(builder);
}

void
on_activate(GtkApplication *app,
            gpointer data)
{
  GtkWidget *window;

  window = GTK_WIDGET(gtk_builder_get_object(builder,
                                             "mainwindow"));
  gtk_application_add_window(app, GTK_WINDOW(window));
  gtk_widget_show_all(window);
}
