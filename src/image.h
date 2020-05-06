#ifndef IMAGE_H
#define IMAGE_H
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "box.h"
#include "darknet.h"
#include "image_opencv.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
typedef struct {
    int w;
    int h;
    int c;
    float *data;
} image;
*/
float get_color(int c, int x, int max);
void flip_image(Image a);
void draw_box(
    Image a, int x1, int y1, int x2, int y2, float r, float g, float b);
void draw_box_width(
    Image a, int x1, int y1, int x2, int y2, int w, float r, float g, float b);
void draw_bbox(Image a, box bbox, int w, float r, float g, float b);
void draw_label(Image a, int r, int c, Image label, const float* rgb);
void write_label(
    Image a, int r, int c, Image* characters, char* string, float* rgb);
void draw_detections(Image im, int num, float thresh, box* boxes, float** probs,
    char** names, Image** labels, int classes);
void draw_detections_v3(Image im, Detection* dets, int num, float thresh,
    char** names, Image** alphabet, int classes, int ext_output);
Image image_distance(Image a, Image b);
void scale_image(Image m, float s);
// image crop_image(image im, int dx, int dy, int w, int h);
Image random_crop_image(Image im, int w, int h);
Image random_augment_image(
    Image im, float angle, float aspect, int low, int high, int size);
void random_distort_image(
    Image im, float hue, float saturation, float exposure);
// LIB_API image resize_image(image im, int w, int h);
// LIB_API void copy_image_from_bytes(image im, char *pdata);
void fill_image(Image m, float s);
void letterbox_image_into(Image im, int w, int h, Image boxed);
// LIB_API image letterbox_image(image im, int w, int h);
// image resize_min(image im, int min);
Image resize_max(Image im, int max);
void translate_image(Image m, float s);
void normalize_image(Image p);
Image rotate_image(Image m, float rad);
void rotate_image_cw(Image im, int times);
void embed_image(Image source, Image dest, int dx, int dy);
void saturate_image(Image im, float sat);
void exposure_image(Image im, float sat);
void distort_image(Image im, float hue, float sat, float val);
void saturate_exposure_image(Image im, float sat, float exposure);
void hsv_to_rgb(Image im);
// LIB_API void rgbgr_image(image im);
void constrain_image(Image im);
void composite_3d(char* f1, char* f2, char* out, int delta);
int best_3d_shift_r(Image a, Image b, int min, int max);

Image grayscale_image(Image im);
Image threshold_image(Image im, float thresh);

Image collapse_image_layers(Image source, int border);
Image collapse_images_horz(Image* ims, int n);
Image collapse_images_vert(Image* ims, int n);

void show_image(Image p, const char* name);
void show_image_normalized(Image im, const char* name);
void save_image_png(Image im, const char* name);
void save_image(Image p, const char* name);
void show_images(Image* ims, int n, char* window);
void show_image_layers(Image p, char* name);
void show_image_collapsed(Image p, char* name);

void print_image(Image m);

// LIB_API image make_image(int w, int h, int c);
Image make_random_image(int w, int h, int c);
Image make_empty_image(int w, int h, int c);
Image float_to_image_scaled(int w, int h, int c, float* data);
Image float_to_image(int w, int h, int c, float* data);
Image copy_image(Image p);
void copy_image_inplace(Image src, Image dst);
Image load_image(char* filename, int w, int h, int c);
Image load_image_stb_resize(char* filename, int w, int h, int c);
// LIB_API image load_image_color(char *filename, int w, int h);
Image** load_alphabet();

// float get_pixel(image m, int x, int y, int c);
// float get_pixel_extend(image m, int x, int y, int c);
// void set_pixel(image m, int x, int y, int c, float val);
// void add_pixel(image m, int x, int y, int c, float val);
float bilinear_interpolate(Image im, float x, float y, int c);

Image get_image_layer(Image m, int l);

// LIB_API void free_image(image m);
void test_resize(char* filename);
#ifdef __cplusplus
}
#endif

#endif
