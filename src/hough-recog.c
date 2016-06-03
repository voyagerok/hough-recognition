#include "hough-recog.h"
#include <math.h>
#include <stdlib.h>

#define SQUARE(x) ((x) * (x))
#define RADIAN(angle, pi) ((float)(angle) * pi / 180)

#define MAX_ANGLE 90
#define ANGLE_STEP 45
#define EPS 0.0001
#define DIAG_LENGTH(width , height) (round(sqrt(((width) - 1) * ((width) - 1) +\
  ((height) - 1) * ((height) - 1))))
#define MAX_DISTANCE(diag) (round(sqrt(2) * (diag)))
#define THRESHOLD 100
#define DIST_DIFF_THRESHOLD 50
#define DIAG_ANGLE 45
#define N_OF_MAX 7

int*
accum_matrix_from_image_with_length(const GdkPixbuf *image,
                                    int *matrix_width,
                                    int *matrix_height)
{
  int *matrix, width, height;
  int rowstride, channels, diag;
  guchar *pixels;
  int max_distance, min_distance;
  int max_angle, min_angle, index;
  int matrix_size, matr_width, matr_height;

  width = gdk_pixbuf_get_width(image);
  height = gdk_pixbuf_get_height(image);
  rowstride = gdk_pixbuf_get_rowstride(image);
  channels = gdk_pixbuf_get_n_channels(image);
  pixels = gdk_pixbuf_get_pixels(image);

  diag = round(sqrt(SQUARE(width - 1) + SQUARE(height - 1)));
//  diag2 = DIAG_LENGTH(width, height);
//  if(diag != diag2)
//    g_print("bad");
  max_distance = MAX_DISTANCE(diag);
  min_distance = -max_distance;
  max_angle = MAX_ANGLE;
  min_angle = -max_angle;

  matr_width = max_distance * 2 + 1;
  //matr_height = (max_angle * 2) / ANGLE_STEP + 1;
  matr_height = (max_angle * 2) / ANGLE_STEP;
  matrix_size = matr_width * matr_height;
  matrix = calloc(matrix_size, sizeof (int));

  for(int i = 0; i < height; ++i)
    for(int j = 0; j < width; ++j)
      {
        index = i * rowstride + j * channels;
        if(pixels[index] != 0)
          continue;
        for(int angle = min_angle; angle < max_angle/*angle <= max_angle*/; angle+= ANGLE_STEP)
          {
            float phi = RADIAN(angle, M_PI);
            float distance = i * sin(phi) + j * cos(phi);
            if(distance >= min_distance &&
               distance <= max_distance)
              {
                index = (angle + max_angle) / ANGLE_STEP *
                    matr_width + (distance + max_distance);
                matrix[index]++;
              }
          }
      }

  *matrix_width = matr_width;
  *matrix_height = matr_height;
  return matrix;
}


typedef struct slist_value
{
  int points;
  int dist;
} sl_value;

typedef struct dist_flag_pair
{
  int dist;
  int flag;
} df_pair;

static int
contains_line (GSList *list,
            int dist)
{
  GSList *iter;
  sl_value *value;

  for(iter = list; iter != NULL; iter = iter->next)
    {
      value = (sl_value*)iter->data;
      if(abs(value->dist - dist) < DIST_DIFF_THRESHOLD)
        return 1;
    }
  return 0;
}

static void
free_htable_elems(gpointer elem)
{
  g_slist_free_full((GSList*)elem, free);
}

GHashTable*
filter_accum_matrix(const int *matrix,
                    int width,
                    int height)
{
  GHashTable *table;
  int matr_size;
  int angle, dist;
  int n_of_points;

  /* TODO: добавить функцию очистки памяти */
  table = g_hash_table_new_full(g_direct_hash,
                                g_direct_equal,
                                NULL,
                                NULL);
  matr_size = width * height;

  for(int i = 0; i < matr_size; ++i)
    {
      n_of_points = matrix[i];
      if(n_of_points > THRESHOLD)
        {
          GSList *lines = NULL;
          sl_value *new_line;

          angle = (i / width) * ANGLE_STEP - MAX_ANGLE;
          dist = abs((i % width) - (width - 1) / 2);
          lines = (GSList*)g_hash_table_lookup(table, GINT_TO_POINTER(angle));
          if(!contains_line(lines, dist))
            {
              new_line = malloc(sizeof(sl_value));
              new_line->dist = dist;
              new_line->points = n_of_points;
              lines = g_slist_prepend(lines, new_line);
              g_hash_table_insert(table, GINT_TO_POINTER(angle), lines);
            }
        }
    }
  return table;

}

