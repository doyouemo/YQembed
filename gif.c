#include <gif_lib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <config.h>
#include <time.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>

#include <gif_lib.h>

extern int *lcd_ptr;
extern int ts_fd;

// 全局变量
volatile int gif_playing = 0;
pthread_t gif_thread;
GifFileType *current_gif = NULL;
int current_x, current_y, current_w, current_h;
const char *current_path = NULL;

// 双线性插值处理GIF帧
static void ProcessGifFrame(GifFileType *gif, SavedImage *frame,
                            int x, int y, int dst_w, int dst_h)
{
    ColorMapObject *cmap = frame->ImageDesc.ColorMap ? frame->ImageDesc.ColorMap : gif->SColorMap;
    if (!cmap || dst_w <= 0 || dst_h <= 0)
        return;

    GraphicsControlBlock gcb;
    if (DGifSavedExtensionToGCB(gif, frame - gif->SavedImages, &gcb) != GIF_OK)
        return;

    const int src_w = frame->ImageDesc.Width;
    const int src_h = frame->ImageDesc.Height;
    const int src_left = frame->ImageDesc.Left;
    const int src_top = frame->ImageDesc.Top;
    const int transparent_color = gcb.TransparentColor;
    const GifByteType *raster = frame->RasterBits;
    const int screen_width = gif->SWidth;
    const int screen_height = gif->SHeight;

    // 预计算倒数避免除法
    const float inv_dst_w_minus_1 = 1.0f / (dst_w - 1);
    const float inv_dst_h_minus_1 = 1.0f / (dst_h - 1);
    const float src_w_minus_1 = src_w - 1;
    const float src_h_minus_1 = src_h - 1;

    // 边界检查预计算
    const int max_x = screen_width - 1;
    const int max_y = screen_height - 1;

    // 目标缓冲区指针
    int *dest_ptr = lcd_ptr + (y * SCREEN_WIDTH) + x;
    const int dest_row_stride = SCREEN_WIDTH - dst_w;

    for (int j = 0; j < dst_h; j++)
    {
        const float v = j * inv_dst_h_minus_1;
        const float src_y = src_top + v * src_h_minus_1;

        // 边界检查
        if (src_y < 0 || src_y >= screen_height)
        {
            dest_ptr += SCREEN_WIDTH;
            continue;
        }

        const int y1 = (int)src_y;
        const int y2 = y1 < max_y ? y1 + 1 : y1;
        const float dy = src_y - y1;
        const float dy1 = 1.0f - dy;

        const int y1_offset = y1 * screen_width;
        const int y2_offset = y2 * screen_width;

        for (int i = 0; i < dst_w; i++)
        {
            const float u = i * inv_dst_w_minus_1;
            const float src_x = src_left + u * src_w_minus_1;

            // 边界检查
            if (src_x < 0 || src_x >= screen_width)
            {
                dest_ptr++;
                continue;
            }

            const int x1 = (int)src_x;
            const int x2 = x1 < max_x ? x1 + 1 : x1;
            const float dx = src_x - x1;
            const float dx1 = 1.0f - dx;

            // 获取四个相邻像素索引
            const int idx11 = y1_offset + x1;
            const int idx12 = y1_offset + x2;
            const int idx21 = y2_offset + x1;
            const int idx22 = y2_offset + x2;

            // 检查透明色
            const GifByteType p11 = raster[idx11];
            const GifByteType p12 = raster[idx12];
            const GifByteType p21 = raster[idx21];
            const GifByteType p22 = raster[idx22];

            int valid_count = 4;
            if (transparent_color != NO_TRANSPARENT_COLOR)
            {
                valid_count = (p11 != transparent_color) +
                              (p12 != transparent_color) +
                              (p21 != transparent_color) +
                              (p22 != transparent_color);
                if (valid_count == 0)
                {
                    dest_ptr++;
                    continue;
                }
            }

            // 双线性插值
            float r = 0, g = 0, b = 0;
            float weight_sum = 0;

            if (valid_count == 4 || p11 != transparent_color)
            {
                const float w = dx1 * dy1;
                const GifColorType *c = &cmap->Colors[p11];
                r += c->Red * w;
                g += c->Green * w;
                b += c->Blue * w;
                weight_sum += w;
            }

            if (valid_count == 4 || p12 != transparent_color)
            {
                const float w = dx * dy1;
                const GifColorType *c = &cmap->Colors[p12];
                r += c->Red * w;
                g += c->Green * w;
                b += c->Blue * w;
                weight_sum += w;
            }

            if (valid_count == 4 || p21 != transparent_color)
            {
                const float w = dx1 * dy;
                const GifColorType *c = &cmap->Colors[p21];
                r += c->Red * w;
                g += c->Green * w;
                b += c->Blue * w;
                weight_sum += w;
            }

            if (valid_count == 4 || p22 != transparent_color)
            {
                const float w = dx * dy;
                const GifColorType *c = &cmap->Colors[p22];
                r += c->Red * w;
                g += c->Green * w;
                b += c->Blue * w;
                weight_sum += w;
            }

            // 归一化并写入结果
            if (weight_sum > 0)
            {
                r /= weight_sum;
                g /= weight_sum;
                b /= weight_sum;
            }

            *dest_ptr++ = ((int)r << 16) | ((int)g << 8) | (int)b;
        }

        dest_ptr += dest_row_stride;
    }
}

