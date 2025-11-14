#!/bin/bash

echo "=== 蓝牙适配器启动脚本 ==="
echo

# 检查用户权限
if [ "$(id -u)" -eq 0 ]; then
    echo "⚠️  检测到root权限，建议使用普通用户运行"
fi

# 1. 检查蓝牙服务状态
echo "1. 检查蓝牙服务..."
if systemctl is-active --quiet bluetooth; then
    echo "✅ 蓝牙服务正在运行"
else
    echo "❌ 蓝牙服务未运行，正在启动..."
    if command -v sudo &> /dev/null; then
        sudo systemctl start bluetooth
    else
        echo "请手动启动蓝牙服务: systemctl start bluetooth"
        exit 1
    fi
fi

# 2. 检查rfkill状态
echo
echo "2. 检查rfkill状态..."
RFKILL_BLOCKED=$(rfkill list bluetooth | grep -c "Soft blocked: yes")
if [ "$RFKILL_BLOCKED" -gt 0 ]; then
    echo "⚠️  蓝牙被rfkill阻塞，正在解除阻塞..."
    if command -v sudo &> /dev/null; then
        sudo rfkill unblock bluetooth
    else
        rfkill unblock bluetooth 2>/dev/null || echo "需要权限解除rfkill阻塞"
    fi
else
    echo "✅ 蓝牙未被rfkill阻塞"
fi

# 3. 启动蓝牙适配器
echo
echo "3. 启动蓝牙适配器..."
echo -e "power on\nexit" | bluetoothctl > /dev/null 2>&1
sleep 1

# 4. 验证启动状态
echo
echo "4. 验证启动状态..."
POWERED_STATUS=$(bluetoothctl show | grep -c "Powered: yes")
if [ "$POWERED_STATUS" -gt 0 ]; then
    echo "✅ 蓝牙适配器启动成功！"

    # 显示详细信息
    echo
    echo "=== 蓝牙适配器信息 ==="
    bluetoothctl show | grep -E "(Controller|Powered|Name|Discoverable|Pairable)"

    echo
    echo "=== 可用命令 ==="
    echo "• 扫描设备:     bluetoothctl scan on"
    echo "• 设置可发现:   bluetoothctl discoverable on"
    echo "• 设置可配对:   bluetoothctl pairable on"
    echo "• 列出设备:     bluetoothctl devices"
    echo "• 查看帮助:     bluetoothctl help"

else
    echo "❌ 蓝牙适配器启动失败"
    echo
    echo "故障排除建议："
    echo "1. 检查蓝牙硬件: lsusb | grep -i bluetooth"
    echo "2. 检查内核模块: lsmod | grep bluetooth"
    echo "3. 重启蓝牙服务: sudo systemctl restart bluetooth"
    echo "4. 检查系统日志: journalctl -u bluetooth -f"
    exit 1
fi

echo
echo "🎉 蓝牙适配器启动完成！"