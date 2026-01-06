#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

// 全局变量
int running = 1;
int frame_count = 0;
float animation_time = 0.0f;
time_t last_activity_time = 0;

// 模拟帧缓冲 (本地测试用)
typedef struct {
    int width, height;
    int bpp;
    unsigned int* pixels; // 模拟像素数据
} mock_framebuffer_t;

mock_framebuffer_t fb;

// 初始化模拟帧缓冲
int init_framebuffer() {
    fb.width = SCREEN_WIDTH;
    fb.height = SCREEN_HEIGHT;
    fb.bpp = 32;
    fb.pixels = malloc(fb.width * fb.height * sizeof(unsigned int));
    
    if (!fb.pixels) {
        printf("无法分配模拟帧缓冲内存\n");
        return -1;
    }
    
    printf("模拟帧缓冲初始化成功: %dx%d, %dbpp\n", fb.width, fb.height, fb.bpp);
    return 0;
}

// 清理帧缓冲
void cleanup_framebuffer() {
    if (fb.pixels) {
        free(fb.pixels);
        fb.pixels = NULL;
    }
}

// 设置像素颜色
void set_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= fb.width || y < 0 || y >= fb.height) return;
    fb.pixels[y * fb.width + x] = color;
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

// 绘制动画
void draw_animation() {
    // 清屏
    draw_rect(0, 0, fb.width, fb.height, COLOR_BLACK);
    
    // 计算动画位置
    float t = animation_time;
    int circle_x = (int)(fb.width/2 + cos(t) * 150);
    int circle_y = (int)(fb.height/2 + sin(t * 2) * 100);
    
    // 绘制多个圆形
    draw_circle(circle_x, circle_y, 30, COLOR_RED);
    draw_circle(fb.width - circle_x, fb.height - circle_y, 20, COLOR_GREEN);
    draw_circle(circle_x, fb.height - circle_y, 25, COLOR_BLUE);
    
    // 绘制旋转的矩形
    float angle = t * 3;
    int rect_x = fb.width/2 + (int)(cos(angle) * 100);
    int rect_y = fb.height/2 + (int)(sin(angle) * 100);
    draw_rect(rect_x - 15, rect_y - 15, 30, 30, COLOR_YELLOW);
    
    // 显示帧数和倒计时
    char fps_text[64];
    time_t current_time = time(NULL);
    int remaining_time = 15 - (int)(current_time - last_activity_time);
    sprintf(fps_text, "Frame: %d Time: %.1fs AutoExit: %ds", frame_count, animation_time, remaining_time);
    
    // 在控制台显示
    printf("\r%s", fps_text);
    fflush(stdout);
}

// 检查自动退出
void check_auto_exit() {
    time_t current_time = time(NULL);
    if (current_time - last_activity_time >= 15) {
        printf("\n15秒无操作，自动退出应用\n");
        running = 0;
    }
    
    // 每5秒显示一次倒计时警告
    static int last_warning = 0;
    int elapsed = current_time - last_activity_time;
    if (elapsed >= 10 && elapsed != last_warning) {
        printf("\n⚠️  警告: %d秒后自动退出\n", 15 - elapsed);
        last_warning = elapsed;
    }
}

// 更新活动时间
void update_activity() {
    last_activity_time = time(NULL);
}

// 网络测试
int test_network() {
    printf("\n开始网络测试...\n");
    
    // 测试DNS解析
    struct hostent* host = gethostbyname("www.google.com");
    if (host == NULL) {
        printf("❌ DNS解析失败\n");
        return -1;
    }
    printf("✅ DNS解析成功: %s\n", host->h_name);
    
    // 测试TCP连接
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("❌ 创建socket失败\n");
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
        printf("❌ 网络连接失败\n");
        close(sock);
        return -1;
    }
    
    printf("✅ 网络连接成功\n");
    close(sock);
    return 0;
}

// 模拟按键测试
void test_buttons() {
    printf("\n开始模拟按键测试...\n");
    printf("按任意键模拟按键，按 'q' 退出\n");
    printf("⚠️  15秒无操作将自动退出\n");
    
    // 初始化活动时间
    update_activity();
    
    char input;
    int button_count = 0;
    
    while (running) {
        // 模拟按键输入
        if (scanf(" %c", &input) == 1) {
            button_count++;
            update_activity(); // 更新活动时间
            
            switch (input) {
                case 'q':
                case 'Q':
                    printf("退出键按下 - 退出测试\n");
                    running = 0;
                    break;
                case 'w':
                case 'W':
                    printf("模拟按键: UP\n");
                    break;
                case 's':
                case 'S':
                    printf("模拟按键: DOWN\n");
                    break;
                case 'a':
                case 'A':
                    printf("模拟按键: LEFT\n");
                    break;
                case 'd':
                case 'D':
                    printf("模拟按键: RIGHT\n");
                    break;
                case ' ':
                    printf("模拟按键: A\n");
                    break;
                default:
                    printf("模拟按键: %c\n", input);
                    break;
            }
        }
        
        // 更新动画
        animation_time += 0.016f; // 假设60FPS
        frame_count++;
        draw_animation();
        
        // 检查自动退出
        check_auto_exit();
        
        usleep(16000); // ~60FPS
    }
}

// 信号处理
void signal_handler(int sig) {
    printf("\n收到信号 %d，退出应用\n", sig);
    running = 0;
}

int main() {
    printf("=== RG34XX 硬件测试应用 (本地版本) ===\n");
    printf("屏幕分辨率: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化帧缓冲
    if (init_framebuffer() < 0) {
        printf("帧缓冲初始化失败\n");
        return 1;
    }
    
    // 显示系统信息
    printf("\n=== 系统信息 ===\n");
    printf("屏幕: %dx%d, %dbpp\n", fb.width, fb.height, fb.bpp);
    printf("编译时间: %s %s\n", __DATE__, __TIME__);
    
    // 网络测试
    printf("\n=== 网络测试 ===\n");
    int network_result = test_network();
    if (network_result == 0) {
        printf("✅ 网络测试通过\n");
    } else {
        printf("❌ 网络测试失败\n");
    }
    
    // 按键和动画测试
    printf("\n=== 按键和动画测试 ===\n");
    printf("控制说明:\n");
    printf("  W/S/A/D - 模拟方向键\n");
    printf("  空格   - 模拟A键\n");
    printf("  Q     - 退出测试\n");
    printf("  其他键 - 模拟其他按键\n");
    
    test_buttons();
    
    // 清理
    cleanup_framebuffer();
    
    printf("\n=== 测试完成 ===\n");
    printf("总帧数: %d\n", frame_count);
    printf("运行时间: %.1f秒\n", animation_time);
    printf("平均FPS: %.1f\n", frame_count / (animation_time > 0 ? animation_time : 1));
    
    return 0;
}
