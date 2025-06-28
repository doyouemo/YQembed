#include "touch.h"
#include "display.h"
#include "media.h"
#include <linux/input.h>

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
int image_count = sizeof(image_paths) / sizeof(image_paths[0]);

int av_fd; // 视频

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

TouchPoint get_touch_data(int fd)
{
    TouchPoint point = {0};
    struct input_event ev;
    static int x = 0, y = 0;
    static bool touching = false;

    while (read(fd, &ev, sizeof(ev)) == sizeof(ev))
    {
        if (ev.type == EV_ABS)
        {
            if (ev.code == ABS_X)
                x = ev.value;
            else if (ev.code == ABS_Y)
                y = ev.value;
        }
        else if (ev.type == EV_KEY && ev.code == BTN_TOUCH)
        {
            touching = ev.value;
        }
        else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
        {
            point.x = (x * SCREEN_WIDTH) / 1024;
            point.y = (y * SCREEN_HEIGHT) / 600;
            point.pressure = touching;
            point.valid = true;
            break;
        }
    }
    return point;
}

// 停止GIF播放的辅助函数
void stop_gif_playback(void)
{
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
        // 清屏
        memset(lcd_ptr, 0x00, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(int));
    }
}

// 显示主界面
void show_main_interface()
{
    show_image("/picture/system/background_album.bmp", 0, 0, 800, 480);
    show_image("/picture/system/xc.png", 175, 190, 100, 100);      // 相册
    show_image("/picture/system/Music.png", 325, 190, 100, 100);   // 视频
    show_image("/picture/system/camera.png", 475, 190, 100, 100);  // 摄像头
    show_image("/picture/system/devices.png", 625, 190, 100, 100); // 设备
}

void handle_touch_events(int ts_fd)
{
    // 初始化显示主界面
    show_main_interface();

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);

        if (point.valid)
        {
            printf("Touch Point: X=%d, Y=%d, Pressure=%d\n",
                   point.x, point.y, point.pressure);

            if (point.pressure == 0)
            {
                printf("Touch released\n");
            }
            if ((point.x > 175 && point.x < 275) && (point.y > 190 && point.y < 290))
            {
                album();
                // 当 album 函数返回后，重新显示主界面
                show_main_interface();
            }
            else if ((point.x > 325 && point.x < 425) && (point.y > 190 && point.y < 290))
            {
                // 实现 video 函数
                music("/music");
                // 当 music 函数返回后，重新显示主界面
                show_main_interface();
            }
            else if ((point.x > 475 && point.x < 575) && (point.y > 190 && point.y < 290))
            {
                // 实现 video 函数
                video("/music");
                show_main_interface();
            }
            // else if ((point.x > 625 && point.x < 725) && (point.y > 190 && point.y < 290))
            // {
            //     // 实现 device 函数
            //     device();
            // show_main_interface();
            // }
        }
    }
}

