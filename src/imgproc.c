#include "imgproc.h"
#include <stdlib.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <time.h>

#define THICKNESS 25
#define STEP_RATIO 40
#define BREACH_RADIUS 200

static IplImage *
pixbuf2ipl(const GdkPixbuf *image)
{
  IplImage *res_image;
  int width, height;
  int depth, n_channels;
  int stride, res_img_stride;
  guchar *image_data;
  guchar *res_image_data;
  gboolean has_alpha;

  width = gdk_pixbuf_get_width(image);
  height = gdk_pixbuf_get_height(image);
  depth = gdk_pixbuf_get_bits_per_sample(image);
  n_channels = gdk_pixbuf_get_n_channels(image);
  stride = gdk_pixbuf_get_rowstride(image);
  has_alpha = gdk_pixbuf_get_has_alpha(image);

  g_assert(depth == CHANNEL_DEPTH);

  image_data = gdk_pixbuf_get_pixels(image);
  res_image = cvCreateImage(cvSize(width, height),
                            depth, n_channels);
  res_image_data = (guchar*)res_image->imageData;
  res_img_stride = res_image->widthStep;

  for(int i = 0; i < height; ++i)
    for(int j = 0; j < width; ++j)
      {
        int index = i * res_img_stride + j * n_channels;
        res_image_data[index] = image_data[index + 2];
        res_image_data[index + 1] = image_data[index + 1];
        res_image_data[index + 2] = image_data[index];
      }

  return res_image;
}

static GdkPixbuf *
ipl2pixbuf(const IplImage *image)
{
  uchar *imageData;
  guchar *pixbufData;
  int widthStep, n_channels, res_stride;
  int width, height, depth, res_n_channels;
  int data_order;
  GdkPixbuf *res_image;
  long ipl_depth;
  CvSize roi;

  cvGetRawData(image, &imageData, &widthStep, &roi);
  width = roi.width;
  height = roi.height;
  n_channels = image->nChannels;
  data_order = image->dataOrder;

  g_assert(data_order == IPL_DATA_ORDER_PIXEL);
  g_assert(n_channels == N_CHANNELS_RGB  ||
           n_channels == N_CHANNELS_RGBA ||
           n_channels == N_CHANNELS_GRAY);

  switch(ipl_depth = image->depth)
    {
    case IPL_DEPTH_8U:
      depth = 8;
      break;
    default:
      depth = 0;
      break;
    }
  g_assert(depth == CHANNEL_DEPTH);

  res_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                             depth, width, height);
  pixbufData = gdk_pixbuf_get_pixels(res_image);
  res_stride = gdk_pixbuf_get_rowstride(res_image);
  res_n_channels = N_CHANNELS_RGB;

  for(int i = 0; i < height; ++i)
    for(int j = 0; j < width; ++j)
      {
        int index = i * widthStep + j * n_channels;
        int res_index = i * res_stride + j * res_n_channels;
        if(n_channels == N_CHANNELS_GRAY)
          pixbufData[res_index] = pixbufData[res_index + 1] =
              pixbufData[res_index + 2] = imageData[index];
        else
          {
            pixbufData[res_index] = imageData[index + 2];
            pixbufData[res_index + 1] = imageData[index + 1];
            pixbufData[res_index + 2] = imageData[index];
          }
      }
  return res_image;
}

GdkPixbuf *
toBinary(const GdkPixbuf *image)
{
  IplImage *cvimage, *grayimage;
  GdkPixbuf *res_image;

  cvimage = pixbuf2ipl(image);
  grayimage = cvCreateImage(cvSize(cvimage->width,
                                   cvimage->height),
                            cvimage->depth,
                            N_CHANNELS_GRAY);
  cvCvtColor(cvimage, grayimage, CV_BGR2GRAY);
  cvThreshold(grayimage, grayimage,
              127, 255, CV_THRESH_BINARY);
  res_image = ipl2pixbuf(grayimage);
  cvReleaseImage(&cvimage);
  cvReleaseImage(&grayimage);

  return res_image;
}