static void
count_lines (gpointer key,
     gpointer value,
     gpointer user_data)
{
  GSList *val;
  int *sum;

  val = (GSList*)value;
  sum = (int*)user_data;

  *sum += g_slist_length(val);
}

static void
check_diag (gpointer key,
            gpointer value,
         gpointer user_data)
{
  int angle;
  int *is_diag;

  angle = GPOINTER_TO_INT(key);
  is_diag = (int*)user_data;

  if(!(*is_diag))
    *is_diag = abs(angle) == DIAG_ANGLE;

}

static void
count_diags (gpointer key,
             gpointer value,
             gpointer user_data)
{
  int angle;
  GSList *lines;
  int *diags;

  angle = GPOINTER_TO_INT(key);
  lines = (GSList*)value;
  diags = (int*)user_data;

  if(abs(angle) == DIAG_ANGLE)
    *diags += g_slist_length(lines);
}


static void
get_first_diag_dist (gpointer key,
                     gpointer value,
                     gpointer user_data)
{
  int angle;
  GSList *lines;
  sl_value *line;
  int *dist;

  angle = GPOINTER_TO_INT(key);
  lines = (GSList*)value;
  dist = (int*)user_data;

  if(abs(angle) == DIAG_ANGLE)
    if(lines != NULL)
      {
        line = (sl_value*)lines->data;
        *dist = line->dist;
      }
}

typedef struct angle_sum_pair
{
  int angle;
  int sum;
} as_pair;

static void
count_points_by_angle (gpointer key,
                     gpointer value,
                     gpointer user_data)
{
  int angle;
  GSList *lines, *iter;
  sl_value *line;
  as_pair *pair;

  angle = GPOINTER_TO_INT(key);
  lines = (GSList*)value;
  pair = (as_pair*)user_data;

  if(abs(angle) == pair->angle)
    {
      for(iter = lines; iter; iter = iter->next)
        {
          line = (sl_value*)iter->data;
          pair->sum += line->points;
        }
    }
}

int
identify_number(GdkPixbuf *image, GHashTable *table)
{
  int n_of_lines, n_of_diags;
  int has_diag, img_width, img_height;
  int img_diag_length;
  int first_diag_line_dist;
#define FOREACH(seq, func, acc)\
  {\
    acc = 0;\
    g_hash_table_foreach(seq, func, &acc);\
  }

  img_width = gdk_pixbuf_get_width(image);
  img_height = gdk_pixbuf_get_height(image);
  img_diag_length = DIAG_LENGTH(img_width,
                                img_height);
  FOREACH(table, count_lines, n_of_lines);

  switch (n_of_lines)
    {
    case 2:
      return 1;
    case 3:
      {
        FOREACH(table, check_diag, has_diag);
        if(has_diag)
          return 7;
        else
          return 4;
      }
    case 4:
      {
        FOREACH(table, check_diag, has_diag);
        if(has_diag)
          {
            FOREACH(table, count_diags, n_of_diags);
            if(n_of_diags == 2)
              return 3;
            else
              return 2;
          }
        else
          return 0;
      }
    case 5:
      {
        FOREACH(table, check_diag, has_diag);
        if(has_diag)
          {
            FOREACH(table, get_first_diag_dist, first_diag_line_dist);
            if(first_diag_line_dist < img_diag_length / 2)
              return 6;
            else
              return 9;
          }
        else
          {
            as_pair pair;
            pair.angle = 0;
            pair.sum = 0;
            g_hash_table_foreach(table, count_points_by_angle, &pair);

            if((pair.sum - img_height) < (img_height / 2))
              return 5;
            else
              return 8;
          }
      }
    default:
      break;
    }

  return -1;
}
