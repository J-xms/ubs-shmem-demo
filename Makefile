# Makefile for UBS Shared Memory Test Demo
#
# 用法:
#   make              # 编译 (需要设置 UBS_HOME 或手动指定路径)
#   make clean        # 清理
#   make install      # 安装到系统
#
# 手动指定路径编译:
#   make UBS_HOME=/path/to/ubs-mem
#

# 默认设置
UBS_HOME ?= /opt/ubs-mem
INSTALL_PREFIX ?= /usr/local

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
CFLAGS += -I$(UBS_HOME)/src/app_lib/include

# 链接器设置
LDFLAGS = -L$(UBS_HOME)/build/lib
LDFLAGS += -lubsm -lpthread -lm -ldl -lnuma

# 目标设置
TARGET = shm_test
SRC = shm_test.c

# 头文件搜索路径
INCLUDE_PATHS = $(UBS_HOME)/src/app_lib/include

.PHONY: all clean install help

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "编译 UBS 共享内存测试程序..."
	@echo "  UBS_HOME: $(UBS_HOME)"
	@echo "  头文件路径: $(INCLUDE_PATHS)"
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "编译完成: ./$(TARGET)"

clean:
	@echo "清理编译产物..."
	rm -f $(TARGET)
	@echo "清理完成"

install: $(TARGET)
	@echo "安装到 $(INSTALL_PREFIX)/bin/..."
	install -m 755 $(TARGET) $(INSTALL_PREFIX)/bin/
	@echo "安装完成"

help:
	@echo "UBS Shared Memory 测试 Demo 编译工具"
	@echo ""
	@echo "用法:"
	@echo "  make                        # 使用默认UBS_HOME=$(UBS_HOME)编译"
	@echo "  make UBS_HOME=/path/to/ubs  # 指定ubs-mem源码目录"
	@echo "  make clean                  # 清理编译产物"
	@echo "  make install                # 安装到系统"
	@echo "  make help                   # 显示本帮助"
	@echo ""
	@echo "前提条件:"
	@echo "  1. 已构建并安装 ubs-mem 库"
	@echo "  2. 设置 UBS_HOME 指向 ubs-mem 源码目录"
	@echo ""
	@echo "编译示例:"
	@echo "  git clone https://atomgit.com/openeuler/ubs-mem.git"
	@echo "  cd ubs-mem && sh build.sh -t release"
	@echo "  cd ../shmTest"
	@echo "  make UBS_HOME=/path/to/ubs-mem"
