#include <stdio.h>
#include "jpeglib.h"
#include "setjmp.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int width = 0;  // JPEG图像宽度
int height = 0; // JPEG图像高度

/*
 * Here's the routine that will replace the standard error_exit method:
 */

extern int *lcd_ptr;
extern int lcd_fd, ts_fd;
extern int lcd_draw_point(int i, int j, int color);

void jpeg_to_lcd(int x, int y, int img_width, int img_height, char *jpeg_buffer)
{
    int i, j;
    unsigned char r, g, b;
    int color;

    for (j = 0; j < img_height; j++)
    {
        for (i = 0; i < img_width; i++)
        {
            r = jpeg_buffer[(img_width * j + i) * 3 + 0];
            g = jpeg_buffer[(img_width * j + i) * 3 + 1];
            b = jpeg_buffer[(img_width * j + i) * 3 + 2];
            color = r << 16 | g << 8 | b;
            lcd_draw_point(i + x, j + y, color);
        }
    }
}

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr)cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message)(cinfo);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

GLOBAL(int)
read_JPEG_file(const char *filename, int *image_width, int *image_height, char **out_buffer)
{
    /* This struct contains the JPEG decompression parameters and pointers to
     * working space (which is allocated as needed by the JPEG library).
     */
    struct jpeg_decompress_struct cinfo;
    /* We use our private extension JPEG error handler.
     * Note that this struct must live as long as the main JPEG parameter
     * struct, to avoid dangling-pointer problems.
     */
    struct my_error_mgr jerr;
    /* More stuff */
    FILE *infile;      /* source file */
    JSAMPARRAY buffer; /* Output row buffer */
    int row_stride;    /* physical row width in output buffer */

    /* In this example we want to open the input file before doing anything else,
     * so that the setjmp() error recovery below can assume the file is open.
     * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
     * requires it in order to read binary files.
     */

    if ((infile = fopen(filename, "rb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", filename);
        return 0;
    }

    /* Step 1: 分配并初始化解压缩对象 */

    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerr.setjmp_buffer))
    {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.
         */
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }
    /* Now we can initialize the JPEG decompression object. */
    jpeg_create_decompress(&cinfo);

    /* Step 2: 指定解压缩数据源 */

    jpeg_stdio_src(&cinfo, infile);

    /* Step 3: 读取文件头数据 */

    (void)jpeg_read_header(&cinfo, TRUE);
    /* We can ignore the return value from jpeg_read_header since
     *   (a) suspension is not possible with the stdio data source, and
     *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
     * See libjpeg.txt for more info.
     */

    *image_width = cinfo.image_width;
    *image_height = cinfo.image_height;

    /* Step 4: 设置解压参数，可以直接使用默认的解压缩参数 */

    /* In this example, we don't need to change any of the defaults set by
     * jpeg_read_header(), so we do nothing here.
     */

    // unsigned int scale_num, scale_denom; /* 设定解压缩参数比例 */
    // cinfo.scale_num = 1; cinfo.scale_denom = 1; // 原尺寸
    // cinfo.scale_num = 2; cinfo.scale_denom = 1; // 1/2尺寸

    /* Step 5: 开始解压 */

    (void)jpeg_start_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
     * with the stdio data source.
     */

    /* We may need to do some setup of our own at this point before reading
     * the data.  After jpeg_start_decompress() we have the correct scaled
     * output image dimensions available, as well as the output colormap
     * if we asked for color quantization.
     * In this example, we need to make an output work buffer of the right size.
     */
    /* 一行的数据大小 */
    row_stride = cinfo.output_width * cinfo.output_components;

    *out_buffer = calloc(1, row_stride * cinfo.image_height);

    /* 一行数据的存储缓冲区 */
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    char *dst = *out_buffer;

    /* Step 6: 循环读取每一行数据 */

    /* Here we use the library's state variable cinfo.output_scanline as the
     * loop counter, so that we don't have to keep track ourselves.
     */
    while (cinfo.output_scanline < cinfo.output_height)
    {
        /* jpeg_read_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could ask for
         * more than one scanline at a time if that's more convenient.
         */
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);

        memcpy(dst, buffer[0], row_stride);
        dst += row_stride;
    }

    /* Step 7: 解压完成 */

    (void)jpeg_finish_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
     * with the stdio data source.
     */

    /* Step 8: 释放资源 */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_decompress(&cinfo);

    /* After finish_decompress, we can close the input file.
     * Here we postpone it until after no more JPEG errors are possible,
     * so as to simplify the setjmp error logic above.  (Actually, I don't
     * think that jpeg_destroy can do an error exit, but why assume anything...)
     */
    fclose(infile);

    /* At this point you may want to check to see whether any corrupt-data
     * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
     */

    /* And we're done! */
    return 1;
}

