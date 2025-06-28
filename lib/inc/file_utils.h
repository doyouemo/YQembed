#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "config.h"

// 文件操作函数
int dir_read_file_recursive(const char *dirname, char (*filenames)[1024], int *count, const char *type);

#endif // FILE_UTILS_H
