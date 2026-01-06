#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// 屏幕分辨率
#define SCREEN_WIDTH  720
#define SCREEN_HEIGHT 480

// 颜色定义
#define COLOR_BLACK   0x000000
#define COLOR_WHITE   0xFFFFFF
#define COLOR_RED     0xFF0000
#define COLOR_GREEN   0x00FF00
#define COLOR_BLUE    0x0000FF
#define COLOR_YELLOW  0xFFFF00

// RG34XX按键映射 (根据实际设备)
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

// 全局变量
int running = 1;
int frame_count = 0;
float animation_time = 0.0f;
time_t last_activity_time = 0;
FILE* log_file = NULL;

// 双缓冲相关
void* back_buffer = NULL;
size_t buffer_size = 0;

// 按键状态显示
char last_key_info[128] = "等待按键输入...";

// 日志函数
void log_message(const char* message) {
    if (log_file) {
        time_t now = time(NULL);
        char* time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0'; // 移除换行符
        fprintf(log_file, "[%s] %s\n", time_str, message);
        fflush(log_file);
    }
    printf("%s\n", message);
}

// 打开日志文件
int open_log_file() {
    log_file = fopen("/mnt/mmc/Roms/APPS/log.txt", "w");
    if (!log_file) {
        printf("警告: 无法创建日志文件\n");
        return -1;
    }
    log_message("=== RG34XX 硬件测试应用启动 ===");
    return 0;
}

// 关闭日志文件
void close_log_file() {
    if (log_file) {
        log_message("=== 应用退出 ===");
        fclose(log_file);
        log_file = NULL;
    }
}

// 简单的帧缓冲结构
typedef struct {
    int fd;
    void* framebuffer;
    size_t size;
    int width, height;
    int bpp; // bits per pixel
} framebuffer_t;

framebuffer_t fb;

// 初始化帧缓冲
int init_framebuffer() {
    fb.fd = open("/dev/fb0", O_RDWR);
    if (fb.fd < 0) {
        printf("无法打开帧缓冲设备 /dev/fb0\n");
        return -1;
    }
    
    // 获取可变屏幕信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb.fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("无法获取屏幕信息");
        close(fb.fd);
        return -1;
    }
    
    // 获取固定屏幕信息
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb.fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("无法获取固定屏幕信息");
        close(fb.fd);
        return -1;
    }
    
    fb.width = vinfo.xres;
    fb.height = vinfo.yres;
    fb.bpp = vinfo.bits_per_pixel;
    
    // 计算帧缓冲大小
    fb.size = fb.width * fb.height * fb.bpp / 8;
    
    // 映射帧缓冲内存
    fb.framebuffer = mmap(0, fb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb.fd, 0);
    if (fb.framebuffer == MAP_FAILED) {
        perror("无法映射帧缓冲内存");
        close(fb.fd);
        return -1;
    }
    
    // 设置16位色深 (如果需要)
    if (fb.bpp != 16 && fb.bpp != 32) {
        vinfo.bits_per_pixel = 16;
        vinfo.red.offset = 11;
        vinfo.red.length = 5;
        vinfo.green.offset = 5;
        vinfo.green.length = 6;
        vinfo.blue.offset = 0;
        vinfo.blue.length = 5;
        
        if (ioctl(fb.fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
            printf("警告: 无法设置色深，使用默认设置\n");
        }
        
        // 重新获取信息
        ioctl(fb.fd, FBIOGET_VSCREENINFO, &vinfo);
        fb.bpp = vinfo.bits_per_pixel;
        fb.size = fb.width * fb.height * fb.bpp / 8;
    }
    
    printf("帧缓冲初始化成功: %dx%d, %dbpp\n", fb.width, fb.height, fb.bpp);
    return 0;
}

// 初始化双缓冲
int init_double_buffer() {
    if (buffer_size == 0) {
        buffer_size = fb.size;
    }
    
    back_buffer = malloc(buffer_size);
    if (!back_buffer) {
        printf("无法分配后缓冲内存\n");
        return -1;
    }
    
    // 清空后缓冲
    memset(back_buffer, 0, buffer_size);
    printf("双缓冲初始化成功\n");
    return 0;
}