// 线程函数
void *gif_thread_func(void *arg)
{
    while (gif_playing)
    {
        if (current_gif)
        {
            for (int i = 0; i < current_gif->ImageCount && gif_playing; i++)
            {
                // 更频繁地检查停止标志
                if (!gif_playing)
                    break;

                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);

                ProcessGifFrame(current_gif, &current_gif->SavedImages[i],
                                current_x, current_y, current_w, current_h);

                GraphicsControlBlock gcb;
                int delay_ms = 100; // 默认延迟

                if (DGifSavedExtensionToGCB(current_gif, i, &gcb) == GIF_OK)
                {
                    delay_ms = gcb.DelayTime * 10;
                    if (delay_ms < 20)
                        delay_ms = 100;
                }

                clock_gettime(CLOCK_MONOTONIC, &end);
                long elapsed = (end.tv_sec - start.tv_sec) * 1000 +
                               (end.tv_nsec - start.tv_nsec) / 1000000;

                // 将延迟分成小块以便更频繁检查停止标志
                long remaining = delay_ms - elapsed;
                while (remaining > 0 && gif_playing)
                {
                    usleep(10000); // 每次睡眠10ms
                    remaining -= 10;
                }
            }
        }
    }
    return NULL;
}

int ShowGifAnimation(const char *path, int x, int y, int dst_w, int dst_h)
{
    int error = 0;

    // 检查文件是否存在
    if (access(path, F_OK) == -1)
    {
        fprintf(stderr, "GIF Error: File '%s' does not exist\n", path);
        return -1;
    }

    // 检查文件可读性
    if (access(path, R_OK) == -1)
    {
        fprintf(stderr, "GIF Error: No permission to read file '%s'\n", path);
        return -1;
    }

    // 停止当前播放
    if (gif_playing)
    {
        gif_playing = 0;
        pthread_join(gif_thread, NULL);
        if (current_gif)
            DGifCloseFile(current_gif, &error);
    }

    // 打开新GIF文件
    current_gif = DGifOpenFileName(path, &error);
    if (!current_gif)
    {
        fprintf(stderr, "GIF Error: %s\n", GifErrorString(error));
        return -1;
    }

    if (DGifSlurp(current_gif) != GIF_OK)
    {
        fprintf(stderr, "Failed to read GIF data\n");
        DGifCloseFile(current_gif, &error);
        return -1;
    }

    // 计算显示尺寸
    float src_ratio = (float)current_gif->SWidth / current_gif->SHeight;
    float dst_ratio = (float)dst_w / dst_h;

    if (src_ratio > dst_ratio)
    {
        current_w = dst_w;
        current_h = (int)(dst_w / src_ratio);
        current_y = y + (dst_h - current_h) / 2;
        current_x = x;
    }
    else
    {
        current_h = dst_h;
        current_w = (int)(dst_h * src_ratio);
        current_x = x + (dst_w - current_w) / 2;
        current_y = y;
    }

    current_path = path;

    // 清屏
    memset(lcd_ptr, 0x00, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(int));

    // 启动播放线程
    gif_playing = 1;
    if (pthread_create(&gif_thread, NULL, gif_thread_func, NULL) != 0)
    {
        fprintf(stderr, "Failed to create GIF thread\n");
        gif_playing = 0;
        DGifCloseFile(current_gif, &error);
        return -1;
    }

    return 0;
}