int album(void)
{
    // 显示初始界面
    show_image("/picture/system/background_album.bmp", 0, 0, 800, 480);
    show_image("/picture/system/Next-1.png", 50, 190, 100, 100);
    show_image("/picture/system/Next-2.png", 650, 190, 100, 100);
    show_image("/picture/system/exit.png", 0, 0, 100, 100);

    // 显示当前图片
    if (image_paths_size > 0)
    {
        show_image(image_paths[current_image_index], 0, 0, 800, 480);
    }

    bool button_pressed = false;
    int button_type = 0; // 0:无 1:上一张 2:下一张 3:退出

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);

        if (!point.valid)
        {
            continue;
        }

        // 调试信息
        printf("Touch Point: X=%d, Y=%d, Pressure=%d\n", point.x, point.y, point.pressure);

        // 处理按下事件
        if (point.pressure == 1)
        {
            // 检测按下的按钮区域
            if (point.x >= 50 && point.x <= 150 && point.y >= 190 && point.y <= 290)
            {
                button_type = 1; // 上一张
                button_pressed = true;
            }
            else if (point.x >= 650 && point.x <= 750 && point.y >= 190 && point.y <= 290)
            {
                button_type = 2; // 下一张
                button_pressed = true;
            }
            else if (point.x >= 0 && point.x <= 100 && point.y >= 0 && point.y <= 100)
            {
                button_type = 3; // 退出
                button_pressed = true;
            }
        }
        // 处理释放事件
        else if (point.pressure == 0 && button_pressed)
        {
            // 停止当前GIF播放（如果有）
            stop_gif_playback();

            // 根据按钮类型执行相应操作
            switch (button_type)
            {
            case 1: // 上一张
                printf("Previous button released - switching image\n");
                current_image_index = (current_image_index - 1 + image_paths_size) % image_paths_size;
                break;

            case 2: // 下一张
                printf("Next button released - switching image\n");
                current_image_index = (current_image_index + 1) % image_paths_size;
                break;

            case 3: // 退出
                return 0;

            default:
                break;
            }

            // 显示新图片
            if (button_type != 3 && image_paths_size > 0)
            {
                show_image(image_paths[current_image_index], 0, 0, 800, 480);
            }

            // 重新显示按钮（如果未退出）
            if (button_type != 3)
            {
                show_image("/picture/system/Next-1.png", 50, 190, 100, 100);
                show_image("/picture/system/Next-2.png", 650, 190, 100, 100);
                show_image("/picture/system/exit.png", 0, 0, 100, 100);
            }

            // 重置按钮状态
            button_pressed = false;
            button_type = 0;
        }
    }
}

// 在全局变量声明后添加函数声明
extern int dir_read_file_recursive(const char *dirname, char (*filenames)[1024], int *count, const char *type);

