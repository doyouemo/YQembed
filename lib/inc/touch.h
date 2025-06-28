#ifndef TOUCH_H
#define TOUCH_H

#include "config.h"

extern volatile int gif_playing;
extern pthread_t gif_thread;
extern GifFileType *current_gif;

// 触摸点获取
TouchPoint get_touch_data(int ts_fd);

// 触摸事件处理
void handle_touch_events(int ts_fd);

// 相册功能
int album(void);

// GIF控制函数
void stop_gif_playback(void);

// 界面显示函数
void show_main_interface();

#endif // TOUCH_H