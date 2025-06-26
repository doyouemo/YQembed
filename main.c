#include <stdio.h>
#include "touch.c"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

// 全局变量定义
int current_image_index = 0;
int screen_width = 800;
char (*image_paths)[1024] = NULL;
int image_paths_size = 0;

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
    // 分配存储空间 - 使用全局变量
    image_paths = calloc(512, 1024);
    if (!image_paths)
    {
        printf("Memory allocation failed\n");
        return -1;
    }

    int image_count = 0;

    // 要搜索的目录列表
    const char *search_dirs[] = {"/picture/user"};
    int dir_count = sizeof(search_dirs) / sizeof(search_dirs[0]);

    // 支持的图片格式
    const char *image_types[] = {".jpg", ".jpeg", ".png", ".bmp", ".gif"};
    int type_count = sizeof(image_types) / sizeof(image_types[0]);

    // 检查目录是否存在
    for (int i = 0; i < dir_count; i++)
    {
        DIR *dir = opendir(search_dirs[i]);
        if (!dir)
        {
            printf("Warning: Directory not found: %s\n", search_dirs[i]);
            continue;
        }
        closedir(dir);

        for (int j = 0; j < type_count; j++)
        {
            dir_read_file_recursive(search_dirs[i], image_paths, &image_count, image_types[j]);
        }
    }

    // 设置全局变量
    image_paths_size = image_count;

    // 调试输出找到的图片
    printf("Found %d images:\n", image_paths_size);
    for (int i = 0; i < image_paths_size; i++)
    {
        printf("  %d: %s\n", i, image_paths[i]);
    }

    // 初始化显示设备
    if (dev_init() != 0)
    {
        free(image_paths);
        image_paths = NULL;
        return -1;
    }

    // 显示找到的图片
    while (1)
    {
        if (current_image_index >= image_paths_size)
        {
            current_image_index = 0; // 循环显示
        }

        printf("Displaying: %s (index %d/%d)\n",
               image_paths[current_image_index],
               current_image_index,
               image_paths_size - 1);

        show_image(image_paths[current_image_index], 0, 0, 800, 480);

        // 等待触摸事件切换图片
        struct input_event ev;
        int x = 0;
        while (1)
        {
            if (read(ts_fd, &ev, sizeof(ev)) != sizeof(ev))
            {
                if (errno == EAGAIN)
                    continue;
                perror("Read error");
                break;
            }

            switch (ev.type)
            {
            case EV_ABS:
                handle_abs_event(&ev, &x);
                break;
            case EV_KEY:
            {
                handle_key_event(&ev, x);
                goto image_changed;
            }
            case EV_SYN:
                handle_sync_event();
                break;
            }
        }

    image_changed:
        // 这里不需要额外操作，因为handle_key_event已经更新了current_image_index
        printf("Image changed to index: %d\n", current_image_index);
    }

    // 清理资源
    dev_uninit();
    free(image_paths);
    image_paths = NULL;
    return 0;
}