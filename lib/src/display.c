#include "display.h"

// #include "bmp.c"
// #include "jpg.c"
// #include "png.c"
// #include "gif.c"

// LCD设备映射
int *lcd_ptr = NULL;
int lcd_fd = -1;

extern char (*image_paths)[1024];
extern int image_paths_size;

int lcd_draw_point(int i, int j, int color)
{
    if (i < 0 || i >= SCREEN_WIDTH || j < 0 || j >= SCREEN_HEIGHT)
        return -1;
    *(lcd_ptr + SCREEN_WIDTH * j + i) = color;
    return 0;
}

int dev_init()
{
    lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1)
        return -1;

    lcd_ptr = mmap(NULL, SCREEN_WIDTH * SCREEN_HEIGHT * 4,
                   PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    if (lcd_ptr == MAP_FAILED)
    {
        close(lcd_fd);
        return -1;
    }
    return 0;
}

int dev_uninit()
{
    if (lcd_ptr != MAP_FAILED)
        munmap(lcd_ptr, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    if (lcd_fd != -1)
        close(lcd_fd);
    return 0;
}

/**
 * 统一图像显示函数（自动识别BMP/JPEG）
 * @param pathname 图片路径
 * @param x 显示位置的x坐标
 * @param y 显示位置的y坐标
 * @param dst_w 期望显示宽度
 * @param dst_h 期望显示高度
 * @return 成功返回0，失败返回-1
 */
int show_image(const char *path, int x, int y, int w, int h)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return -1;

    if (strcasecmp(ext, ".bmp") == 0)
        return lcd_draw_scale_bmp(path, x, y, w, h);
    else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return lcd_draw_scale_jpeg(path, x, y, w, h);
    else if (strcasecmp(ext, ".png") == 0)
        return lcd_draw_scale_png(path, x, y, w, h);
    else if (strcasecmp(ext, ".gif") == 0)
        return ShowGifAnimation(path, x, y, w, h);

    return -1;
}