# RG34XX SDL2多语言应用 - 项目文档

## 🤖 AI作者信息

**开发助手**: Cascade (SWE-1.5)
**模型版本**: Cognition AI Coding Assistant
**开发时间**: 2026年1月
**专长**: C/C++开发、SDL2图形编程、跨平台编译、嵌入式系统

---

## 📋 项目概述

本项目为RG34XX掌机开发的SDL2多语言应用，支持中文、日文、韩文显示，具备完整的输入监听和动画功能。

## 🎮 按键监听与映射

### SDL2事件类型
- **SDL_JOYBUTTONDOWN/SDL_JOYBUTTONUP**: 手柄按键事件
- **SDL_JOYAXISMOTION**: 摇杆轴运动事件（方向键）
- **SDL_JOYHATMOTION**: 帽子运动事件（十字键）
- **SDL_KEYDOWN/SDL_KEYUP**: 键盘事件
- **SDL_MOUSEBUTTONDOWN/SDL_MOUSEBUTTONUP**: 鼠标事件

### 按键映射表
| 按键编号 | 显示名称 | 说明 |
|---------|---------|------|
| 0 | A | A按键 |
| 1 | B | B按键 |
| 2 | Y | Y按键（X和Y交换） |
| 3 | X | X按键（X和Y交换） |
| 4 | L1 | 左肩键1 |
| 5 | R1 | 右肩键1 |
| 6 | SELECT | 选择键（L2） |
| 7 | START | 开始键 |
| 8 | M | M键 |
| 9 | L2 | 左肩键2（L3） |
| 10 | R2 | 右肩键2 |
| 13 | VOL- | 音量减小 |
| 14 | VOL+ | 音量增加 |

### 方向键检测
- **轴运动**: Axis0/1（主摇杆），Axis2/3（副摇杆）
- **触发阈值**: 8000（降低阈值提高敏感度）
- **帽子运动**: Hat UP/DOWN/LEFT/RIGHT（十字键）

## 📚 程序依赖

### 核心库
- **SDL2**: 图形渲染和事件处理
- **SDL2_ttf**: 文字渲染
- **libc**: 标准C库
- **libm**: 数学库

### 字体依赖
- **Noto Sans CJK**: 支持中日韩等多语言字符

### 系统依赖
```bash
# Ubuntu/Debian
sudo apt-get install libsdl2-dev libsdl2-ttf-dev

# macOS
brew install sdl2 sdl2_ttf
```

## 🔧 编译方式

### Mac版本
```bash
# 使用Makefile
make sdl2-mac

# 直接编译
gcc -Wall -O2 -I/usr/include/SDL2 -D_REENTRANT src/sdl2-main.c -o rg34xx-sdl2-mac -lSDL2_ttf -lSDL2 -lm
```

### ARM版本（掌机）
```bash
# Docker方式（推荐）
./docker-build-sdl2.sh

# 交叉编译
make sdl2-arm CC=aarch64-linux-gnu-gcc

# 直接编译
aarch64-linux-gnu-gcc -Wall -O2 -D_GNU_SOURCE -D__CROSS_COMPILE__ -I/usr/include/SDL2 -D_REENTRANT src/sdl2-main.c -o rg34xx-sdl2-arm -lSDL2_ttf -lSDL2 -lm
```

### 编译参数说明
- **-Wall**: 开启所有警告
- **-O2**: 优化级别2
- **-D_GNU_SOURCE**: GNU扩展支持
- **-D__CROSS_COMPILE__**: 交叉编译标志
- **-D_REENTRANT**: 线程安全
- **-I**: 头文件路径
- **-l**: 链接库

## 🌏 中文显示问题解决方案

### 问题原因
1. **字体缺失**: 掌机系统缺少中文字体
2. **字体路径错误**: 字体文件路径不正确
3. **编码问题**: UTF-8编码未正确设置
4. **字体不支持**: 使用的字体不支持中文字符

### 解决方案
1. **使用Noto Sans CJK字体**
   - 专门支持中日韩字符
   - 开源免费字体
   - 字符覆盖全面