static void
crop(const GdkPixbuf *image,
     int *x, int *y,
     int *width,
     int *height)
{
  int rowstride;
  int image_width;
  int image_height;
  guchar *pixels;
  struct point
  {
    int x,y;
  } top;
  struct point right;
  struct point left;
  struct point bottom;
  int index, n_channels;

  rowstride = gdk_pixbuf_get_rowstride(image);
  image_width = gdk_pixbuf_get_width(image);
  image_height = gdk_pixbuf_get_height(image);
  pixels = gdk_pixbuf_get_pixels(image);
  n_channels = gdk_pixbuf_get_n_channels(image);

  /* top */
  for(int i = 0; i < image_height; ++i)
    for(int j = 0; j < image_width; ++j)
      {
        index = i * rowstride + j * n_channels;
        if(pixels[index] == 0)
          {
            top.x = j;
            top.y = i;
            goto left;
          }
      }

left:
  for(int i = 0; i < image_width; ++i)
    for(int j = 0; j < image_height; ++j)
      {
        index = j * rowstride + i * n_channels;
        if(pixels[index] == 0)
          {
            left.x = i;
            left.y = j;
            goto bottom;
          }
      }

bottom:
  for(int i = image_height - 1; i >= 0; --i)
    for(int j = 0; j < image_width; ++j)
      {
        index = i * rowstride + j * n_channels;
        if(pixels[index] == 0)
          {
            bottom.x = j;
            bottom.y = i;
            goto right;
          }
      }

right:
  for(int i = image_width - 1; i >= 0; --i)
    for(int j = 0; j < image_height; ++j)
      {
        index = j * rowstride + i * n_channels;
        if(pixels[index] == 0)
          {
            right.x = i;
            right.y = j;
            goto end;
          }
      }

end:
  *x = left.x;
  *y = top.y;
  *width = right.x - left.x + 1;
  *height = bottom.y - top.y + 1;
}

static GdkPixbuf*
get_image_from_ROI(const GdkPixbuf *image,
                   int x, int y,
                   int width, int height)
{
  GdkPixbuf *dest;
  gboolean has_alpha;
  int depth, src_stride, dst_stride;
  guchar *src_pixels, *dst_pixels;
  int src_index, dst_index, n_channels;

  has_alpha = gdk_pixbuf_get_has_alpha(image);
  depth = gdk_pixbuf_get_bits_per_sample(image);
  src_pixels = gdk_pixbuf_get_pixels(image);
  src_stride = gdk_pixbuf_get_rowstride(image);
  n_channels = gdk_pixbuf_get_n_channels(image);

  dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                        has_alpha, depth,
                        width, height);
  dst_stride = gdk_pixbuf_get_rowstride(dest);
  dst_pixels = gdk_pixbuf_get_pixels(dest);

  for(int i = 0; i < height; ++i)
    for(int j = 0; j < width; ++j)
      {
        src_index = (i + y) * src_stride + n_channels * (j + x);
        dst_index = i * dst_stride + n_channels * j;

        dst_pixels[dst_index] = src_pixels[src_index];
        dst_pixels[dst_index + 1] = src_pixels[src_index + 1];
        dst_pixels[dst_index + 2] = src_pixels[src_index + 2];
      }

  return dest;
}

GdkPixbuf*
cropImage(const GdkPixbuf *image)
{
  GdkPixbuf *cropped;
  int x, y, width, height;

  crop(image, &x, &y, &width, &height);
  cropped = get_image_from_ROI(image, x, y, width, height);

  return cropped;
}

