#!/bin/bash

# RG34XX SDL2应用安装脚本
# 支持多语言显示和按键监听

set -e

echo "🚀 RG34XX SDL2应用安装脚本"
echo "========================="

# 检查必要文件
if [ ! -f "rg34xx-sdl2-arm" ]; then
    echo "❌ 找不到ARM版本可执行文件"
    exit 1
fi

if [ ! -f "NotoSansCJK-Regular.ttc" ]; then
    echo "❌ 找不到字体文件"
    exit 1
fi

# 设备配置
DEFAULT_DEVICE_IP="192.168.66.194"
DEFAULT_SSH_PASSWORD="root"

DEVICE_IP=${1:-$DEFAULT_DEVICE_IP}
SSH_PASSWORD=${2:-$DEFAULT_SSH_PASSWORD}

echo "📱 目标设备: $DEVICE_IP"
echo "🔑 SSH密码: $SSH_PASSWORD"
echo ""

# 检查网络连接
echo "🔍 检查设备连接..."
if ! ping -c 1 "$DEVICE_IP" &> /dev/null; then
    echo "❌ 无法连接到设备 $DEVICE_IP"
    echo "请检查设备是否开机和网络连接"
    exit 1
fi

echo "✅ 设备连接正常"
echo ""

# 创建应用目录
echo "📁 创建应用目录..."
if command -v sshpass &> /dev/null && ! ssh -o BatchMode=yes root@"$DEVICE_IP" "echo 'test'" &> /dev/null; then
    echo "使用密码认证创建目录..."
    sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no root@"$DEVICE_IP" "mkdir -p /mnt/mmc/Roms/APPS"
else
    echo "使用密钥认证创建目录..."
    ssh root@"$DEVICE_IP" "mkdir -p /mnt/mmc/Roms/APPS"
fi

# 复制文件
echo "📦 复制应用文件..."
if command -v sshpass &> /dev/null && ! ssh -o BatchMode=yes root@"$DEVICE_IP" "echo 'test'" &> /dev/null; then
    echo "使用密码认证复制文件..."
    sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no rg34xx-sdl2-arm root@"$DEVICE_IP":/mnt/mmc/Roms/APPS/
    sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no NotoSansCJK-Regular.ttc root@"$DEVICE_IP":/mnt/mmc/Roms/APPS/
else
    echo "使用密钥认证复制文件..."
    scp rg34xx-sdl2-arm root@"$DEVICE_IP":/mnt/mmc/Roms/APPS/
    scp NotoSansCJK-Regular.ttc root@"$DEVICE_IP":/mnt/mmc/Roms/APPS/
fi

# 设置权限
echo "🔧 设置文件权限..."
if command -v sshpass &> /dev/null && ! ssh -o BatchMode=yes root@"$DEVICE_IP" "echo 'test'" &> /dev/null; then
    echo "使用密码认证设置权限..."
    sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no root@"$DEVICE_IP" "chmod +x /mnt/mmc/Roms/APPS/rg34xx-sdl2-arm"
    sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no root@"$DEVICE_IP" "chmod 644 /mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc"
else
    echo "使用密钥认证设置权限..."
    ssh root@"$DEVICE_IP" "chmod +x /mnt/mmc/Roms/APPS/rg34xx-sdl2-arm"
    ssh root@"$DEVICE_IP" "chmod 644 /mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc"
fi

# 显示文件信息
echo ""
echo "📋 安装完成，文件信息:"
if command -v sshpass &> /dev/null && ! ssh -o BatchMode=yes root@"$DEVICE_IP" "echo 'test'" &> /dev/null; then
    sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no root@"$DEVICE_IP" "ls -lh /mnt/mmc/Roms/APPS/rg34xx-sdl2-arm"
    sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no root@"$DEVICE_IP" "ls -lh /mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc"
else
    ssh root@"$DEVICE_IP" "ls -lh /mnt/mmc/Roms/APPS/rg34xx-sdl2-arm"
    ssh root@"$DEVICE_IP" "ls -lh /mnt/mmc/Roms/APPS/NotoSansCJK-Regular.ttc"
fi

# 运行说明
echo ""
echo "🎮 运行应用:"
echo "ssh root@$DEVICE_IP 'cd /mnt/mmc/Roms/APPS && ./rg34xx-sdl2-arm'"
echo ""
echo "✅ 安装完成！"
echo ""
echo "📝 功能说明:"
echo "- 支持中文、日文、韩文显示"
echo "- 支持手柄按键和方向键"
echo "- 支持键盘和鼠标输入"
echo "- 包含动画演示"
echo "- ESC键退出程序"
