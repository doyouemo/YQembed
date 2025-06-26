#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/input.h>

#include "show_image.c"

#include "config.h"

/* 事件类型定义 */
#define EV_SYN 0x00 // 同步事件，表示一组输入事件结束
#define EV_KEY 0x01 // 按键/触摸事件（如 BTN_TOUCH）
#define EV_ABS 0x03 // 绝对坐标事件（X、Y、压力）

/* 坐标轴相关代码 */
#define ABS_X 0x00        // X 坐标
#define ABS_Y 0x01        // Y 坐标
#define ABS_PRESSURE 0x18 // 触摸压力值

/* 触摸按键状态 */
#define BTN_TOUCH 0x14a // 表示触摸是否按下（1）或抬起（0）

// 外部声明全局变量
extern int screen_width;
extern int current_image_index;
extern char (*image_paths)[1024]; // 与 main.c 中的定义一致
extern int image_paths_size;

// 处理坐标事件
void handle_abs_event(const struct input_event *ev, int *x)
{
    switch (ev->code)
    {
    case ABS_X:
        *x = ev->value * 800 / 1024;
        printf("X: %d\n", *x);
        break;
    case ABS_Y:
        printf("Y: %d\n", ev->value * 480 / 600);
        break;
    }
}

// 处理触摸状态事件（按下/抬起）
void handle_key_event(const struct input_event *ev, int x)
{
    if (ev->code == BTN_TOUCH && ev->value == 1) // 触摸按下
    {
        // 停止当前GIF播放
        if (gif_playing)
        {
            gif_playing = 0;
            pthread_join(gif_thread, NULL);
            if (current_gif)
            {
                int error = 0;
                DGifCloseFile(current_gif, &error);
                current_gif = NULL;
            }
        }
        if (image_paths_size == 0)
            return;
        if (x < screen_width / 2) // 点击左边
        {
            current_image_index = (current_image_index - 1 + image_paths_size) % image_paths_size;
        }
        else // 点击右边
        {
            current_image_index = (current_image_index + 1) % image_paths_size;
        }
        if (current_image_index >= 0 && current_image_index < image_paths_size)
        {
            show_image(image_paths[current_image_index], 0, 0, screen_width, 480);
        }
    }
}

// 处理同步事件（表示一个事件序列结束）
void handle_sync_event(void)
{
    // printf("---- End of Event ----\n");
}