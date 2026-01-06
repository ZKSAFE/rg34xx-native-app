#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <string.h>

// RG34XX按键映射
#define RG34XX_BTN_UP     544
#define RG34XX_BTN_DOWN   545
#define RG34XX_BTN_LEFT   546
#define RG34XX_BTN_RIGHT  547
#define RG34XX_BTN_A      305
#define RG34XX_BTN_B      304
#define RG34XX_BTN_X      308
#define RG34XX_BTN_Y      307
#define RG34XX_BTN_L      310
#define RG34XX_BTN_R      311
#define RG34XX_BTN_SELECT 314
#define RG34XX_BTN_START  315
#define RG34XX_BTN_M      316
#define RG34XX_BTN_L2     312
#define RG34XX_BTN_R2     313

const char* get_key_name(int code) {
    switch (code) {
        case RG34XX_BTN_UP:     return "UP";
        case RG34XX_BTN_DOWN:   return "DOWN";
        case RG34XX_BTN_LEFT:   return "LEFT";
        case RG34XX_BTN_RIGHT:  return "RIGHT";
        case RG34XX_BTN_A:      return "A";
        case RG34XX_BTN_B:      return "B";
        case RG34XX_BTN_X:      return "X";
        case RG34XX_BTN_Y:      return "Y";
        case RG34XX_BTN_L:      return "L1";
        case RG34XX_BTN_R:      return "R1";
        case RG34XX_BTN_SELECT: return "SELECT";
        case RG34XX_BTN_START:  return "START";
        case RG34XX_BTN_M:      return "M";
        case RG34XX_BTN_L2:     return "L2";
        case RG34XX_BTN_R2:     return "R2";
        default:                return "UNKNOWN";
    }
}

int main() {
    printf("=== RG34XX 按键监听测试 ===\n");
    printf("按任意按键测试，按Ctrl+C退出\n\n");
    
    // 尝试打开输入设备
    int fd = -1;
    const char* devices[] = {
        "/dev/input/event1",  // RG34XX主要输入设备
        "/dev/input/event0",
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
        NULL
    };
    
    for (int i = 0; devices[i] != NULL; i++) {
        fd = open(devices[i], O_RDONLY | O_NONBLOCK);
        if (fd > 0) {
            printf("✅ 成功打开输入设备: %s\n", devices[i]);
            break;
        } else {
            printf("❌ 无法打开 %s: %s\n", devices[i], strerror(errno));
        }
    }
    
    if (fd < 0) {
        printf("❌ 无法打开任何输入设备\n");
        printf("请检查:\n");
        printf("1. 设备权限: ls -la /dev/input/event*\n");
        printf("2. 输入设备: cat /proc/bus/input/devices\n");
        return 1;
    }
    
    printf("🎮 开始监听按键...\n");
    printf("按键格式: [时间] 事件类型 按键码 按键名称 状态\n\n");
    
    struct input_event ev;
    int running = 1;
    
    while (running) {
        if (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY) {
                const char* key_name = get_key_name(ev.code);
                const char* action = (ev.value == 1) ? "按下" : 
                                   (ev.value == 0) ? "释放" : "重复";
                
                printf("[%ld.%06ld] EV_KEY %d %s %s\n", 
                       ev.time.tv_sec, ev.time.tv_usec, 
                       ev.code, key_name, action);
                
                // 如果按下START键，退出
                if (ev.code == RG34XX_BTN_START && ev.value == 1) {
                    printf("\n🎯 START键按下，退出监听\n");
                    running = 0;
                }
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("❌ 读取错误: %s\n", strerror(errno));
            break;
        }
        
        usleep(1000); // 1ms延迟
    }
    
    close(fd);
    printf("\n✅ 按键监听测试完成\n");
    return 0;
}
