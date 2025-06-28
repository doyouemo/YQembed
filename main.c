#include "config.h"
#include "file_utils.h"
#include "display.h"
#include "touch.h"

// 全局变量定义
int current_image_index = 0;
int screen_width = SCREEN_WIDTH;
char (*image_paths)[1024] = NULL;
int image_paths_size = 0;

int ts_fd = -1;

/**
 * 递归读取目录中特定类型的文件
 */
int dir_read_file_recursive(const char *dirname, char (*filenames)[1024], int *count, const char *type)
{
    DIR *dirp = opendir(dirname);
    if (dirp == NULL)
    {
        return -1;
    }

    char buf[4096] = {0};
    int length = *count;

    while (1)
    {
        struct dirent *ep = readdir(dirp);
        if (ep == NULL)
        {
            *count = length;
            break;
        }

        // 跳过"."和".."
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
        {
            continue;
        }

        // 构建完整路径
        snprintf(buf, sizeof(buf), "%s/%s", dirname, ep->d_name);

        if (ep->d_type == DT_DIR)
        {
            dir_read_file_recursive(buf, filenames, &length, type);
        }
        else if (ep->d_type == DT_REG)
        {
            const char *ext = strrchr(ep->d_name, '.');
            if (ext && strcasecmp(ext, type) == 0)
            {
                if (length < 512)
                { // 防止数组越界
                    strncpy(filenames[length], buf, 1023);
                    filenames[length][1023] = '\0';
                    length++;
                }
            }
        }
    }

    closedir(dirp);
    return 0;
}

int main(void)
{
    ts_fd = open("/dev/input/event0", O_RDONLY); // 或其他正确的触摸屏设备路径
    if (ts_fd == -1)
    {
        perror("Failed to open touchscreen device");
        return -1;
    }
    // 初始化路径存储
    image_paths = calloc(512, 1024);
    if (!image_paths)
    {
        printf("Memory allocation failed\n");
        return -1;
    }

    // 搜索图片文件
    const char *search_dirs[] = {"/picture/user"};
    const char *extensions[] = {".jpg", ".jpeg", ".png", ".bmp", ".gif"};

    for (int i = 0; i < sizeof(search_dirs) / sizeof(search_dirs[0]); i++)
    {
        for (int j = 0; j < sizeof(extensions) / sizeof(extensions[0]); j++)
        {
            dir_read_file_recursive(search_dirs[i], image_paths, &image_paths_size, extensions[j]);
        }
    }

    // 初始化设备
    if (dev_init() != 0)
    {
        free(image_paths);
        return -1;
    }

    // 进入主循环
    handle_touch_events(ts_fd);

    // 清理
    dev_uninit();
    free(image_paths);
    return 0;
}