/**
 * 缩放显示JPEG图片
 * @param x 显示位置的x坐标
 * @param y 显示位置的y坐标
 * @param pathname 图片路径
 * @param dst_w 目标宽度
 * @param dst_h 目标高度
 * @return 成功返回0，失败返回-1
 */
#include <stdio.h>
#include <math.h>
#include "jpeglib.h"
#include "setjmp.h"
#include <math.h>

extern int *lcd_ptr;
extern int lcd_fd, ts_fd;
extern int lcd_draw_point(int i, int j, int color);

/**
 * 缩放显示JPEG图片（保持宽高比）
 * @param x 显示位置的x坐标
 * @param y 显示位置的y坐标
 * @param pathname 图片路径
 * @param dst_w 期望显示宽度
 * @param dst_h 期望显示高度
 * @return 成功返回0，失败返回-1
 */
#include <stdio.h>
#include <math.h>
#include "jpeglib.h"
#include "setjmp.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern int *lcd_ptr;
extern int lcd_fd, ts_fd;
extern int lcd_draw_point(int i, int j, int color);

/**
 * 缩放显示JPEG图片（保持宽高比）
 * @param x 显示位置的x坐标
 * @param y 显示位置的y坐标
 * @param pathname 图片路径
 * @param dst_w 期望显示宽度
 * @param dst_h 期望显示高度
 * @return 成功返回0，失败返回-1
 */
int lcd_draw_scale_jpeg(const char *pathname, int x, int y, int dst_w, int dst_h)
{
    char *jpeg_buffer = NULL;
    int src_w, src_h;

    // 1. 解压JPEG获取原始图像数据
    if (!read_JPEG_file(pathname, &src_w, &src_h, &jpeg_buffer))
    {
        fprintf(stderr, "Failed to decode JPEG file\n");
        return -1;
    }

    // 计算原始宽高比
    float src_ratio = (float)src_w / src_h;

    // 屏幕可用区域
    int max_width = 800 - x;
    int max_height = 480 - y;

    // 计算保持比例的目标尺寸
    float scale_w = (float)max_width / src_w;
    float scale_h = (float)max_height / src_h;
    float scale_ratio = MIN(scale_w, scale_h);

    // 计算实际显示尺寸（保持比例）
    dst_w = (int)(src_w * scale_ratio);
    dst_h = (int)(src_h * scale_ratio);

    printf("Original size: %dx%d (ratio: %.2f)\n", src_w, src_h, src_ratio);
    printf("Available space: %dx%d\n", max_width, max_height);
    printf("Display size: %dx%d (scale: %.2f)\n", dst_w, dst_h, scale_ratio);

    // 2. 双线性插值缩放
    for (int j = 0; j < dst_h; j++)
    {
        float v = (float)j / (dst_h - 1);
        for (int i = 0; i < dst_w; i++)
        {
            float u = (float)i / (dst_w - 1);

            // 计算源图像坐标
            float src_x = u * (src_w - 1);
            float src_y = v * (src_h - 1);

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            int x2 = (x1 + 1 < src_w) ? x1 + 1 : x1;
            int y2 = (y1 + 1 < src_h) ? y1 + 1 : y1;

            float dx = src_x - x1;
            float dy = src_y - y1;

            // 获取四个相邻像素(RGB格式)
            unsigned char *p11 = &jpeg_buffer[(y1 * src_w + x1) * 3];
            unsigned char *p12 = &jpeg_buffer[(y1 * src_w + x2) * 3];
            unsigned char *p21 = &jpeg_buffer[(y2 * src_w + x1) * 3];
            unsigned char *p22 = &jpeg_buffer[(y2 * src_w + x2) * 3];

            // 双线性插值计算新像素值
            int r = (int)(p11[0] * (1 - dx) * (1 - dy) + p12[0] * dx * (1 - dy) +
                          p21[0] * (1 - dx) * dy + p22[0] * dx * dy);
            int g = (int)(p11[1] * (1 - dx) * (1 - dy) + p12[1] * dx * (1 - dy) +
                          p21[1] * (1 - dx) * dy + p22[1] * dx * dy);
            int b = (int)(p11[2] * (1 - dx) * (1 - dy) + p12[2] * dx * (1 - dy) +
                          p21[2] * (1 - dx) * dy + p22[2] * dx * dy);

            // 限制颜色值在0-255范围内
            r = (r > 255) ? 255 : (r < 0) ? 0
                                          : r;
            g = (g > 255) ? 255 : (g < 0) ? 0
                                          : g;
            b = (b > 255) ? 255 : (b < 0) ? 0
                                          : b;

            // 绘制到LCD (BGR格式)
            int color = (r << 16) | (g << 8) | b;
            lcd_draw_point(i + x, j + y, color);
        }
    }

    // 3. 释放资源
    free(jpeg_buffer);
    return 0;
}