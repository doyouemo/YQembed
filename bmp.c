#include "config.h"

extern int *lcd_ptr;
extern int lcd_fd, ts_fd;
extern int lcd_draw_point(int i, int j, int color);

int lcd_draw_scale_bmp(const char *pathname, int x, int y, int dst_w, int dst_h)
{
    // 打开图片文件
    int bmp_fd = open(pathname, O_RDONLY);
    if (bmp_fd == -1)
    {
        printf("open bmp file failed: %s\n", strerror(errno));
        return -1;
    }

    BITMAP_HEAD_INFO head_info;
    if (read(bmp_fd, &head_info, sizeof(head_info)) != sizeof(head_info))
    {
        printf("read bmp header failed\n");
        close(bmp_fd);
        return -1;
    }

    // 检查是否是BMP文件
    if (head_info.bfType != 0x4D42)
    {
        printf("not a bmp file\n");
        close(bmp_fd);
        return -1;
    }

    // 只支持24位和32位BMP
    if (head_info.biBitCount != 24 && head_info.biBitCount != 32)
    {
        printf("only support 24/32 bit bmp\n");
        close(bmp_fd);
        return -1;
    }

    int src_w = head_info.biWidth;
    int src_h = abs(head_info.biHeight); // 处理负高度
    int is_top_down = head_info.biHeight < 0;

    // 检查目标尺寸是否超出屏幕范围(800x480)，如果超出则按比例缩放
    int max_width = 800 - x;  // 最大可用宽度
    int max_height = 480 - y; // 最大可用高度

    if (dst_w > max_width || dst_h > max_height)
    {
        float width_ratio = (float)max_width / dst_w;
        float height_ratio = (float)max_height / dst_h;
        float scale_ratio = (width_ratio < height_ratio) ? width_ratio : height_ratio;

        dst_w = (int)(dst_w * scale_ratio);
        dst_h = (int)(dst_h * scale_ratio);

        printf("Adjusting size to fit screen: %dx%d -> %dx%d\n",
               dst_w, dst_h, dst_w, dst_h);
    }

    // 分配原始图像内存
    IMAGE src_img;
    src_img.w = src_w;
    src_img.h = src_h;
    src_img.color = (BGRA *)malloc(src_w * src_h * sizeof(BGRA));
    if (!src_img.color)
    {
        printf("malloc failed\n");
        close(bmp_fd);
        return -1;
    }

    // 移动到像素数据开始处
    lseek(bmp_fd, head_info.bfOffBits, SEEK_SET);

    // 读取像素数据
    int bytes_per_pixel = head_info.biBitCount / 8;
    int row_padded = (src_w * bytes_per_pixel + 3) & (~3);
    unsigned char *row_data = malloc(row_padded);
    if (!row_data)
    {
        printf("malloc row data failed\n");
        free(src_img.color);
        close(bmp_fd);
        return -1;
    }

    for (int i = 0; i < src_h; i++)
    {
        if (read(bmp_fd, row_data, row_padded) != row_padded)
        {
            printf("read pixel data failed\n");
            free(row_data);
            free(src_img.color);
            close(bmp_fd);
            return -1;
        }

        int dst_row = is_top_down ? i : (src_h - 1 - i);
        for (int j = 0; j < src_w; j++)
        {
            unsigned char *pixel = row_data + j * bytes_per_pixel;
            src_img.color[dst_row * src_w + j].blue = pixel[0];
            src_img.color[dst_row * src_w + j].green = pixel[1];
            src_img.color[dst_row * src_w + j].red = pixel[2];
            src_img.color[dst_row * src_w + j].transparency = (bytes_per_pixel == 4) ? pixel[3] : 0;
        }
    }

    free(row_data);
    close(bmp_fd);

    // 分配缩放后图像内存
    IMAGE dst_img;
    dst_img.w = dst_w;
    dst_img.h = dst_h;
    dst_img.color = (BGRA *)malloc(dst_w * dst_h * sizeof(BGRA));
    if (!dst_img.color)
    {
        printf("malloc failed\n");
        free(src_img.color);
        return -1;
    }

    // 双线性插值缩放
    for (int j = 0; j < dst_h; j++)
    {
        float v = (float)j / (dst_h - 1);
        for (int i = 0; i < dst_w; i++)
        {
            float u = (float)i / (dst_w - 1);

            float src_x = u * (src_w - 1);
            float src_y = v * (src_h - 1);

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            int x2 = x1 + 1 < src_w ? x1 + 1 : x1;
            int y2 = y1 + 1 < src_h ? y1 + 1 : y1;

            float dx = src_x - x1;
            float dy = src_y - y1;

            BGRA p11 = src_img.color[y1 * src_w + x1];
            BGRA p12 = src_img.color[y1 * src_w + x2];
            BGRA p21 = src_img.color[y2 * src_w + x1];
            BGRA p22 = src_img.color[y2 * src_w + x2];

            // 插值计算
            dst_img.color[j * dst_w + i].blue =
                p11.blue * (1 - dx) * (1 - dy) +
                p12.blue * dx * (1 - dy) +
                p21.blue * (1 - dx) * dy +
                p22.blue * dx * dy;

            dst_img.color[j * dst_w + i].green =
                p11.green * (1 - dx) * (1 - dy) +
                p12.green * dx * (1 - dy) +
                p21.green * (1 - dx) * dy +
                p22.green * dx * dy;

            dst_img.color[j * dst_w + i].red =
                p11.red * (1 - dx) * (1 - dy) +
                p12.red * dx * (1 - dy) +
                p21.red * (1 - dx) * dy +
                p22.red * dx * dy;

            dst_img.color[j * dst_w + i].transparency = 0;
        }
    }

    // 绘制到LCD
    for (int j = 0; j < dst_h && (j + y) < 480; j++)
    {
        for (int i = 0; i < dst_w && (i + x) < 800; i++)
        {
            BGRA pixel = dst_img.color[j * dst_w + i];
            int color = pixel.blue | (pixel.green << 8) | (pixel.red << 16);
            lcd_draw_point(i + x, j + y, color);
        }
    }

    // 释放资源
    free(src_img.color);
    free(dst_img.color);
    return 0;
}