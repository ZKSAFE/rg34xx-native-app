#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#ifndef __CROSS_COMPILE__
#include <sys/utsname.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#endif
#include <locale.h>

// 屏幕尺寸
#define SCREEN_WIDTH  720
#define SCREEN_HEIGHT 480

// 颜色定义
#define COLOR_BLACK   0x000000
#define COLOR_WHITE   0xFFFFFF
#define COLOR_RED     0xFF0000
#define COLOR_GREEN   0x00FF00
#define COLOR_BLUE    0x0000FF
#define COLOR_YELLOW  0xFFFF00

// 应用上下文
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* font_small;
    TTF_Font* font_chinese;  // 中文字体
    
    // 帧缓冲模式 (Linux嵌入式)
    int framebuffer_mode;
    void* fb_memory;
    int fb_fd;
    
    // 系统信息
    char system_info[512];
    char device_info[256];
    
    // 输入信息
    char input_info[256];
    char last_key_info[256];
    
    // 运行状态
    int running;
    int frame_count;
    time_t start_time;
    time_t last_input_time;  // 最后一次输入时间
    
    // 动画相关
    float box_x, box_y;
    float box_vx, box_vy;
    int box_size;
    unsigned int box_color;
    
} app_context_t;

// 日志文件
FILE* log_file = NULL;

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
    log_message("=== RG34XX SDL2 应用启动 ===");
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

// 获取系统信息
void get_system_info(app_context_t* app) {
#ifndef __CROSS_COMPILE__
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        sprintf(app->system_info, "System: %s %s | CPU: %s | Host: %s", 
                sys_info.sysname, sys_info.release, sys_info.machine, sys_info.nodename);
    } else {
        strcpy(app->system_info, "System: Unknown");
    }
#else
    strcpy(app->system_info, "System: RG34XX Handheld | Cross-compiled");
#endif
    
    sprintf(app->device_info, "RG34XX Handheld | Screen: %dx%d | SDL2 Rendering", 
            SCREEN_WIDTH, SCREEN_HEIGHT);
}

// 初始化动画
void init_animation(app_context_t* app) {
    app->box_x = SCREEN_WIDTH / 2.0f;
    app->box_y = SCREEN_HEIGHT / 2.0f;
    app->box_vx = 2.0f;  // 水平速度
    app->box_vy = 1.5f;  // 垂直速度
    app->box_size = 40;
    app->box_color = COLOR_RED;
}

// 更新动画
void update_animation(app_context_t* app) {
    // 更新位置
    app->box_x += app->box_vx;
    app->box_y += app->box_vy;
    
    // 边界碰撞检测
    if (app->box_x <= 0 || app->box_x + app->box_size >= SCREEN_WIDTH) {
        app->box_vx = -app->box_vx;
        app->box_x = (app->box_x <= 0) ? 0 : SCREEN_WIDTH - app->box_size;
        
        // 碰撞时改变颜色
        app->box_color = (app->box_color == COLOR_RED) ? COLOR_BLUE : COLOR_RED;
    }
    
    if (app->box_y <= 0 || app->box_y + app->box_size >= SCREEN_HEIGHT) {
        app->box_vy = -app->box_vy;
        app->box_y = (app->box_y <= 0) ? 0 : SCREEN_HEIGHT - app->box_size;
        
        // 碰撞时改变颜色
        app->box_color = (app->box_color == COLOR_RED) ? COLOR_GREEN : COLOR_RED;
    }
}

