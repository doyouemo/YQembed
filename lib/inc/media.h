#ifndef MEDIA_H
#define MEDIA_H

#include "config.h"

extern int av_fd;

// 媒体控制函数
void cleanup_music(void);
void mplayer_init();
void send_cmd(char *cmd);
int res_play();
int pause_playback();
int stop_play();
void set_volume(int volume_level);
void vol_up(int *current_volume);
void vol_down(int *current_volume);
int fast_forward();
int my_rewind();
int mute();



#endif // MEDIA_H