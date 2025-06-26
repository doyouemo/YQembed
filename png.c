#include "config.h"
#include <setjmp.h>
#define PNG_SKIP_SETJMP_CHECK
#include <png.h>

extern int *lcd_ptr;
extern int lcd_fd, ts_fd;
extern int lcd_draw_point(int i, int j, int color);

static int get_background_color(int x, int y)
{
    if (x >= 0 && x < 800 && y >= 0 && y < 480)
    {
        return *(lcd_ptr + y * 800 + x);
    }
    return 0x000000;
}

int lcd_draw_scale_png(const char *pathname, int x, int y, int dst_w, int dst_h)
{
    // 参数检查
    if (!pathname || x < 0 || y < 0 || dst_w <= 0 || dst_h <= 0)
    {
        printf("Invalid parameters\n");
        return -1;
    }

    // 打开PNG文件
    FILE *fp = fopen(pathname, "rb");
    if (!fp)
    {
        printf("open png file failed: %s\n", strerror(errno));
        return -1;
    }

    // 检查PNG签名
    png_byte sig[8];
    if (fread(sig, 1, 8, fp) != 8)
    {
        printf("read png signature failed\n");
        fclose(fp);
        return -1;
    }

    if (png_sig_cmp(sig, 0, 8) != 0)
    {
        printf("not a png file (invalid signature)\n");
        fclose(fp);
        return -1;
    }

    // 初始化PNG结构
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return -1;
    }

    // 错误处理设置
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return -1;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    // 获取图像信息
    int src_w = png_get_image_width(png_ptr, info_ptr);
    int src_h = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    // 转换为32位RGBA
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    // 分配行指针内存
    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * src_h);
    if (!row_pointers)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return -1;
    }

    // 分配像素数据内存
    for (int row = 0; row < src_h; row++)
    {
        row_pointers[row] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!row_pointers[row])
        {
            for (int j = 0; j < row; j++)
                free(row_pointers[j]);
            free(row_pointers);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return -1;
        }
    }

    // 读取图像数据
    png_read_image(png_ptr, row_pointers);
    fclose(fp);

    // 计算保持宽高比的缩放尺寸
    float src_ratio = (float)src_w / src_h;
    float dst_ratio = (float)dst_w / dst_h;

    int scaled_w, scaled_h;
    int offset_x = 0, offset_y = 0;

    if (src_ratio > dst_ratio)
    {
        // 以宽度为准，高度填充
        scaled_w = dst_w;
        scaled_h = (int)(dst_w / src_ratio);
        offset_y = (dst_h - scaled_h) / 2;
    }
    else
    {
        // 以高度为准，宽度填充
        scaled_h = dst_h;
        scaled_w = (int)(dst_h * src_ratio);
        offset_x = (dst_w - scaled_w) / 2;
    }

    // 清空目标区域（黑色背景）
    for (int j = 0; j < dst_h; j++)
    {
        for (int i = 0; i < dst_w; i++)
        {
            if (x + i < 800 && y + j < 480)
            {
                lcd_draw_point(x + i, y + j, 0x000000);
            }
        }
    }

    // 双线性插值缩放
    for (int j = 0; j < scaled_h; j++)
    {
        float v = (float)j / (scaled_h - 1);
        for (int i = 0; i < scaled_w; i++)
        {
            float u = (float)i / (scaled_w - 1);

            // 计算源图像坐标
            float src_x = u * (src_w - 1);
            float src_y = v * (src_h - 1);

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            int x2 = (x1 < src_w - 1) ? x1 + 1 : x1;
            int y2 = (y1 < src_h - 1) ? y1 + 1 : y1;

            float dx = src_x - x1;
            float dy = src_y - y1;

            // 获取四个点的RGBA值
            png_byte *p11 = &(row_pointers[y1][x1 * 4]);
            png_byte *p12 = &(row_pointers[y1][x2 * 4]);
            png_byte *p21 = &(row_pointers[y2][x1 * 4]);
            png_byte *p22 = &(row_pointers[y2][x2 * 4]);

            // 插值计算
            int r = (1 - dx) * (1 - dy) * p11[0] + dx * (1 - dy) * p12[0] +
                    (1 - dx) * dy * p21[0] + dx * dy * p22[0];
            int g = (1 - dx) * (1 - dy) * p11[1] + dx * (1 - dy) * p12[1] +
                    (1 - dx) * dy * p21[1] + dx * dy * p22[1];
            int b = (1 - dx) * (1 - dy) * p11[2] + dx * (1 - dy) * p12[2] +
                    (1 - dx) * dy * p21[2] + dx * dy * p22[2];
            int a = (1 - dx) * (1 - dy) * p11[3] + dx * (1 - dy) * p12[3] +
                    (1 - dx) * dy * p21[3] + dx * dy * p22[3];

            // 混合颜色（简单Alpha混合）
            if (a == 0)
                continue;

            if (a < 255)
            {
                // 部分透明：混合背景
                int bg_color = get_background_color(x + offset_x + i, y + offset_y + j);
                int bg_r = (bg_color >> 16) & 0xFF;
                int bg_g = (bg_color >> 8) & 0xFF;
                int bg_b = bg_color & 0xFF;

                float alpha = a / 255.0f;
                r = r * alpha + bg_r * (1 - alpha);
                g = g * alpha + bg_g * (1 - alpha);
                b = b * alpha + bg_b * (1 - alpha);
            }

            // 绘制到目标位置（居中显示）
            int draw_x = x + offset_x + i;
            int draw_y = y + offset_y + j;
            if (draw_x < 800 && draw_y < 480)
            {
                int color = (r << 16) | (g << 8) | b;
                lcd_draw_point(draw_x, draw_y, color);
            }
        }
    }

    // 释放资源
    for (int row = 0; row < src_h; row++)
    {
        free(row_pointers[row]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return 0;
}