// 初始化SDL2
int init_sdl2(app_context_t* app) {
    log_message("=== Initializing SDL2 ===");
    
    // 初始化SDL2 (包含视频和手柄子系统)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        char error_msg[128];
        sprintf(error_msg, "SDL2 initialization failed: %s", SDL_GetError());
        log_message(error_msg);
        return -1;
    }
    
    // 初始化手柄子系统
    SDL_JoystickEventState(SDL_ENABLE);
    
    // 检测手柄数量
    int num_joysticks = SDL_NumJoysticks();
    char joystick_info[128];
    sprintf(joystick_info, "Detected %d joystick(s)", num_joysticks);
    log_message(joystick_info);
    
    // 打开所有手柄
    for (int i = 0; i < num_joysticks; i++) {
        SDL_Joystick* joystick = SDL_JoystickOpen(i);
        if (joystick) {
            char joy_name[128];
            sprintf(joy_name, "Opened joystick %d: %s", i, SDL_JoystickName(joystick));
            log_message(joy_name);
        }
    }
    
    // 初始化TTF
    if (TTF_Init() < 0) {
        char error_msg[128];
        sprintf(error_msg, "TTF initialization failed: %s", TTF_GetError());
        log_message(error_msg);
        return -1;
    }
    
    // 创建窗口
    #ifdef __linux__
    #ifndef __CROSS_COMPILE__
        // 检查是否为帧缓冲模式
        if (access("/dev/fb0", F_OK) == 0) {
            log_message("Framebuffer device detected, using framebuffer mode");
            app->framebuffer_mode = 1;
            
            // 尝试从帧缓冲创建窗口
            int fb_fd = open("/dev/fb0", O_RDWR);
            if (fb_fd >= 0) {
                struct fb_var_screeninfo vinfo;
                if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) >= 0) {
                    log_message("Creating SDL2 window from framebuffer");
                    app->fb_fd = fb_fd;
                    app->window = SDL_CreateWindow("RG34XX Test", 
                                              SDL_WINDOWPOS_UNDEFINED, 
                                              SDL_WINDOWPOS_UNDEFINED, 
                                              vinfo.xres, vinfo.yres, 
                                              SDL_WINDOW_SHOWN);
                } else {
                    close(fb_fd);
                }
            }
        }
    #endif
    #endif
    
    // 如果不是帧缓冲模式，创建普通窗口
    if (!app->window) {
        log_message("Creating normal SDL2 window");
        app->framebuffer_mode = 0;
        app->window = SDL_CreateWindow("RG34XX Test", 
                                      SDL_WINDOWPOS_CENTERED, 
                                      SDL_WINDOWPOS_CENTERED, 
                                      SCREEN_WIDTH, SCREEN_HEIGHT, 
                                      SDL_WINDOW_SHOWN);
    }
    
    if (!app->window) {
        char error_msg[128];
        sprintf(error_msg, "Window creation failed: %s", SDL_GetError());
        log_message(error_msg);
        return -1;
    }
    
    // 创建渲染器
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
    if (!app->renderer) {
        char error_msg[128];
        sprintf(error_msg, "Renderer creation failed: %s", SDL_GetError());
        log_message(error_msg);
        return -1;
    }
    
    // 加载字体 (使用Noto Sans CJK字体)
    // 优先使用Noto Sans CJK字体，专门支持中日韩等东亚语言
    const char* font_paths[] = {
        "./NotoSansCJK-Regular.ttc",     // 当前目录的字体
        "/mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc",  // 掌机应用目录
        "NotoSansCJK-Regular.ttc",       // 相对路径
        "NotoSans-Regular.ttf",         // 普通Noto Sans字体
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Times.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/arial.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
        "arial.ttf",
        "FreeSans.ttf",
        NULL
    };
    
    // 加载主字体
    for (int i = 0; font_paths[i]; i++) {
        app->font = TTF_OpenFont(font_paths[i], 18);
        if (app->font) {
            char font_msg[128];
            sprintf(font_msg, "Font loaded successfully: %s", font_paths[i]);
            log_message(font_msg);
            break;
        }
    }
    
    if (!app->font) {
        log_message("Error: Cannot load any font for main text");
        return -1;
    }
    
    // 小字体
    for (int i = 0; font_paths[i]; i++) {
        app->font_small = TTF_OpenFont(font_paths[i], 14);
        if (app->font_small) {
            break;
        }
    }
    if (!app->font_small) {
        app->font_small = app->font; // 备用
    }
    
    // 中文字体优先使用CJK字体
    for (int i = 0; font_paths[i]; i++) {
        app->font_chinese = TTF_OpenFont(font_paths[i], 18);
        if (app->font_chinese) {
            break;
        }
    }
    if (!app->font_chinese) {
        log_message("Warning: Cannot load Chinese font, using main font");
        app->font_chinese = app->font; // 备用
    }
    
    log_message("SDL2 initialization successful");
    return 0;
}