int music(const char *dirname)
{
    // 显示初始界面
    show_image("/picture/system/background_music.png", 0, 0, 800, 480);
    show_image("/picture/system/before.png", 200, 370, 80, 80);
    show_image("/picture/system/stop.png", 360, 370, 80, 80); // 初始显示暂停按钮
    show_image("/picture/system/next.png", 540, 370, 80, 80);
    show_image("/picture/system/back.png", 0, 0, 80, 80);
    // 显示音量控制按钮
    show_image("/picture/system/jian.png", 20, 370, 80, 80);
    show_image("/picture/system/jia.png", 700, 370, 80, 80);

    bool is_paused = false;
    char (*music_names)[1024] = calloc(512, 1024);
    int music_count = 0;
    char cmd_buf[1024];

    // 递归读取音乐文件（支持MP3等格式）
    dir_read_file_recursive(dirname, music_names, &music_count, ".mp3");
    dir_read_file_recursive(dirname, music_names, &music_count, ".wav");
    dir_read_file_recursive(dirname, music_names, &music_count, ".ogg");

    if (music_count == 0)
    {
        printf("No music files found in %s\n", dirname);
        free(music_names);
        return -1;
    }

    int current_index = 0;
    int volume = 50;                 // 初始音量(0-100)
    mkfifo("/tmp/music_fifo", 0666); // 创建控制管道
    mplayer_init();                  // 初始化mplayer控制

    // 初始播放第一首
    if (music_count > 0)
    {
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "mplayer %s -slave -quiet -input file=/tmp/music_fifo "
                 "-volume %d -softvol -softvol-max 100 &",
                 music_names[current_index], volume);
        system(cmd_buf);
    }

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);
        if (!point.valid)
            continue;

        // 处理按钮按下
        if (point.pressure == 1)
        {
            // 上一首按钮 (200,370)-(280,450)
            if (point.x >= 200 && point.x <= 280 && point.y >= 370 && point.y <= 450)
            {
                printf("Previous music button pressed\n");
                system("killall -9 mplayer");

                current_index = (current_index - 1 + music_count) % music_count;
                // 切换歌曲后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);

                snprintf(cmd_buf, sizeof(cmd_buf),
                         "mplayer %s -slave -quiet -input file=/tmp/music_fifo "
                         "-volume %d -softvol -softvol-max 100 &",
                         music_names[current_index], volume);
                system(cmd_buf);
            }
            // 播放/暂停按钮 (360,370)-(440,450)
            else if (point.x >= 360 && point.x <= 440 && point.y >= 370 && point.y <= 450)
            {
                printf("Play/Pause button pressed\n");
                if (is_paused)
                {
                    // 恢复播放
                    show_image("/picture/system/stop.png", 360, 370, 80, 80);
                    res_play();
                    is_paused = false;
                }
                else
                {
                    // 暂停播放
                    show_image("/picture/system/start.png", 360, 370, 80, 80);
                    pause_playback();
                    is_paused = true;
                }
            }
            // 下一首按钮 (540,370)-(620,450)
            else if (point.x >= 540 && point.x <= 620 && point.y >= 370 && point.y <= 450)
            {
                printf("Next music button pressed\n");
                system("killall -9 mplayer");

                current_index = (current_index + 1) % music_count;
                // 切换歌曲后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);

                snprintf(cmd_buf, sizeof(cmd_buf),
                         "mplayer %s -slave -quiet -input file=/tmp/music_fifo "
                         "-volume %d -softvol -softvol-max 100 &",
                         music_names[current_index], volume);
                system(cmd_buf);
            }
            // 退出按钮 (0,0)-(80,80)
            else if (point.x >= 0 && point.x <= 80 && point.y >= 0 && point.y <= 80)
            {
                system("killall -9 mplayer");
                unlink("/tmp/music_fifo");
                free(music_names);
                return 0;
            }
            // 增加音量按钮 (700,370)-(780,450)
            else if (point.x >= 700 && point.x <= 780 && point.y >= 370 && point.y <= 450)
            {
                if (volume < 100)
                {
                    volume += 5;
                    if (volume > 100)
                        volume = 100;
                    printf("Volume increased to %d\n", volume);

                    // 通过管道发送音量命令
                    int fd = open("/tmp/music_fifo", O_WRONLY);
                    if (fd > 0)
                    {
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                        write(fd, cmd, strlen(cmd));
                        close(fd);
                    }
                }
            }
            // 减少音量按钮 (20,370)-(100,450)
            else if (point.x >= 20 && point.x <= 100 && point.y >= 370 && point.y <= 450)
            {
                if (volume > 0)
                {
                    volume -= 5;
                    if (volume < 0)
                        volume = 0;
                    printf("Volume decreased to %d\n", volume);

                    // 通过管道发送音量命令
                    int fd = open("/tmp/music_fifo", O_WRONLY);
                    if (fd > 0)
                    {
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                        write(fd, cmd, strlen(cmd));
                        close(fd);
                    }
                }
            }
        }
    }
}

