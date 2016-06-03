#ifndef HOUGHRECOG_H
#define HOUGHRECOG_H

#include <gtk/gtk.h>

int*
accum_matrix_from_image_with_length(const GdkPixbuf *image,
                                    int *matrix_width,
                                    int *matrix_height);

int
identify_number(GdkPixbuf *image,
         GHashTable *lines);

GHashTable*
filter_accum_matrix(const int *matrix,
                    int width,
                    int height);

void
highlight (GdkPixbuf *image, GHashTable *table);


#endif // HOUGHRECOG_H
