#ifndef IMGPROC_H
#define IMGPROC_H

#include <gtk/gtk.h>
#include <opencv2/core/core_c.h>

#define CHANNEL_DEPTH 8
#define N_CHANNELS_RGB 3
#define N_CHANNELS_RGBA 4
#define N_CHANNELS_GRAY 1


GdkPixbuf*
toBinary(const GdkPixbuf *image);
GdkPixbuf*
cropImage(const GdkPixbuf *image);
GdkPixbuf*
canny_detector(const GdkPixbuf *image);
GdkPixbuf*
draw_digit(int digit);
GdkPixbuf*
noise (const GdkPixbuf *image);
GdkPixbuf*
breach (const GdkPixbuf *image);

#endif // IMGPROC_H