// 渲染文字
void render_text(app_context_t* app, const char* text, int x, int y, SDL_Color color) {
    if (!app->font || !text) return;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(app->font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app->renderer, texture, NULL, &dst_rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// 渲染小文字
void render_text_small(app_context_t* app, const char* text, int x, int y, SDL_Color color) {
    if (!app->font_small || !text) return;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(app->font_small, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app->renderer, texture, NULL, &dst_rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// 渲染中文文字
void render_chinese_text(app_context_t* app, const char* text, int x, int y, SDL_Color color) {
    TTF_Font* font = app->font_chinese ? app->font_chinese : app->font;
    if (!font || !text) return;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app->renderer, texture, NULL, &dst_rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// 渲染界面
void render_ui(app_context_t* app) {
    // 清屏
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    
    // 绘制标题栏
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 128, 255);
    SDL_Rect title_bar = {0, 0, SCREEN_WIDTH, 40};
    SDL_RenderFillRect(app->renderer, &title_bar);
    
    // 绘制系统信息
    SDL_Color white = {255, 255, 255, 255};
    render_text_small(app, app->system_info, 10, 10, white);
    render_text_small(app, app->device_info, 10, 25, white);
    
    // 绘制输入信息区域
    SDL_SetRenderDrawColor(app->renderer, 0, 64, 64, 255);
    SDL_Rect input_area = {0, 50, SCREEN_WIDTH, 80};
    SDL_RenderFillRect(app->renderer, &input_area);
    
    SDL_Color yellow = {255, 255, 0, 255};
    render_text(app, "=== Input Monitoring ===", 10, 60, yellow);
    render_text_small(app, app->input_info, 10, 80, white);
    render_text_small(app, app->last_key_info, 10, 100, white);
    
    // 绘制按键状态区域
    SDL_SetRenderDrawColor(app->renderer, 0, 128, 0, 255);
    SDL_Rect status_area = {0, 140, SCREEN_WIDTH, 60};
    SDL_RenderFillRect(app->renderer, &status_area);
    
    SDL_Color green = {0, 255, 0, 255};
    render_text(app, "=== Key Status ===", 10, 150, green);
    
    // 绘制运行时间
    time_t current_time = time(NULL);
    int run_time = current_time - app->start_time;
    char time_info[64];
    sprintf(time_info, "Runtime: %d seconds | Frames: %d", run_time, app->frame_count);
    render_text_small(app, time_info, 10, 175, white);
    
    // 绘制动画区域 (缩小高度，为底部信息留空间)
    SDL_SetRenderDrawColor(app->renderer, 32, 32, 32, 255);
    SDL_Rect animation_area = {0, 210, SCREEN_WIDTH, 120};
    SDL_RenderFillRect(app->renderer, &animation_area);
    
    // 绘制动画标题
    SDL_Color magenta = {255, 0, 255, 255};
    render_text(app, "=== Animation Demo ===", 10, 220, magenta);
    
    // 绘制移动的方块
    SDL_SetRenderDrawColor(app->renderer, 
                         (app->box_color >> 16) & 0xFF,
                         (app->box_color >> 8) & 0xFF,
                         app->box_color & 0xFF, 255);
    SDL_Rect box_rect = {(int)app->box_x, (int)app->box_y, app->box_size, app->box_size};
    SDL_RenderFillRect(app->renderer, &box_rect);
    
    // 绘制方块信息
    char box_info[128];
    sprintf(box_info, "Box: (%.0f, %.0f) | Speed: (%.1f, %.1f) | Color: 0x%06X", 
            app->box_x, app->box_y, app->box_vx, app->box_vy, app->box_color);
    render_text_small(app, box_info, 10, 260, white);
    
    // 绘制中文测试
    SDL_Color orange = {255, 165, 0, 255};
    render_chinese_text(app, "中文测试：方块动画演示", 10, 280, orange);
    render_chinese_text(app, "繁體中文：方塊動畫演示", 10, 300, orange);
    render_chinese_text(app, "日本語：ブロックアニメーション", 10, 320, orange);
    render_chinese_text(app, "한국어：블록 애니메이션", 10, 340, orange);
    render_chinese_text(app, "按任意键测试输入 | ESC退出程序", 10, 360, orange);
    
    // 绘制操作提示
    SDL_Color cyan = {0, 255, 255, 255};
    render_text_small(app, "Press any key to test | ESC to exit | F1 to switch input", 10, 380, cyan);
    
    // 绘制底部信息
    SDL_SetRenderDrawColor(app->renderer, 128, 0, 128, 255);
    SDL_Rect bottom_bar = {0, SCREEN_HEIGHT - 40, SCREEN_WIDTH, 40};
    SDL_RenderFillRect(app->renderer, &bottom_bar);
    
    render_text_small(app, "RG34XX SDL2 Multi-Input Test v1.0 | 中文支持", 10, SCREEN_HEIGHT - 30, white);
    
    // 呈现
    SDL_RenderPresent(app->renderer);
}

// 处理键盘输入
void handle_keyboard_event(app_context_t* app, SDL_KeyboardEvent* event) {
    const char* key_name = SDL_GetKeyName(event->keysym.sym);
    const char* state = (event->state == SDL_PRESSED) ? "Pressed" : "Released";
    
    char key_msg[256];
    sprintf(key_msg, "Keyboard: %s %s (Scancode: %d)", key_name, state, event->keysym.scancode);
    log_message(key_msg);
    
    sprintf(app->last_key_info, "Keyboard: %s %s", key_name, state);
    sprintf(app->input_info, "Input: Keyboard | Key: %s | State: %s", key_name, state);
}

// 处理鼠标输入
void handle_mouse_event(app_context_t* app, SDL_MouseButtonEvent* event) {
    const char* button_name = "Unknown";
    switch (event->button) {
        case SDL_BUTTON_LEFT:   button_name = "Left"; break;
        case SDL_BUTTON_RIGHT:  button_name = "Right"; break;
        case SDL_BUTTON_MIDDLE:  button_name = "Middle"; break;
        case SDL_BUTTON_X1:     button_name = "X1"; break;
        case SDL_BUTTON_X2:     button_name = "X2"; break;
    }
    
    const char* state = (event->state == SDL_PRESSED) ? "Pressed" : "Released";
    
    char mouse_msg[256];
    sprintf(mouse_msg, "Mouse: %s %s | Position: (%d, %d)", button_name, state, event->x, event->y);
    log_message(mouse_msg);
    
    sprintf(app->last_key_info, "Mouse: %s %s", button_name, state);
    sprintf(app->input_info, "Input: Mouse | Button: %s | Position: (%d, %d)", button_name, event->x, event->y);
}

// 处理手柄输入
void handle_joystick_event(app_context_t* app, SDL_JoyButtonEvent* event) {
    char button_name[16];  // 增加缓冲区大小
    switch (event->button) {
        case 0: strcpy(button_name, "A"); break;
        case 1: strcpy(button_name, "B"); break;
        case 2: strcpy(button_name, "Y"); break;     // X和Y交换
        case 3: strcpy(button_name, "X"); break;     // X和Y交换
        case 4: strcpy(button_name, "L1"); break;
        case 5: strcpy(button_name, "R1"); break;
        case 6: strcpy(button_name, "SELECT"); break;  // L2显示为SELECT
        case 7: strcpy(button_name, "START"); break;     // R2显示为L3
        case 8: strcpy(button_name, "M"); break;     // START显示为R2
        case 9: strcpy(button_name, "L2"); break;     // L3显示为L2
        case 10: strcpy(button_name, "R2"); break;    // 修正R3
        case 13: strcpy(button_name, "VOL-"); break; // 音量增加显示为Btn14
        case 14: strcpy(button_name, "VOL+"); break; // 音量增加显示为Btn14
        default: 
            sprintf(button_name, "Btn%d", (int)event->button);  // 修复格式警告
            break;
    }
    
    const char* state = (event->state == SDL_PRESSED) ? "Pressed" : "Released";
    
    char joystick_msg[256];
    sprintf(joystick_msg, "Joystick%d: %s %s", event->which, button_name, state);
    log_message(joystick_msg);
    
    sprintf(app->last_key_info, "Joystick%d: %s %s", event->which, button_name, state);
    sprintf(app->input_info, "Input: Joystick%d | Button: %s | State: %s", event->which, button_name, state);
}

// 处理事件
void handle_events(app_context_t* app) {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        // 调试模式：显示所有事件类型
        #ifdef DEBUG
        char debug_msg[128];
        sprintf(debug_msg, "Event type: %d", event.type);
        log_message(debug_msg);
        #endif
        
        switch (event.type) {
            case SDL_QUIT:
                log_message("Quit event received");
                app->running = 0;
                break;
                
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                log_message("Keyboard event detected");
                app->last_input_time = time(NULL);  // 更新最后输入时间
                handle_keyboard_event(app, &event.key);
                
                // 特殊按键处理
                if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            log_message("ESC key pressed - Exiting application");
                            app->running = 0;
                            break;
                        case SDLK_F1:
                            log_message("F1 key pressed - Switching input source");
                            sprintf(app->input_info, "Input source switched: Keyboard -> Mouse -> Joystick");
                            break;
                    }
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                log_message("Mouse event detected");
                app->last_input_time = time(NULL);  // 更新最后输入时间
                handle_mouse_event(app, &event.button);
                break;
                
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                log_message("Joystick button event detected");
                app->last_input_time = time(NULL);  // 更新最后输入时间
                handle_joystick_event(app, &event.jbutton);
                break;
                
            case SDL_JOYAXISMOTION:
                log_message("Joystick axis motion detected");
                app->last_input_time = time(NULL);  // 更新最后输入时间
                // 处理方向键 - 降低阈值提高敏感度
                char axis_info[128];
                sprintf(axis_info, "Axis%d: %d", event.jaxis.axis, event.jaxis.value);
                log_message(axis_info);
                
                if (event.jaxis.axis == 0) {  // X轴
                    if (event.jaxis.value < -8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Direction: LEFT", event.jaxis.which);
                        log_message("Direction: LEFT");
                    } else if (event.jaxis.value > 8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Direction: RIGHT", event.jaxis.which);
                        log_message("Direction: RIGHT");
                    }
                } else if (event.jaxis.axis == 1) {  // Y轴
                    if (event.jaxis.value < -8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Direction: UP", event.jaxis.which);
                        log_message("Direction: UP");
                    } else if (event.jaxis.value > 8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Direction: DOWN", event.jaxis.which);
                        log_message("Direction: DOWN");
                    }
                } else if (event.jaxis.axis == 2) {  // 可能是第二个摇杆X轴
                    if (event.jaxis.value < -8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Axis2: LEFT", event.jaxis.which);
                        log_message("Axis2: LEFT");
                    } else if (event.jaxis.value > 8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Axis2: RIGHT", event.jaxis.which);
                        log_message("Axis2: RIGHT");
                    }
                } else if (event.jaxis.axis == 3) {  // 可能是第二个摇杆Y轴
                    if (event.jaxis.value < -8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Axis2: UP", event.jaxis.which);
                        log_message("Axis2: UP");
                    } else if (event.jaxis.value > 8000) {
                        sprintf(app->input_info, "Input: Joystick%d | Axis2: DOWN", event.jaxis.which);
                        log_message("Axis2: DOWN");
                    }
                }
                break;
                
            case SDL_JOYHATMOTION:
                log_message("Joystick hat motion detected");
                app->last_input_time = time(NULL);  // 更新最后输入时间
                // 处理帽子运动（十字键）
                switch (event.jhat.value) {
                    case SDL_HAT_UP:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: UP", event.jhat.which);
                        log_message("Hat: UP");
                        break;
                    case SDL_HAT_DOWN:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: DOWN", event.jhat.which);
                        log_message("Hat: DOWN");
                        break;
                    case SDL_HAT_LEFT:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: LEFT", event.jhat.which);
                        log_message("Hat: LEFT");
                        break;
                    case SDL_HAT_RIGHT:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: RIGHT", event.jhat.which);
                        log_message("Hat: RIGHT");
                        break;
                    case SDL_HAT_CENTERED:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: CENTERED", event.jhat.which);
                        log_message("Hat: CENTERED");
                        break;
                    default:
                        sprintf(app->input_info, "Input: Joystick%d | Hat: %d", event.jhat.which, event.jhat.value);
                        log_message("Hat: OTHER");
                        break;
                }
                break;
                
            case SDL_JOYDEVICEADDED:
                {
                    char device_msg[128];
                    sprintf(device_msg, "Joystick device added: %d", event.jdevice.which);
                    log_message(device_msg);
                }
                break;
                
            case SDL_JOYDEVICEREMOVED:
                {
                    char device_msg[128];
                    sprintf(device_msg, "Joystick device removed: %d", event.jdevice.which);
                    log_message(device_msg);
                }
                break;
                
            default:
                // 记录未知事件
                #ifdef DEBUG
                char unknown_msg[128];
                sprintf(unknown_msg, "Unknown event type: %d", event.type);
                log_message(unknown_msg);
                #endif
                break;
        }
    }
}

// 清理资源
void cleanup(app_context_t* app) {
    log_message("=== Cleaning up resources ===");
    
    if (app->font) {
        TTF_CloseFont(app->font);
    }
    if (app->font_small) {
        TTF_CloseFont(app->font_small);
    }
    if (app->font_chinese) {
        TTF_CloseFont(app->font_chinese);
    }
    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }
    if (app->fb_fd >= 0) {
        close(app->fb_fd);
    }
    
    // 关闭所有手柄 - SDL2会自动清理，无需手动关闭
    // SDL_Quit()会自动关闭所有打开的手柄
    
    TTF_Quit();
    SDL_Quit();
    
    log_message("Resource cleanup completed");
}

// 主函数
int main(int argc, char* argv[]) {
    printf("=== RG34XX SDL2 Multi-Input Test Application ===\n");
    
    // 设置UTF-8编码
    setlocale(LC_ALL, "en_US.UTF-8");
    
    app_context_t app = {0};
    app.running = 1;
    app.frame_count = 0;
    app.start_time = time(NULL);
    app.fb_fd = -1;
    
    // 打开日志文件
    open_log_file();
    
    // 获取系统信息
    get_system_info(&app);
    log_message(app.system_info);
    log_message(app.device_info);
    
    // 初始化SDL2
    if (init_sdl2(&app) < 0) {
        log_message("SDL2 initialization failed");
        close_log_file();
        return 1;
    }
    
    // 初始化动画
    init_animation(&app);
    
    // 初始化输入信息
    strcpy(app.input_info, "Waiting for input...");
    strcpy(app.last_key_info, "No key input");
    app.last_input_time = time(NULL);  // 初始化最后输入时间
    
    log_message("=== Starting main loop ===");
    
    // 主循环
    while (app.running) {
        handle_events(&app);
        update_animation(&app);  // 更新动画
        render_ui(&app);
        
        app.frame_count++;
        
        // 15秒无输入自动退出 (仅Linux环境)
        time_t current_time = time(NULL);
        int idle_time = current_time - app.last_input_time;
        #ifdef __linux__
            if (idle_time >= 15) {
                log_message("15 seconds without input - Auto exit for handheld device");
                app.running = 0;
            }
        #endif
        
        // 控制帧率 (60FPS)
        SDL_Delay(16);
    }
    
    // 清理
    cleanup(&app);
    close_log_file();
    
    printf("\n=== Test completed ===\n");
    printf("Total frames: %d\n", app.frame_count);
    printf("Runtime: %ld seconds\n", time(NULL) - app.start_time);
    
    return 0;
}
