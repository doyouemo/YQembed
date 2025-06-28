#include "media.h"
#include <unistd.h>
#include <sys/stat.h>

int av_fd = -1;

// 添加清理函数
void cleanup_music(void)
{
    system("killall -9 madplay");
    return 0;
}

// 1. 播放器初始化（打开管道文件，用于调控播放过程当中所使用的动态参数）
void mplayer_init()
{
    // 在临时目录创建管道文件
    if (access("/tmp/fifo", F_OK))
    {
        mkfifo("/tmp/fifo", 0777);
    }

    // 打开管道文件
    av_fd = open("/tmp/fifo", O_RDWR);
    return 0;
}

// 发送指令
void send_cmd(char *cmd)
{
    write(av_fd, cmd, strlen(cmd));
    return 0;
}

// 2. 继续播放
int res_play()
{
    system("killall -18  mplayer");
    return 0;
}

// 3. 暂停播放
int pause_playback()
{
    system("killall -19  mplayer");
    return 0;
}

// 4. 停止播放
int stop_play()
{
    system("killall -9  mplayer");
    return 0;
}

// 音量控制函数
void set_volume(int volume_level)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "volume %d 1\n", volume_level);
    send_cmd(cmd);

    // 当音量为0时强制静音
    if (volume_level == 0)
    {
        send_cmd("mute 1\n");
    }
    else
    {
        send_cmd("mute 0\n"); // 确保非0时取消静音
    }
}

// 音量增加
void vol_up(int *current_volume)
{
    if (*current_volume < 100)
    {
        *current_volume += 5;
        if (*current_volume > 100)
            *current_volume = 100;
        set_volume(*current_volume);
        printf("Volume increased to %d\n", *current_volume);
    }
}

// 音量减少
void vol_down(int *current_volume)
{
    if (*current_volume > 0)
    {
        *current_volume -= 5;
        if (*current_volume < 0)
            *current_volume = 0;
        set_volume(*current_volume);
        printf("Volume decreased to %d\n", *current_volume);
    }
}
// 快进 5 秒
int fast_forward()
{
    send_cmd("seek +5\n");
    return 0;
}

// 快退 10 秒
int my_rewind()
{
    send_cmd("seek -10\n");
    return 0;
}

// 静音（开关）
int mute()
{
    send_cmd("mute\n");
    return 0;
}