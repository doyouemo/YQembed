#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// 外部变量声明
extern int *lcd_ptr;
extern int lcd_fd, ts_fd;

// 设备初始化/反初始化
int dev_init();
int dev_uninit();

// 基本绘图函数
int lcd_draw_point(int i, int j, int color);

// 图像显示函数
int show_image(const char *pathname, int x, int y, int dst_w, int dst_h);
int lcd_draw_scale_bmp(const char *pathname, int x, int y, int dst_w, int dst_h);
int lcd_draw_scale_jpeg(const char *pathname, int x, int y, int dst_w, int dst_h);
int lcd_draw_scale_png(const char *pathname, int x, int y, int dst_w, int dst_h);
int ShowGifAnimation(const char *path, int x, int y, int dst_w, int dst_h);

// JPEG解码函数
int read_JPEG_file(const char *filename, int *image_width, int *image_height, char **out_buffer);

#endif // DISPLAY_H