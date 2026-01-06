#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <errno.h>
#include <string.h>

int main() {
    printf("=== 帧缓冲测试程序 ===\n");
    
    // 打开帧缓冲设备
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("错误: 无法打开 /dev/fb0 - %s\n", strerror(errno));
        return 1;
    }
    
    printf("✅ 成功打开 /dev/fb0\n");
    
    // 获取屏幕信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        printf("错误: 无法获取屏幕信息 - %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    
    printf("📱 屏幕信息:\n");
    printf("  分辨率: %dx%d\n", vinfo.xres, vinfo.yres);
    printf("  虚拟分辨率: %dx%d\n", vinfo.xres_virtual, vinfo.yres_virtual);
    printf("  色深: %d bpp\n", vinfo.bits_per_pixel);
    printf("  红色: %d bits, 偏移 %d\n", vinfo.red.length, vinfo.red.offset);
    printf("  绿色: %d bits, 偏移 %d\n", vinfo.green.length, vinfo.green.offset);
    printf("  蓝色: %d bits, 偏移 %d\n", vinfo.blue.length, vinfo.blue.offset);
    
    // 获取固定信息
    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        printf("错误: 无法获取固定屏幕信息 - %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    
    printf("📊 固定信息:\n");
    printf("  设备名称: %s\n", finfo.id);
    printf("  内存类型: %d\n", finfo.type);
    printf("  可视区域: %d bytes\n", finfo.smem_len);
    printf("  行长度: %d bytes\n", finfo.line_length);
    
    // 计算帧缓冲大小
    size_t size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    printf("💾 帧缓冲大小: %zu bytes\n", size);
    
    // 映射内存
    void* framebuffer = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (framebuffer == MAP_FAILED) {
        printf("错误: 无法映射帧缓冲内存 - %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    
    printf("✅ 成功映射帧缓冲内存\n");
    
    // 测试绘制
    printf("🎨 开始测试绘制...\n");
    
    // 清屏为红色
    printf("  绘制红色全屏...\n");
    if (vinfo.bits_per_pixel == 32) {
        unsigned int* pixel = (unsigned int*)framebuffer;
        unsigned int red = 0x00FF0000;
        for (size_t i = 0; i < size / 4; i++) {
            pixel[i] = red;
        }
    } else if (vinfo.bits_per_pixel == 16) {
        unsigned short* pixel = (unsigned short*)framebuffer;
        unsigned short red = 0xF800; // RGB565
        for (size_t i = 0; i < size / 2; i++) {
            pixel[i] = red;
        }
    }
    
    // 刷新屏幕
    printf("  刷新屏幕...\n");
    ioctl(fd, FBIOBLANK, 0);
    
    printf("✅ 红色屏幕绘制完成，等待3秒...\n");
    sleep(3);
    
    // 清屏为绿色
    printf("  绘制绿色全屏...\n");
    if (vinfo.bits_per_pixel == 32) {
        unsigned int* pixel = (unsigned int*)framebuffer;
        unsigned int green = 0x0000FF00;
        for (size_t i = 0; i < size / 4; i++) {
            pixel[i] = green;
        }
    } else if (vinfo.bits_per_pixel == 16) {
        unsigned short* pixel = (unsigned short*)framebuffer;
        unsigned short green = 0x07E0; // RGB565
        for (size_t i = 0; i < size / 2; i++) {
            pixel[i] = green;
        }
    }
    
    ioctl(fd, FBIOBLANK, 0);
    printf("✅ 绿色屏幕绘制完成，等待3秒...\n");
    sleep(3);
    
    // 清屏为蓝色
    printf("  绘制蓝色全屏...\n");
    if (vinfo.bits_per_pixel == 32) {
        unsigned int* pixel = (unsigned int*)framebuffer;
        unsigned int blue = 0x000000FF;
        for (size_t i = 0; i < size / 4; i++) {
            pixel[i] = blue;
        }
    } else if (vinfo.bits_per_pixel == 16) {
        unsigned short* pixel = (unsigned short*)framebuffer;
        unsigned short blue = 0x001F; // RGB565
        for (size_t i = 0; i < size / 2; i++) {
            pixel[i] = blue;
        }
    }
    
    ioctl(fd, FBIOBLANK, 0);
    printf("✅ 蓝色屏幕绘制完成，等待3秒...\n");
    sleep(3);
    
    // 清屏为黑色
    printf("  清屏为黑色...\n");
    memset(framebuffer, 0, size);
    ioctl(fd, FBIOBLANK, 0);
    
    // 清理
    munmap(framebuffer, size);
    close(fd);
    
    printf("✅ 帧缓冲测试完成\n");
    return 0;
}