2. **多路径字体加载**
   ```c
   const char* font_paths[] = {
       "./NotoSansCJK-Regular.ttc",     // 当前目录
       "/mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc",  // 掌机应用目录
       "NotoSansCJK-Regular.ttc",       // 相对路径
       // ... 其他备用路径
   };
   ```

3. **UTF-8环境设置**
   ```c
   setlocale(LC_ALL, "en_US.UTF-8");
   ```

4. **字体文件部署**
   - 将字体文件复制到应用目录
   - 设置正确权限（644）

## 🕳 踩过的坑

### 1. SDL2初始化问题
**问题**: 手柄事件无法接收
**原因**: 未初始化SDL_INIT_JOYSTICK子系统
**解决**: 
```c
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
SDL_JoystickEventState(SDL_ENABLE);
```

### 2. 交叉编译错误
**问题**: 缺少系统头文件
**原因**: 交叉编译时包含了Linux特定头文件
**解决**: 条件编译
```c
#ifndef __CROSS_COMPILE__
#include <sys/utsname.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#endif
```

### 3. 按键映射混乱
**问题**: 按键显示与实际不符
**原因**: SDL2按键编号与硬件按键不对应
**解决**: 实际测试并建立映射表

### 4. 15秒自动退出失效
**问题**: 从程序启动开始计算，不是从最后输入开始
**原因**: 时间戳更新逻辑错误
**解决**: 
```c
time_t last_input_time;
// 在每个输入事件中更新
app->last_input_time = time(NULL);
```

### 5. 方向键无响应
**问题**: 摇杆轴运动检测阈值过高
**原因**: 阈值设置为10000，过于敏感
**解决**: 降低阈值到8000，增加轴检测范围

### 6. 字体加载失败
**问题**: 掌机上字体显示为方框
**原因**: 字体文件路径不正确
**解决**: 多路径尝试，优先应用目录

### 7. 内存泄漏
**问题**: 手柄资源未正确释放
**原因**: SDL_JoystickOpened函数不存在
**解决**: SDL_Quit()自动清理，无需手动释放

### 8. Docker构建时区问题
**问题**: 构建时区设置交互式提示
**原因**: tzdata安装需要时区配置
**解决**: 
```dockerfile
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
```

## 📁 项目结构

```
rg34xx-native-app/
├── src/
│   └── sdl2-main.c          # 主程序源码
├── rg34xx-sdl2-arm          # ARM可执行文件
├── NotoSansCJK-Regular.ttc   # 多语言字体文件
├── Dockerfile.sdl2          # Docker构建文件
├── Makefile                 # 编译配置
└── install.sh               # 安装脚本
```

## 🚀 部署流程

1. **编译ARM版本**
   ```bash
   docker build -t rg34xx-sdl2-builder -f Dockerfile.sdl2 .
   docker run --rm -v $(pwd):/output rg34xx-sdl2-builder cp rg34xx-sdl2-arm /output/
   ```

2. **安装到掌机**
   ```bash
   ./install.sh [IP] [PASSWORD]
   ```

3. **运行应用**
   ```bash
   ssh root@[IP] 'cd /mnt/mmc/Roms/APPS && ./rg34xx-sdl2-arm'
   ```

## 🎯 功能特性

- ✅ 多语言支持（中文、繁体中文、日文、韩文）
- ✅ 手柄按键监听（完整映射）
- ✅ 方向键检测（摇杆+十字键）
- ✅ 键盘和鼠标输入支持
- ✅ 动画演示（彩色移动方块）
- ✅ 设备信息显示
- ✅ 15秒无操作自动退出
- ✅ 完整的错误处理和日志

## 📝 调试技巧

1. **查看日志**
   ```bash
   ssh root@[IP] 'cd /mnt/mmc/Roms/APPS && ./rg34xx-sdl2-arm 2>&1 | tee debug.log'
   ```

2. **杀死进程**
   ```bash
   ssh root@[IP] 'pkill rg34xx-sdl2-arm'
   ```

3. **检查输入设备**
   ```bash
   ssh root@[IP] 'ls -la /dev/input/'
   ```

---

**RG34XX SDL2多语言应用 - 完整的掌机GUI解决方案！** 🎮✨
