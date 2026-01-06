# RG34XX 原生应用 Makefile

CC = aarch64-linux-gnu-gcc
CFLAGS = -Wall -O2 -static -D_GNU_SOURCE
LDFLAGS = -static
LIBS = -lm -lpthread

# 目标文件
TARGET = rg34xx-test
SRCDIR = src
OBJDIR = obj

# 源文件
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# 本地版本源文件
LOCAL_SOURCES = src/main_local.c
LOCAL_OBJECTS = $(LOCAL_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# SDL2版本源文件
SDL2_SOURCES = src/sdl2-main.c
SDL2_OBJECTS = $(SDL2_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# SDL2编译设置
SDL2_CFLAGS = -Wall -O2 -D_GNU_SOURCE $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL2_LIBS = $(shell pkg-config --libs sdl2 SDL2_ttf) -lm

# 默认目标
all: $(TARGET)

# 创建目标目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接最终可执行文件
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# 帧缓冲测试程序
fb-test: src/fb-test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

# 按键测试程序
key-test: src/key-test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

# SDL2版本 (ARM)
sdl2-arm: CC = aarch64-linux-gnu-gcc
sdl2-arm: $(SDL2_OBJECTS)
	$(CC) $(SDL2_CFLAGS) -D__CROSS_COMPILE__ -o rg34xx-sdl2-arm $^ $(SDL2_LIBS)

# SDL2版本 (Mac)
sdl2-mac: CC = gcc
sdl2-mac: $(SDL2_OBJECTS)
	$(CC) $(SDL2_CFLAGS) -o rg34xx-sdl2-mac $^ $(SDL2_LIBS)

# 本地版本
local: CC = gcc
local: CFLAGS += -DLOCAL_TEST
local: $(LOCAL_OBJECTS)
	$(CC) $(CFLAGS) -o rg34xx-test-local $(LOCAL_OBJECTS) $(LDFLAGS) $(LIBS)

# 清理
clean:
	rm -rf $(OBJDIR) $(TARGET) rg34xx-test-local fb-test key-test rg34xx-sdl2-arm rg34xx-sdl2-mac $(OBJECTS) $(LOCAL_OBJECTS) $(SDL2_OBJECTS)
	@echo "清理完成"

# 安装到设备
install: $(TARGET)
	@if [ -z "$(DEVICE_IP)" ]; then \
		echo "请设置设备IP: export DEVICE_IP=192.168.1.100"; \
		exit 1; \
	fi
	scp $(TARGET) root@$(DEVICE_IP):/tmp/
	ssh root@$(DEVICE_IP) "chmod +x /tmp/$(TARGET) && /tmp/$(TARGET)"

# 帮助
help:
	@echo "可用目标:"
	@echo "  all        - 编译ARM版本"
	@echo "  fb-test    - 编译帧缓冲测试程序"
	@echo "  key-test   - 编译按键测试程序"
	@echo "  sdl2-arm   - 编译SDL2版本 (ARM)"
	@echo "  sdl2-mac   - 编译SDL2版本 (Mac)"
	@echo "  local      - 编译本地测试版本"
	@echo "  clean      - 清理编译文件"
	@echo "  install    - 安装到设备"
	@echo "  help       - 显示此帮助"

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# 发布版本
release: CFLAGS += -DNDEBUG -s
release: $(TARGET)

# 帮助信息
help-all:
	@echo "RG34XX 原生应用编译选项:"
	@echo "  make          - 编译ARM版本"
	@echo "  make local    - 编译本地测试版本"
	@echo "  make debug    - 编译调试版本"
	@echo "  make release  - 编译发布版本"
	@echo "  make clean    - 清理编译文件"
	@echo "  make install  - 安装到设备 (需要DEVICE_IP)"
	@echo "  make help     - 显示帮助信息"

.PHONY: all clean local debug release install help