GdkPixbuf*
canny_detector(const GdkPixbuf *image)
{
  GdkPixbuf *res;
  IplImage *cvimage, *canny;
  IplImage *binary;

  cvimage = pixbuf2ipl(image);
  binary = cvCreateImage(cvGetSize(cvimage),
                         cvimage->depth,
                         N_CHANNELS_GRAY);
  canny = cvCreateImage(cvGetSize(cvimage),
                        cvimage->depth,
                        N_CHANNELS_GRAY);
  cvCvtColor(cvimage, binary, CV_BGR2GRAY);
  cvCanny(binary, canny, 10, 30, 3);
  res = ipl2pixbuf(canny);
  cvReleaseImage(&cvimage);
  cvReleaseImage(&binary);
  cvReleaseImage(&canny);

  return res;
}

#define DRAW_ZERO(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, height - 1),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, height - 1),\
         cvPoint(0, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_ONE(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_TWO(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, height - 1),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_THREE(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(width -1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width -1, (height - 1) / 2),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_FOUR(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_FIVE(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(0, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, height - 1),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_SIX(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, height - 1),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, height - 1),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_SEVEN(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_EIGHT(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, 0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(width - 1, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, height - 1),\
         cvPoint(0, height - 1),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, height - 1),\
         cvPoint(0, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }

#define DRAW_NINE(cvimage, thickness, width, height)\
{\
  cvLine(cvimage, cvPoint(0, height - 1),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, (height - 1) / 2),\
         cvPoint(0, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0, (height - 1) / 2),\
         cvPoint(0,0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(0,0),\
         cvPoint(width - 1, 0),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  cvLine(cvimage, cvPoint(width - 1, 0),\
         cvPoint(width - 1, (height - 1) / 2),\
         cvScalar(0,0,0,0), thickness, 8, 0);\
  }


GdkPixbuf*
draw_digit(int digit)
{
  GdkPixbuf *res;
  IplImage *cvimage;
  int width, height;

  cvimage = cvCreateImage(cvSize(200, 400),
                          CHANNEL_DEPTH,
                          N_CHANNELS_GRAY);
  width = cvimage->width;
  height = cvimage->height;
  cvSet(cvimage, cvScalar(255,255,255,255), NULL);

  switch (digit)
    {
    case 0:
      DRAW_ZERO(cvimage, THICKNESS, width, height);
      break;
    case 1:
      DRAW_ONE(cvimage, THICKNESS, width, height);
      break;
    case 2:
      DRAW_TWO(cvimage, THICKNESS, width, height);
      break;
    case 3:
      DRAW_THREE(cvimage, THICKNESS, width, height);
      break;
    case 4:
      DRAW_FOUR(cvimage, THICKNESS, width, height);
      break;
    case 5:
      DRAW_FIVE(cvimage, THICKNESS, width, height);
      break;
    case 6:
      DRAW_SIX(cvimage, THICKNESS, width, height);
      break;
    case 7:
      DRAW_SEVEN(cvimage, THICKNESS, width, height);
      break;
    case 8:
      DRAW_EIGHT(cvimage, THICKNESS, width, height);
      break;
    case 9:
      DRAW_NINE(cvimage, THICKNESS, width, height);
      break;
    default:
      break;
    }

  res = ipl2pixbuf(cvimage);
  return res;
}

GdkPixbuf*
noise (const GdkPixbuf *image)
{
  int width, height;
  guchar *pixels, *res_pix;
  int channels, stride, depth;
  int res_stride, rand_val;
  int s_index, d_index;
  GdkPixbuf *res;
  gboolean has_alpha;

  width = gdk_pixbuf_get_width(image);
  height = gdk_pixbuf_get_height(image);
  pixels = gdk_pixbuf_get_pixels(image);
  channels = gdk_pixbuf_get_n_channels(image);
  stride = gdk_pixbuf_get_rowstride(image);
  has_alpha = gdk_pixbuf_get_has_alpha(image);
  depth = gdk_pixbuf_get_bits_per_sample(image);

  res = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                       has_alpha, depth,
                       width, height);
  res_pix = gdk_pixbuf_get_pixels(res);
  res_stride = gdk_pixbuf_get_rowstride(res);

  srand(time(NULL));

  int width_step = width / STEP_RATIO;
  int height_step = height / STEP_RATIO;

  for(int i = 0; i < height; i += height_step)
    for(int j = 0; j < width; j += width_step)
      {

        rand_val = rand() % 18;

        for(int k = 0; k < height && k < height_step; ++k)
          for(int l = 0; l < width && l < width_step; ++l)
            {
              s_index = (k + i) * stride + (l + j) * channels;
              d_index = (k + i) * res_stride + (l + j) * channels;

              if(!rand_val)
                {
                  res_pix[d_index] = (1 << depth) - pixels[s_index] - 1;
                  res_pix[d_index + 1] = (1 << depth) - pixels[s_index + 2] - 1;
                  res_pix[d_index + 2] = (1 << depth) - pixels[s_index + 2] - 1;
                }
              else
                {
                  res_pix[d_index] = pixels[s_index];
                  res_pix[d_index + 1] = pixels[s_index + 1];
                  res_pix[d_index + 2] = pixels[s_index + 2];
                }
            }


      }
  return res;
}

GdkPixbuf*
breach (const GdkPixbuf *image)
{
  int width, height;
  guchar *pixels, *res_pix;
  int channels, stride, depth;
  int res_stride, rand_val;
  int s_index, d_index;
  GdkPixbuf *res;
  gboolean has_alpha;

  width = gdk_pixbuf_get_width(image);
  height = gdk_pixbuf_get_height(image);
  pixels = gdk_pixbuf_get_pixels(image);
  channels = gdk_pixbuf_get_n_channels(image);
  stride = gdk_pixbuf_get_rowstride(image);
  has_alpha = gdk_pixbuf_get_has_alpha(image);
  depth = gdk_pixbuf_get_bits_per_sample(image);

  res = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                       has_alpha, depth,
                       width, height);
  res_pix = gdk_pixbuf_get_pixels(res);
  res_stride = gdk_pixbuf_get_rowstride(res);

  srand(time(NULL));

  for (int i = 0; i < height; ++i)
    for (int j = 0; j < width; ++j)
      {
        s_index = i * stride + j * channels;
        if (pixels[s_index] == 0)
          {
            rand_val = rand() % 50;
            if (!rand_val)
              {
                int start_x = (i - BREACH_RADIUS) < 0 ? 0 : i - BREACH_RADIUS;
                int end_x = (i + BREACH_RADIUS) >= width ? width - 1 : i + BREACH_RADIUS;
                int start_y = (j - BREACH_RADIUS) < 0 ? 0 : j - BREACH_RADIUS;
                int end_y = (j + BREACH_RADIUS) >= height ? height - 1 : j + BREACH_RADIUS;

                for (int y = start_y; y <= end_y; ++y)
                  for (int x = start_x; x <= end_x; ++x)
                    {
                      s_index = y * stride + x * channels;
                      if (pixels[s_index] == 0)
                        {
                          d_index = y * res_stride + x * channels;
                          res_pix[d_index] = res_pix[d_index + 1] = res_pix[d_index + 2] = 255;
                        }
                    }
              }
          }
        else
          {
            rand_val = rand() % 30;
            d_index = i * res_stride + j * channels;
            if (!rand_val)
              {
                res_pix[d_index] = (1 << depth) - pixels[s_index] - 1;
                res_pix[d_index + 1] = (1 << depth) - pixels[s_index + 2] - 1;
                res_pix[d_index + 2] = (1 << depth) - pixels[s_index + 2] - 1;
              }
            else
              {
                res_pix[d_index] = pixels[s_index];
                res_pix[d_index + 1] = pixels[s_index + 1];
                res_pix[d_index + 2] = pixels[s_index + 2];
              }
          }
      }
  return res;
}

#undef DRAW_ZERO
#undef DRAW_ONE
#undef DRAW_TWO
#undef DRAW_THREE
#undef DRAW_FOUR
#undef DRAW_FIVE
#undef DRAW_SIX
#undef DRAW_SEVEN
#undef DRAW_EIGHT
#undef DRAW_NINE