// 清理双缓冲
void cleanup_double_buffer() {
    if (back_buffer) {
        free(back_buffer);
        back_buffer = NULL;
    }
}

// 复制后缓冲到帧缓冲
void flip_buffer() {
    if (back_buffer && fb.framebuffer) {
        memcpy(fb.framebuffer, back_buffer, buffer_size);
        
        // 强制刷新
        if (fb.fd >= 0) {
            ioctl(fb.fd, FBIOBLANK, 0);
            
            struct fb_var_screeninfo vinfo;
            if (ioctl(fb.fd, FBIOGET_VSCREENINFO, &vinfo) >= 0) {
                ioctl(fb.fd, FBIOPAN_DISPLAY, &vinfo);
            }
        }
    }
}

// 清理帧缓冲
void cleanup_framebuffer() {
    cleanup_double_buffer(); // 先清理双缓冲
    if (fb.framebuffer && fb.framebuffer != MAP_FAILED) {
        munmap(fb.framebuffer, fb.size);
    }
    if (fb.fd >= 0) {
        close(fb.fd);
    }
}

// 设置像素颜色 (支持双缓冲)
void set_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= fb.width || y < 0 || y >= fb.height) {
        return;
    }
    
    int location = x + y * fb.width;
    
    // 使用后缓冲
    void* target_buffer = back_buffer ? back_buffer : fb.framebuffer;
    
    if (fb.bpp == 32) {
        // 32位色深
        unsigned int* pixel = (unsigned int*)target_buffer + location;
        *pixel = color;
    } else if (fb.bpp == 16) {
        // 16位色深 (RGB565)
        unsigned short* pixel = (unsigned short*)target_buffer + location;
        
        // 提取RGB分量
        unsigned int r = (color >> 16) & 0xFF;
        unsigned int g = (color >> 8) & 0xFF;
        unsigned int b = color & 0xFF;
        
        // 转换为RGB565
        unsigned short rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *pixel = rgb565;
    }
}

// 设置像素颜色到帧缓冲 (直接绘制)
void set_pixel_fb(int x, int y, unsigned int color) {
    if (x < 0 || x >= fb.width || y < 0 || y >= fb.height) {
        return;
    }
    
    int location = x + y * fb.width;
    
    if (fb.bpp == 32) {
        // 32位色深
        unsigned int* pixel = (unsigned int*)fb.framebuffer + location;
        *pixel = color;
    } else if (fb.bpp == 16) {
        // 16位色深 (RGB565)
        unsigned short* pixel = (unsigned short*)fb.framebuffer + location;
        
        // 提取RGB分量
        unsigned int r = (color >> 16) & 0xFF;
        unsigned int g = (color >> 8) & 0xFF;
        unsigned int b = color & 0xFF;
        
        // 转换为RGB565
        unsigned short rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *pixel = rgb565;
    }
}

// 绘制矩形
void draw_rect(int x, int y, int width, int height, unsigned int color) {
    for (int i = y; i < y + height && i < fb.height; i++) {
        for (int j = x; j < x + width && j < fb.width; j++) {
            set_pixel(j, i, color);
        }
    }
}

// 绘制圆形
void draw_circle(int cx, int cy, int radius, unsigned int color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                set_pixel(cx + x, cy + y, color);
            }
        }
    }
}

// 绘制文字 (简单像素文字)
void draw_text(int x, int y, const char* text, unsigned int color) {
    int char_width = 6;
    int char_height = 8;
    int spacing = 1;
    
    for (int i = 0; text[i] != '\0'; i++) {
        int char_x = x + i * (char_width + spacing);
        
        // 简单的字符绘制 (只绘制基本字符)
        char c = text[i];
        if (c >= ' ' && c <= '~') {
            // 绘制字符轮廓
            draw_rect(char_x, y, char_width, char_height, color);
        }
    }
}

// 绘制进度条
void draw_progress_bar(int x, int y, int width, int height, int progress, unsigned int color) {
    // 绘制边框
    draw_rect(x, y, width, height, COLOR_WHITE);
    
    // 绘制填充
    if (progress > 0) {
        int fill_width = (width - 2) * progress / 100;
        draw_rect(x + 1, y + 1, fill_width, height - 2, color);
    }
}

