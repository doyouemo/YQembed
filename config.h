#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <unistd.h>

#include <gif_lib.h>

// 定义屏幕尺寸
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// 定义 MIN 宏
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// BGRA颜色结构体
typedef struct tagBGRA
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char transparency;
} BGRA;

// 图像结构体
typedef struct tagIMAGE
{
    unsigned int w;
    unsigned int h;
    BGRA *color;
} IMAGE;

// BMP文件头结构体
#pragma pack(push, 1)
typedef struct tagBITMAP_HEAD_INFO
{
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
    unsigned int biSize;
    unsigned int biWidth;
    unsigned int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    unsigned int biXPelsPerMeter;
    unsigned int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} BITMAP_HEAD_INFO;
#pragma pack(pop)

// PNG块结构
typedef struct
{
    unsigned int length;
    char type[4];
    unsigned char *data;
    unsigned int crc;
} PNG_CHUNK;

// PNG文件头信息
typedef struct
{
    unsigned int width;
    unsigned int height;
    unsigned char bit_depth;
    unsigned char color_type;
    unsigned char compression;
    unsigned char filter;
    unsigned char interlace;
} PNG_HEADER;

// 错误对象收集结构体
struct my_error_mgr
{
    struct jpeg_error_mgr pub; /* "public" fields */
    jmp_buf setjmp_buffer;     /* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;

// 外部变量声明
extern int *lcd_ptr;
extern int lcd_fd, ts_fd;

// 外部函数声明
extern int lcd_draw_point(int i, int j, int color);
extern int dev_init();
extern int dev_uninit();
extern int show_image(const char *pathname, int x, int y, int dst_w, int dst_h);
extern int read_JPEG_file(const char *filename, int *image_width, int *image_height, char **out_buffer);
extern int lcd_draw_scale_jpeg(const char *pathname, int x, int y, int dst_w, int dst_h);
extern int lcd_draw_scale_bmp(const char *pathname, int x, int y, int dst_w, int dst_h);
extern int lcd_draw_scale_png(const char *pathname, int x, int y, int dst_w, int dst_h);

// GIF控制相关全局变量
extern volatile int gif_playing;
extern pthread_t gif_thread;
extern GifFileType *current_gif;

// 常量定义
#define SCALE_MODE_BILINEAR 1

#endif // CONFIG_H