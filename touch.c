#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/input.h>
#include <dirent.h> // 添加目录操作支持
#include <signal.h> // 添加信号处理支持
#include "show_image.c"
#include "config.h"
#include <sys/wait.h> // 添加在文件顶部
#include <stdbool.h>

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

int album(void);
int music(const char *path);
int video(const char *path);

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

TouchPoint get_touch_data(int ts_fd)
{
    TouchPoint point = {0};
    struct input_event event;
    static int x = 0, y = 0;
    static bool touching = false;

    while (read(ts_fd, &event, sizeof(event)) == sizeof(event))
    {
        if (event.type == EV_ABS)
        {
            if (event.code == ABS_X)
            {
                x = event.value;
            }
            else if (event.code == ABS_Y)
            {
                y = event.value;
            }
        }
        else if (event.type == EV_KEY && event.code == BTN_TOUCH)
        {
            touching = event.value;              // 1=按下, 0=释放
            printf("BTN_TOUCH: %d\n", touching); // 调试输出
        }

        if (event.type == EV_SYN && event.code == SYN_REPORT)
        {
            point.x = (x * 800) / 1024; // 映射到屏幕坐标
            point.y = (y * 400) / 600;
            point.pressure = touching ? 1 : 0;
            point.valid = true; // 始终返回有效数据
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
            // else if ((point.x > 475 && point.x < 575) && (point.y > 190 && point.y < 290))
            // {
            //     // 实现 video 函数
            //     video();
            // }
            // else if ((point.x > 625 && point.x < 725) && (point.y > 190 && point.y < 290))
            // {
            //     // 实现 device 函数
            //     device();
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

// 在music函数开头添加变量声明
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
    char (*mp3names)[1024] = calloc(512, 1024);
    int mp3count = 0;

    // 递归读取MP3文件
    dir_read_file_recursive(dirname, mp3names, &mp3count, ".mp3");

    if (mp3count == 0)
    {
        printf("No MP3 files found in %s\n", dirname);
        free(mp3names);
        return -1;
    }

    int current_index = 0;
    pid_t playing_pid = -1;
    int volume = 0; // 初始音量

    // 初始播放第一首
    if (mp3count > 0)
    {
        playing_pid = fork();
        if (playing_pid == 0)
        {
            execlp("madplay", "madplay", "-Q", "--attenuate", "0", mp3names[current_index], NULL);
            perror("execlp failed");
            _exit(1);
        }
    }

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);
        if (!point.valid)
            continue;

        // 处理按钮按下
        if (point.pressure == 1)
        {
            // 上一首按钮
            if (point.x >= 200 && point.x <= 280 && point.y >= 300 && point.y <= 400)
            {
                printf("Previous button pressed\n");
                if (playing_pid > 0)
                {
                    kill(playing_pid, SIGKILL);
                    waitpid(playing_pid, NULL, 0);
                }

                current_index = (current_index - 1 + mp3count) % mp3count;
                playing_pid = fork();
                // 切换歌曲后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);
                if (playing_pid == 0)
                {
                    char volume_arg[32];
                    snprintf(volume_arg, sizeof(volume_arg), "--attenuate=%d", volume);
                    execlp("madplay", "madplay", "-Q", volume_arg, mp3names[current_index], NULL);
                    _exit(1);
                }
            }
            // 播放/暂停按钮
            else if (point.x >= 360 && point.x <= 420 && point.y >= 300 && point.y <= 400)
            {
                printf("Play/Pause button pressed\n");
                if (playing_pid > 0)
                {
                    if (is_paused)
                    {
                        // 恢复播放
                        show_image("/picture/system/stop.png", 360, 370, 80, 80);
                        kill(playing_pid, SIGCONT);
                        is_paused = false;
                    }
                    else
                    {
                        // 暂停播放
                        show_image("/picture/system/start.png", 360, 370, 80, 80);
                        kill(playing_pid, SIGSTOP);
                        is_paused = true;
                    }
                }
            }
            // 下一首按钮
            else if (point.x >= 540 && point.x <= 620 && point.y >= 300 && point.y <= 400)
            {
                printf("Next button pressed\n");
                if (playing_pid > 0)
                {
                    kill(playing_pid, SIGKILL);
                    waitpid(playing_pid, NULL, 0);
                }

                current_index = (current_index + 1) % mp3count;
                playing_pid = fork();
                // 切换歌曲后重置暂停状态和UI
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);
                if (playing_pid == 0)
                {
                    char volume_arg[32];
                    snprintf(volume_arg, sizeof(volume_arg), "--attenuate=%d", volume);
                    execlp("madplay", "madplay", "-Q", volume_arg, mp3names[current_index], NULL);
                    _exit(1);
                }
            }
            // 退出按钮
            else if (point.x >= 0 && point.x <= 100 && point.y >= 0 && point.y <= 100)
            {
                if (playing_pid > 0)
                {
                    kill(playing_pid, SIGKILL);
                    waitpid(playing_pid, NULL, 0);
                }
                free(mp3names);
                return 0;
            }
            // 增加音量按钮
            else if (point.x >= 700 && point.x <= 780 && point.y >= 300 && point.y <= 400)
            {
                if (volume < 18)
                {                // 音量范围-175~18，减少时检查上限
                    volume += 5; // 注意：音量值越大表示音量越小
                    if (volume > 18)
                        volume = 18;

                    printf("Volume decreased to %d\n", volume);

                    // 如果正在播放，需要重新启动madplay应用新音量
                    if (playing_pid > 0)
                    {
                        kill(playing_pid, SIGKILL);
                        waitpid(playing_pid, NULL, 0);

                        playing_pid = fork();
                        if (playing_pid == 0)
                        {
                            char volume_arg[32];
                            snprintf(volume_arg, sizeof(volume_arg), "--attenuate=%d", volume);
                            execlp("madplay", "madplay", "-Q", volume_arg, mp3names[current_index], NULL);
                            _exit(1);
                        }
                    }
                }
            }
            // 减少音量按钮
            else if (point.x >= 20 && point.x <= 100 && point.y >= 300 && point.y <= 400)
            {
                if (volume > -175)
                {                // 音量范围-175~18，增加时检查下限
                    volume -= 5; // 注意：音量值越小表示音量越大
                    if (volume < -175)
                        volume = -175;

                    printf("Volume increased to %d\n", volume);

                    // 如果正在播放，需要重新启动madplay应用新音量
                    if (playing_pid > 0)
                    {
                        kill(playing_pid, SIGKILL);
                        waitpid(playing_pid, NULL, 0);

                        playing_pid = fork();
                        if (playing_pid == 0)
                        {
                            char volume_arg[32];
                            snprintf(volume_arg, sizeof(volume_arg), "--attenuate=%d", volume);
                            execlp("madplay", "madplay", "-Q", volume_arg, mp3names[current_index], NULL);
                            _exit(1);
                        }
                    }
                }
            }
        }
    }
}
// 1
// 2
int video(const char *path)
{
    // 显示初始界面 - 使用视频背景图
    show_image("/picture/system/background_video.bmp", 0, 0, 800, 480);

    // 显示控制按钮(位置与音乐播放器一致)
    show_image("/picture/system/before.png", 200, 370, 80, 80); // 上一个
    show_image("/picture/system/stop.png", 360, 370, 80, 80);   // 暂停(初始状态)
    show_image("/picture/system/next.png", 540, 370, 80, 80);   // 下一个
    show_image("/picture/system/back.png", 0, 0, 80, 80);       // 返回

    // 音量控制按钮
    show_image("/picture/system/jian.png", 20, 370, 80, 80); // 音量减
    show_image("/picture/system/jia.png", 700, 370, 80, 80); // 音量加

    bool is_paused = false;
    char (*videofiles)[1024] = calloc(512, 1024); // 存储视频文件路径
    int videocount = 0;

    // 递归搜索视频文件(支持多种格式)
    dir_read_file_recursive(path, videofiles, &videocount, ".mp4");
    dir_read_file_recursive(path, videofiles, &videocount, ".avi");
    dir_read_file_recursive(path, videofiles, &videocount, ".mkv");
    dir_read_file_recursive(path, videofiles, &videocount, ".mov");

    if (videocount == 0)
    {
        printf("未找到视频文件: %s\n", path);
        show_image("/picture/system/no_video.bmp", 200, 200, 400, 100); // 显示无视频提示
        sleep(2);
        free(videofiles);
        return -1;
    }

    int current_index = 0;
    pid_t playing_pid = -1;
    int volume = 50; // 默认音量50%

    // 管道用于控制mplayer
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("创建管道失败");
        free(videofiles);
        return -1;
    }

    // 播放第一个视频
    if (videocount > 0)
    {
        playing_pid = fork();
        if (playing_pid == 0)
        {
            close(pipefd[0]); // 子进程关闭读端

            // 重定向标准输入到管道写端
            dup2(pipefd[1], STDIN_FILENO);
            close(pipefd[1]);

            // mplayer参数说明:
            // -quiet: 减少输出
            // -fs: 全屏模式
            // -slave: 启用从模式(可通过管道控制)
            // -volume: 初始音量
            execlp("mplayer", "mplayer", "-quiet", "-fs", "-slave",
                   "-volume", "50", videofiles[current_index], NULL);
            perror("启动mplayer失败");
            _exit(1);
        }
        close(pipefd[1]); // 父进程关闭写端
    }

    while (1)
    {
        TouchPoint point = get_touch_data(ts_fd);
        if (!point.valid)
            continue;

        // 只处理按下事件(避免重复触发)
        if (point.pressure == 1)
        {
            // 上一个视频按钮
            if (point.x >= 200 && point.x <= 280 && point.y >= 370 && point.y <= 450)
            {
                printf("切换到上一个视频\n");

                // 发送停止命令给mplayer
                write(pipefd[0], "stop\n", 5);

                // 更新索引
                current_index = (current_index - 1 + videocount) % videocount;

                // 加载新视频
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "loadfile \"%s\" 0\n", videofiles[current_index]);
                write(pipefd[0], cmd, strlen(cmd));

                // 更新UI状态
                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);
            }

            // 播放/暂停按钮
            else if (point.x >= 360 && point.x <= 440 && point.y >= 370 && point.y <= 450)
            {
                if (is_paused)
                {
                    printf("恢复播放\n");
                    write(pipefd[0], "pause\n", 6);
                    show_image("/picture/system/stop.png", 360, 370, 80, 80);
                }
                else
                {
                    printf("暂停播放\n");
                    write(pipefd[0], "pause\n", 6);
                    show_image("/picture/system/start.png", 360, 370, 80, 80);
                }
                is_paused = !is_paused;
            }

            // 下一个视频按钮
            else if (point.x >= 540 && point.x <= 620 && point.y >= 370 && point.y <= 450)
            {
                printf("切换到下一个视频\n");

                write(pipefd[0], "stop\n", 5);

                current_index = (current_index + 1) % videocount;

                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "loadfile \"%s\" 0\n", videofiles[current_index]);
                write(pipefd[0], cmd, strlen(cmd));

                is_paused = false;
                show_image("/picture/system/stop.png", 360, 370, 80, 80);
            }

            // 退出按钮
            else if (point.x >= 0 && point.x <= 80 && point.y >= 0 && point.y <= 80)
            {
                printf("退出视频播放\n");

                // 发送退出命令
                write(pipefd[0], "quit\n", 5);

                // 等待子进程结束
                waitpid(playing_pid, NULL, 0);

                close(pipefd[0]);
                free(videofiles);
                return 0;
            }

            // 音量增加按钮
            else if (point.x >= 700 && point.x <= 780 && point.y >= 370 && point.y <= 450)
            {
                if (volume < 100)
                {
                    volume += 5;
                    if (volume > 100)
                        volume = 100;

                    printf("音量增加到: %d\n", volume);

                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                    write(pipefd[0], cmd, strlen(cmd));
                }
            }

            // 音量减少按钮
            else if (point.x >= 20 && point.x <= 100 && point.y >= 370 && point.y <= 450)
            {
                if (volume > 0)
                {
                    volume -= 5;
                    if (volume < 0)
                        volume = 0;

                    printf("音量减少到: %d\n", volume);

                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume);
                    write(pipefd[0], cmd, strlen(cmd));
                }
            }
        }
    }
}

//  处理同步事件（表示一个事件序列结束）
void handle_sync_event(void)
{
    // printf("---- End of Event ----\n");
}