// 强制刷新帧缓冲
void refresh_framebuffer() {
    // 在某些系统上需要手动刷新
    if (fb.fd >= 0) {
        // 尝试刷新整个屏幕
        ioctl(fb.fd, FBIOBLANK, 0);
        
        // 或者使用pan display刷新
        struct fb_var_screeninfo vinfo;
        if (ioctl(fb.fd, FBIOGET_VSCREENINFO, &vinfo) >= 0) {
            ioctl(fb.fd, FBIOPAN_DISPLAY, &vinfo);
        }
    }
}

// 绘制界面
void draw_ui() {
    char debug_msg[256];
    sprintf(debug_msg, "绘制界面: %dx%d, %dbpp", fb.width, fb.height, fb.bpp);
    log_message(debug_msg);
    
    // 清屏 (使用鲜艳的颜色确保可见)
    draw_rect(0, 0, fb.width, fb.height, COLOR_BLACK);
    
    // 绘制标题背景
    draw_rect(0, 0, fb.width, 40, COLOR_BLUE);
    
    // 绘制底部信息栏
    draw_rect(0, fb.height - 60, fb.width, 60, COLOR_BLUE);
    
    // 绘制动画区域边框
    draw_rect(10, 50, fb.width - 20, fb.height - 120, COLOR_WHITE);
    
    // 在动画区域内绘制动画
    draw_rect(15, 55, fb.width - 30, fb.height - 130, COLOR_BLACK);
    
    // 绘制动画
    float t = animation_time;
    int circle_x = (int)(fb.width/2 + cos(t) * 100);
    int circle_y = (int)(fb.height/2 + sin(t * 2) * 60);
    
    // 确保圆形在动画区域内
    if (circle_x > 15 && circle_x < fb.width - 15 && circle_y > 55 && circle_y < fb.height - 55) {
        draw_circle(circle_x, circle_y, 30, COLOR_WHITE);
    }
    
    // 绘制旋转矩形
    float angle = t * 3;
    int rect_x = fb.width/2 + (int)(cos(angle) * 60);
    int rect_y = fb.height/2 + (int)(sin(angle) * 40);
    
    if (rect_x > 15 && rect_x < fb.width - 15 && rect_y > 55 && rect_y < fb.height - 55) {
        draw_rect(rect_x - 20, rect_y - 20, 40, 40, COLOR_GREEN);
    }
    
    // 绘制按键信息 (使用简单的像素块表示文字)
    draw_rect(20, fb.height - 50, fb.width - 40, 20, COLOR_BLACK);
    
    // 显示按键信息 (简化文字显示)
    int text_x = 30;
    int text_y = fb.height - 45;
    for (int i = 0; i < strlen(last_key_info) && i < 30; i++) {
        // 简单的字符显示 (每个字符用小方块表示)
        draw_rect(text_x + i * 15, text_y, 10, 10, COLOR_YELLOW);
    }
    
    // 绘制状态信息
    time_t current_time = time(NULL);
    int remaining_time = 15 - (int)(current_time - last_activity_time);
    
    // 在状态栏绘制像素块表示文字
    for (int i = 0; i < remaining_time && i < 15; i++) {
        draw_rect(10 + i * 20, fb.height - 20, 15, 15, COLOR_WHITE);
    }
    
    sprintf(debug_msg, "状态更新: Frame=%d Time=%.1fs AutoExit=%ds", frame_count, animation_time, remaining_time);
    log_message(debug_msg);
    
    // 复制后缓冲到帧缓冲
    flip_buffer();
    log_message("帧缓冲刷新完成");
}

// 检查自动退出
void check_auto_exit() {
    time_t current_time = time(NULL);
    if (current_time - last_activity_time >= 15) {
        printf("15秒无操作，自动退出应用\n");
        running = 0;
    }
    
    // 每5秒显示一次倒计时警告
    static int last_warning = 0;
    int elapsed = current_time - last_activity_time;
    if (elapsed >= 10 && elapsed != last_warning) {
        printf("⚠️  警告: %d秒后自动退出\n", 15 - elapsed);
        last_warning = elapsed;
    }
}