int video(const char *dirname)
{
    // 显示初始界面 - 使用视频背景图
    show_image("/picture/system/background_video.jpg", 0, 0, 800, 480);
    show_image("/picture/system/before.png", 200, 400, 80, 80);
    show_image("/picture/system/stop.png", 360, 400, 80, 80); // 初始显示暂停按钮
    show_image("/picture/system/next.png", 540, 400, 80, 80);
    show_image("/picture/system/back.png", 0, 0, 80, 80);
    // 显示音量控制按钮
    show_image("/picture/system/jian.png", 20, 400, 80, 80);
    show_image("/picture/system/jia.png", 700, 400, 80, 80);

    bool is_paused = false;
    char (*video_names)[1024] = calloc(512, 1024);
    int video_count = 0;

    // 递归读取视频文件(支持常见格式)
    dir_read_file_recursive(dirname, video_names, &video_count, ".mp4");
    dir_read_file_recursive(dirname, video_names, &video_count, ".avi");
    dir_read_file_recursive(dirname, video_names, &video_count, ".mkv");

    if (video_count == 0)
    {
        printf("No video files found in %s\n", dirname);
        free(video_names);
        return -1;
    }

    int current_index = 0;
    int volume = 50; // 初始音量(0-100)
    char cmd_buf[1024];

    // 创建控制管道
    mkfifo("/tmp/video_fifo", 0666);
    mplayer_init();

    // 初始播放第一个视频
    if (video_count > 0)
    {

        snprintf(cmd_buf, sizeof(cmd_buf),
                 "mplayer %s -slave -quiet -input file=/tmp/video_fifo "
                 "-zoom -x 800 -y 400 -geometry 0:0 -vo fbdev2 "
                 "-volume %d -softvol -softvol-max 100 &",
                 video_names[current_index], volume);
        system(cmd_buf);
    }

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);
        if (!point.valid)
            continue;

        // 处理按钮按下
        if (point.pressure == 1)
        {
            // 上一个视频按钮 (200,370)-(280,450)
            if (point.x >= 200 && point.x <= 280 && point.y >= 400 && point.y <= 480)
            {
                printf("Previous video button pressed\n");
                system("killall -9 mplayer");

                current_index = (current_index - 1 + video_count) % video_count;
                // 切换视频后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 400, 80, 80);

                snprintf(cmd_buf, sizeof(cmd_buf),
                         "mplayer %s -slave -quiet -input file=/tmp/video_fifo "
                         "-zoom -x 800 -y 400 -geometry 0:0 -vo fbdev2 "
                         "-volume %d -softvol -softvol-max 100 &",
                         video_names[current_index], volume);
                system(cmd_buf);
            }
            // 播放/暂停按钮 (360,370)-(440,450)
            else if (point.x >= 360 && point.x <= 440 && point.y >= 400 && point.y <= 480)
            {
                printf("Play/Pause button pressed\n");
                if (is_paused)
                {
                    // 恢复播放
                    show_image("/picture/system/stop.png", 360, 400, 80, 80);
                    res_play();
                    is_paused = false;
                }
                else
                {
                    // 暂停播放
                    show_image("/picture/system/start.png", 360, 400, 80, 80);
                    pause_playback();
                    is_paused = true;
                }
            }
            // 下一个视频按钮 (540,370)-(620,450)
            else if (point.x >= 540 && point.x <= 620 && point.y >= 400 && point.y <= 480)
            {
                printf("Next video button pressed\n");
                system("killall -9 mplayer");

                current_index = (current_index + 1) % video_count;
                // 切换视频后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 400, 80, 80);

                snprintf(cmd_buf, sizeof(cmd_buf),
                         "mplayer %s -slave -quiet -input file=/tmp/video_fifo "
                         "-zoom -x 800 -y 400 -geometry 0:0 -vo fbdev2 "
                         "-volume %d -softvol -softvol-max 100 &",
                         video_names[current_index], volume);
                system(cmd_buf);
            }
            // 退出按钮 (0,0)-(80,80)
            else if (point.x >= 0 && point.x <= 80 && point.y >= 0 && point.y <= 80)
            {
                system("killall -9 mplayer");
                unlink("/tmp/video_fifo");
                free(video_names);
                return 0;
            }
            // 增加音量按钮 (700,370)-(780,450)
            else if (point.x >= 700 && point.x <= 780 && point.y >= 400 && point.y <= 480)
            {
                if (volume < 100)
                {
                    volume += 5;
                    if (volume > 100)
                        volume = 100;
                    printf("Volume increased to %d\n", volume);

                    // 通过管道发送音量命令
                    int fd = open("/tmp/video_fifo", O_WRONLY);
                    if (fd > 0)
                    {
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                        write(fd, cmd, strlen(cmd));
                        close(fd);
                    }
                }
            }
            // 减少音量按钮 (20,370)-(100,450)
            else if (point.x >= 20 && point.x <= 100 && point.y >= 400 && point.y <= 480)
            {
                if (volume > 0)
                {
                    volume -= 5;
                    if (volume < 0)
                        volume = 0;
                    printf("Volume decreased to %d\n", volume);

                    // 通过管道发送音量命令
                    int fd = open("/tmp/video_fifo", O_WRONLY);
                    if (fd > 0)
                    {
                        char cmd[32];
                        snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                        write(fd, cmd, strlen(cmd));
                        close(fd);
                    }
                }
            }
        }
    }
}
// 3
//   处理同步事件（表示一个事件序列结束）
void handle_sync_event(void)
{
    // printf("---- End of Event ----\n");
}