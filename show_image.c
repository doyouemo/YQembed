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

#include "jpeglib.h"
#include "setjmp.h"

#include <string.h> // 需要包含这个头文件以使用 strrchr 和 strdup
#include <libgen.h> // 需要包含这个头文件以使用 basename 函数

#include "bmp.c"
#include "jpg.c"
#include "png.c"
#include "gif.c"

int *lcd_ptr;
int lcd_fd, ts_fd;

extern char (*image_paths)[1024];
extern int image_paths_size;

int lcd_draw_point(int i, int j, int color)
{
    if (i < 0 || i >= 800 || j < 0 || j >= 480)
    {
        return -1; // 超出屏幕范围
    }
    *(lcd_ptr + 800 * j + i) = color;
    return 0;
}

int dev_init()
{
    // 1,打开设备文件
    lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1)
    {
        printf("open lcd device failed: %s\n", strerror(errno));
        return -1;
    }

    // 2,为lcd设备建立内存映射关系
    lcd_ptr = mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    if (lcd_ptr == MAP_FAILED)
    {
        printf("mmap failed: %s\n", strerror(errno));
        close(lcd_fd);
        return -1;
    }

    ts_fd = open("/dev/input/event0", O_RDONLY);
    if (ts_fd == -1)
    {
        printf("open ts device failed: %s\n", strerror(errno));
        munmap(lcd_ptr, 800 * 480 * 4);
        close(lcd_fd);
        return -1;
    }

    return 0;
}

int dev_uninit()
{
    if (lcd_ptr != MAP_FAILED)
    {
        munmap(lcd_ptr, 800 * 480 * 4);
    }
    if (lcd_fd != -1)
    {
        close(lcd_fd);
    }
    if (ts_fd != -1)
    {
        close(ts_fd);
    }
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
int show_image(const char *pathname, int x, int y, int dst_w, int dst_h)
{
    // 获取文件名（不包括路径）
    char *filename = strdup(pathname); // 复制字符串，因为 basename 可能会修改它
    char *name = basename(filename);   // 获取纯文件名

    // 检查文件扩展名
    const char *ext = strrchr(pathname, '.');
    if (!ext)
    {
        printf("Error: File '%s' has no extension\n", name);
        free(filename);
        return -1;
    }

    printf("Displaying image file: %s\n", name);

    if (strcasecmp(ext, ".bmp") == 0)
    {
        int ret = lcd_draw_scale_bmp(pathname, x, y, dst_w, dst_h);
        free(filename);
        return ret;
    }
    else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
    {
        int ret = lcd_draw_scale_jpeg(pathname, x, y, dst_w, dst_h);
        free(filename);
        return ret;
    }
    else if (strcasecmp(ext, ".png") == 0)
    {
        int ret = lcd_draw_scale_png(pathname, x, y, dst_w, dst_h);
        free(filename);
        return ret;
    }
    else if (strcasecmp(ext, ".gif") == 0)
    {
        return ShowGifAnimation(pathname, x, y, dst_w, dst_h);
    }
    else
    {
        printf("Error: Unsupported file type '%s' for file '%s'\n", ext, name);
        printf("Supported formats: .bmp, .jpg/.jpeg, .png, .gif\n");
        free(filename);
        return -1;
    }
}