// 更新活动时间
void update_activity() {
    last_activity_time = time(NULL);
}

// 网络测试
int test_network() {
    log_message("开始网络测试...");
    
    // 测试DNS解析
    struct hostent* host = gethostbyname("www.baidu.com");
    if (host == NULL) {
        char error_msg[128];
        sprintf(error_msg, "DNS解析失败: %s", hstrerror(h_errno));
        log_message(error_msg);
        return -1;
    }
    
    char success_msg[128];
    sprintf(success_msg, "DNS解析成功: %s", host->h_name);
    log_message(success_msg);
    
    // 测试TCP连接
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_message("创建socket失败");
        return -1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_message("网络连接失败");
        close(sock);
        return -1;
    }
    
    log_message("网络连接成功");
    close(sock);
    return 0;
}

// 按键测试
void test_buttons() {
    log_message("开始按键测试...");
    
    // 优先尝试打开 /dev/input/event1
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
            char device_msg[64];
            sprintf(device_msg, "打开输入设备: %s", devices[i]);
            log_message(device_msg);
            break;
        }
    }
    
    if (fd < 0) {
        log_message("无法打开输入设备");
        return;
    }
    
    struct input_event ev;
    int button_states[16] = {0}; // 记录按键状态
    
    while (running) {
        if (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY) {
                int button_index = -1;
                const char* button_name = "UNKNOWN";
                
                switch (ev.code) {
                    case RG34XX_BTN_UP:    button_index = 0; button_name = "UP"; break;
                    case RG34XX_BTN_DOWN:  button_index = 1; button_name = "DOWN"; break;
                    case RG34XX_BTN_LEFT:  button_index = 2; button_name = "LEFT"; break;
                    case RG34XX_BTN_RIGHT: button_index = 3; button_name = "RIGHT"; break;
                    case RG34XX_BTN_A:     button_index = 4; button_name = "A"; break;
                    case RG34XX_BTN_B:     button_index = 5; button_name = "B"; break;
                    case RG34XX_BTN_X:     button_index = 6; button_name = "X"; break;
                    case RG34XX_BTN_Y:     button_index = 7; button_name = "Y"; break;
                    case RG34XX_BTN_L:     button_index = 8; button_name = "L1"; break;
                    case RG34XX_BTN_R:     button_index = 9; button_name = "R1"; break;
                    case RG34XX_BTN_SELECT:button_index = 10; button_name = "SELECT"; break;
                    case RG34XX_BTN_START: button_index = 11; button_name = "START"; break;
                    case RG34XX_BTN_M:     button_index = 12; button_name = "M"; break;
                    case RG34XX_BTN_L2:    button_index = 13; button_name = "L2"; break;
                    case RG34XX_BTN_R2:    button_index = 14; button_name = "R2"; break;
                }
                
                if (button_index >= 0) {
                    if (ev.value == 1 && button_states[button_index] == 0) {
                        char press_msg[64];
                        sprintf(press_msg, "按键按下: %s (code=%d)", button_name, ev.code);
                        log_message(press_msg);
                        button_states[button_index] = 1;
                        update_activity(); // 更新活动时间
                        
                        // 更新按键信息显示
                        sprintf(last_key_info, "按键: %s (%d)", button_name, ev.code);
                        
                        // 特殊按键处理
                        if (ev.code == RG34XX_BTN_START) {
                            log_message("START键按下 - 退出测试");
                            sprintf(last_key_info, "START键 - 退出中...");
                            running = 0;
                        }
                    } else if (ev.value == 0) {
                        char release_msg[64];
                        sprintf(release_msg, "按键释放: %s (code=%d)", button_name, ev.code);
                        log_message(release_msg);
                        button_states[button_index] = 0;
                        update_activity(); // 更新活动时间
                        
                        // 更新按键信息显示
                        sprintf(last_key_info, "释放: %s (%d)", button_name, ev.code);
                    }
                } else {
                    // 记录未知按键码
                    char unknown_msg[64];
                    sprintf(unknown_msg, "未知按键: code=%d, value=%d", ev.code, ev.value);
                    log_message(unknown_msg);
                    
                    // 更新按键信息显示
                    sprintf(last_key_info, "未知按键: %d", ev.code);
                }
            }
        }
        
        // 更新动画
        animation_time += 0.016f; // 假设60FPS
        frame_count++;
        draw_ui();
        
        // 检查自动退出
        check_auto_exit();
        
        usleep(16000); // ~60FPS
    }
    
    close(fd);
}

