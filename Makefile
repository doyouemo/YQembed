# 工具链配置
CROSS_COMPILE = arm-linux-
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

# 目录配置
SRC_DIR = .
LIB_DIR = ./lib
INC_DIR = ./lib/inc
LIB_SRC_DIR = ./lib/src
OUT_DIR = ./build

# 使用绝对路径
HOME_DIR = /home/doudou
JPEG_INC = $(HOME_DIR)/libjpeg/include
PNG_INC = $(HOME_DIR)/libpng/include
GIF_INC = $(HOME_DIR)/libgif/include
JPEG_LIB = $(HOME_DIR)/libjpeg/lib
PNG_LIB = $(HOME_DIR)/libpng/lib
GIF_LIB = $(HOME_DIR)/libgif/lib

# 系统头文件路径
SYSROOT = /usr/local/cross/gec6818-5.4.0/usr/arm-none-linux-gnueabi/sysroot
SYS_INC = $(SYSROOT)/usr/include

# 编译选项
CFLAGS = -Wall -O2 -pthread \
         -I$(INC_DIR) -I$(SYS_INC) \
         -I$(JPEG_INC) -I$(PNG_INC) -I$(GIF_INC) \
         -D_GNU_SOURCE

LDFLAGS = -L$(JPEG_LIB) -L$(PNG_LIB) -L$(GIF_LIB) \
          -ljpeg -lpng -lgif -lz -lm -pthread

# 源文件列表
SRCS = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/png.c \
    $(LIB_SRC_DIR)/display.c \
    $(LIB_SRC_DIR)/bmp.c \
    $(LIB_SRC_DIR)/gif.c \
    $(LIB_SRC_DIR)/jpg.c \
    $(LIB_SRC_DIR)/media.c \
    $(LIB_SRC_DIR)/touch.c

# 生成对应的目标文件列表(全部放在build根目录)
OBJS = $(addprefix $(OUT_DIR)/, \
       $(notdir $(SRCS:.c=.o)))

# 最终目标
TARGET = $(OUT_DIR)/main

# 默认目标
all: $(OUT_DIR) $(TARGET)

# 创建输出目录
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# 主程序链接
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)
	$(STRIP) $@

# 通用编译规则(处理来自不同目录的源文件)
$(OUT_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/%.o: $(LIB_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean