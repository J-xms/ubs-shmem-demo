# Makefile for UBS Shared Memory Demo
#
# UBS库已安装时直接:
#   make
#   ./shm_test [共享内存名] [迭代次数]

CC = gcc
CFLAGS = -Wall -O2 -std=c99

# 自动检测UBS库和头文件路径
# 优先使用系统路径，找不到再用UBS_HOME
SYS_INC_PATHS := /usr/include /usr/local/include /opt/ubs/include
SYS_LIB_PATHS := /usr/lib /usr/lib64 /usr/local/lib /usr/lib/aarch64-linux-gnu /opt/ubs/lib

# 动态检测头文件
UBS_INC := $(wildcard $(addsuffix /ubs_mem.h, $(SYS_INC_PATHS)))
ifneq ($(UBS_INC),)
    UBS_HOME := $(dir $(UBS_INC))/..
    override UBS_HOME := $(abspath $(UBS_HOME))
endif

# 动态检测库文件
ifneq ($(wildcard $(addsuffix /libubsm.so, $(SYS_LIB_PATHS))),)
    DETECTED_LIB := $(firstword $(wildcard $(addsuffix /libubsm.so, $(SYS_LIB_PATHS))))
    DETECTED_LIB_DIR := $(dir $(DETECTED_LIB))
endif

# 如果环境变量UBS_HOME存在，覆盖自动检测
ifdef UBS_HOME
    override UBS_INC_DIR = $(UBS_HOME)/src/app_lib/include
    override UBS_LIB_DIR = $(UBS_HOME)/build/lib
else ifneq ($(DETECTED_LIB_DIR),)
    override UBS_INC_DIR = $(DETECTED_LIB_DIR)/../include
    override UBS_LIB_DIR = $(DETECTED_LIB_DIR)
else
    # 回退到默认猜测路径
    override UBS_INC_DIR = /usr/include
    override UBS_LIB_DIR = /usr/lib
endif

CFLAGS += -I$(UBS_INC_DIR)
LDFLAGS := -L$(UBS_LIB_DIR) -lubsm -lpthread -lm -ldl

TARGET = shm_test
SRC = shm_test.c

.PHONY: all clean run help

all: $(TARGET)
	@echo "编译完成: ./$(TARGET)"
	@echo "  头文件: $(UBS_INC_DIR)"
	@echo "  库文件: $(UBS_LIB_DIR)"

$(TARGET): $(SRC)
	@echo "编译UBS共享内存测试..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) $(ARGS)

help:
	@echo "用法:"
	@echo "  make                    # 直接编译(自动检测UBS路径)"
	@echo "  make UBS_HOME=/path     # 指定ubs-mem源码目录"
	@echo "  make run ARGS='name n'  # 编译并运行"
	@echo ""
	@echo "运行示例:"
	@echo "  ./shm_test demo_shm 100000"