// 信号处理
void signal_handler(int sig) {
    printf("\n收到信号 %d，退出应用\n", sig);
    running = 0;
}

int main() {
    printf("=== RG34XX 硬件测试应用 v1.3 ===\n");
    printf("屏幕分辨率: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 打开日志文件
    open_log_file();
    
    // 检查设备权限
    log_message("=== 检查设备权限 ===");
    
    // 检查帧缓冲设备
    if (access("/dev/fb0", R_OK | W_OK) == 0) {
        log_message("帧缓冲设备 /dev/fb0 可读写");
    } else {
        log_message("错误: 帧缓冲设备 /dev/fb0 权限不足");
        log_message("尝试修复权限...");
        system("chmod 666 /dev/fb0");
        
        if (access("/dev/fb0", R_OK | W_OK) == 0) {
            log_message("帧缓冲设备权限修复成功");
        } else {
            log_message("错误: 无法修复帧缓冲设备权限");
            close_log_file();
            return 1;
        }
    }
    
    // 检查输入设备
    int input_devices = 0;
    for (int i = 0; i < 5; i++) {
        char device_path[32];
        sprintf(device_path, "/dev/input/event%d", i);
        if (access(device_path, R_OK) == 0) {
            char device_msg[64];
            sprintf(device_msg, "输入设备 %s 可读", device_path);
            log_message(device_msg);
            input_devices++;
        }
    }
    
    if (input_devices == 0) {
        log_message("警告: 未找到可读的输入设备");
    }
    
    // 初始化帧缓冲
    log_message("=== 初始化帧缓冲 ===");
    if (init_framebuffer() < 0) {
        log_message("帧缓冲初始化失败");
        close_log_file();
        return 1;
    }
    
    // 初始化双缓冲
    log_message("=== 初始化双缓冲 ===");
    if (init_double_buffer() < 0) {
        log_message("双缓冲初始化失败，使用单缓冲模式");
    }
    
    // 显示系统信息
    log_message("=== 系统信息 ===");
    char sys_info[128];
    sprintf(sys_info, "屏幕: %dx%d, %dbpp", fb.width, fb.height, fb.bpp);
    log_message(sys_info);
    
    // 测试基本绘制
    log_message("=== 测试基本绘制 ===");
    draw_rect(0, 0, fb.width, fb.height, COLOR_RED);
    flip_buffer();
    log_message("红色全屏测试完成");
    
    sleep(1);
    
    draw_rect(0, 0, fb.width, fb.height, COLOR_GREEN);
    flip_buffer();
    log_message("绿色全屏测试完成");
    
    sleep(1);
    
    draw_rect(0, 0, fb.width, fb.height, COLOR_BLUE);
    flip_buffer();
    log_message("蓝色全屏测试完成");
    
    sleep(1);
    
    // 网络测试
    log_message("=== 网络测试 ===");
    int network_result = test_network();
    if (network_result == 0) {
        log_message("网络测试通过");
    } else {
        log_message("网络测试失败");
    }
    
    // 按键和动画测试
    log_message("=== 按键和动画测试 ===");
    log_message("按任意按键进行测试，按START键退出");
    log_message("⚠️  15秒无操作将自动退出");
    
    // 初始化活动时间
    update_activity();
    
    // 先显示一个静态界面确保可见
    draw_ui();
    log_message("初始界面绘制完成");
    sleep(2);
    
    test_buttons();
    
    // 清理
    cleanup_framebuffer();
    close_log_file();
    
    // 显示最终统计
    printf("\n=== 测试完成 ===\n");
    printf("总帧数: %d\n", frame_count);
    printf("运行时间: %.1f秒\n", animation_time);
    
    return 0;
}
