# Linux BLE GATT Server 项目

本项目演示了如何在Linux系统上使用GDBus和C++语言实现一个蓝牙低功耗(BLE) GATT服务器。

## 项目概述

这个GATT服务器实现了以下功能：
- 蓝牙低功耗(BLE) GATT服务器
- 电池服务(Battery Service, UUID: 0x180F)
- 电池电量特征值(Battery Level, UUID: 0x2A19)
- 蓝牙广播和设备发现
- 完整的D-Bus接口实现

## 技术栈

- **编程语言**: C++
- **蓝牙协议栈**: BlueZ (Linux官方蓝牙协议栈)
- **D-Bus库**: GDBus (GLib的一部分)
- **构建系统**: CMake
- **依赖库**: GLib 2.0, GIO 2.0, GIO-Unix 2.0

## 项目结构

```
bluetooth/
├── CMakeLists.txt              # CMake构建配置
├── README.md                   # 项目说明文档
├── include/                    # C++头文件
│   ├── bluez_interface.h       # BlueZ接口管理
│   ├── gatt_application.h      # GATT应用基类
│   ├── gatt_service.h          # GATT服务类
│   ├── gatt_characteristic.h   # GATT特征值类
│   └── advertisement_manager.h # 广告管理器
├── src/                        # 源代码文件
│   ├── main.cpp                # 完整版主程序
│   ├── bluez_interface.cpp     # BlueZ接口实现
│   ├── gatt_application.cpp    # GATT应用实现
│   ├── gatt_service.cpp        # GATT服务实现
│   ├── gatt_characteristic.cpp # GATT特征值实现
│   ├── advertisement_manager.cpp # 广告管理器实现
│   ├── bluetooth_server_simple.cpp # 简化版服务器
│   └── bluetooth_minimal.cpp   # 最小化可运行版本
└── build/                      # 构建输出目录
    └── bluetooth_gatt_server_minimal # 可执行文件
```

## 核心概念解析

### 1. BlueZ D-Bus架构

BlueZ使用D-Bus作为主要的IPC机制。关键接口包括：

- **org.bluez.GattManager1**: 管理GATT应用的注册和注销
- **org.bluez.GattApplication1**: GATT应用接口
- **org.bluez.GattService1**: GATT服务接口
- **org.bluez.GattCharacteristic1**: GATT特征值接口
- **org.bluez.LEAdvertisingManager1**: 管理LE广告的注册和注销
- **org.bluez.LEAdvertisement1**: LE广告接口

### 2. GATT层次结构

```
GATT Application
├── Service 1 (Battery Service, 0x180F)
│   └── Characteristic 1 (Battery Level, 0x2A19)
│       ├── Properties: Read, Notify
│       └── Value: 0-100 (电池电量百分比)
└── Service 2 (Custom Service)
    └── Characteristic 1 (Counter)
        ├── Properties: Read, Write, Notify
        └── Value: 32位计数器
```

### 3. D-Bus对象路径

所有GATT对象都需要在D-Bus上注册，路径结构如下：
- 应用: `/org/bluez/example/gatt`
- 服务: `/org/bluez/example/gatt/service0`
- 特征值: `/org/bluez/example/gatt/service0/char0`
- 广告: `/org/bluez/example/advertisement`

## 编译和运行

### 环境要求

- Ubuntu 20.04+ 或其他Linux发行版
- BlueZ蓝牙协议栈
- GLib开发库
- CMake 3.16+

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake
sudo apt install libglib2.0-dev libgio2.0-dev pkg-config

# 确保蓝牙服务运行
sudo systemctl status bluetooth
```

### 编译项目

```bash
# 克隆或下载项目到本地
cd bluetooth

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make bluetooth_gatt_server_minimal

# 或者使用便捷命令
make run_minimal
```

### 运行服务器

```bash
# 直接运行
./bluetooth_gatt_server_minimal

# 或者使用make命令
make run_minimal
```

**注意**: 运行程序需要适当的D-Bus权限。建议在用户模式下运行，或配置D-Bus权限。

## 代码详解

### 1. 主要类和接口

#### BluezInterface类
负责与BlueZ D-Bus服务通信，管理适配器状态和GATT应用注册。

关键方法：
- `initialize()`: 初始化D-Bus连接
- `registerApplication()`: 注册GATT应用到BlueZ
- `powerOnAdapter()`: 启用蓝牙适配器

#### GattApplication类
实现GATT应用接口，管理服务集合。

关键方法：
- `addService()`: 添加GATT服务
- `exportInterface()`: 导出D-Bus接口
- `handleGetServices()`: 处理服务获取请求

#### GattService类
实现GATT服务接口，管理特征值集合。

关键方法：
- `addCharacteristic()`: 添加特征值
- `exportInterface()`: 导出D-Bus接口
- `getCharacteristicList()`: 获取特征值列表

#### GattCharacteristic类
实现GATT特征值接口，处理读写操作。

关键方法：
- `handleReadValue()`: 处理读操作
- `handleWriteValue()`: 处理写操作
- `handleStartNotify()`: 处理通知开始
- `handleStopNotify()`: 处理通知停止

#### AdvertisementManager类
管理蓝牙LE广告，控制设备发现。

关键方法：
- `setDeviceName()`: 设置设备名称
- `setServiceUUIDs()`: 设置广播服务UUID
- `exportInterface()`: 导出广告接口

### 2. D-Bus接口注册

所有接口都通过`g_dbus_connection_register_object()`注册：

```cpp
GDBusInterfaceVTable vtable = {
    method_call_handler,    // 方法调用处理
    nullptr,               // 方法调用完成回调
    get_property_handler,   // 属性获取处理
    set_property_handler    // 属性设置处理
};

guint reg_id = g_dbus_connection_register_object(
    connection,            // D-Bus连接
    object_path,           // 对象路径
    interface_info,        // 接口信息
    &vtable,              // vtable
    user_data,            // 用户数据
    nullptr,              // 销毁回调
    &error                // 错误信息
);
```

### 3. 方法调用处理

通过`method_call_handler()`处理所有D-Bus方法调用：

```cpp
void method_call_handler(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* method_name,
                        GVariant* parameters,
                        GDBusMethodInvocation* invocation,
                        gpointer user_data) {

    // 根据接口和方法名称分发处理
    if (g_strcmp0(interface_name, "org.bluez.GattCharacteristic1") == 0) {
        if (g_strcmp0(method_name, "ReadValue") == 0) {
            // 处理读操作
            GVariant* value = get_characteristic_value();
            g_dbus_method_invocation_return_value(invocation, value);
        }
        // ... 其他方法处理
    }
}
```

## 测试和调试

### 1. 蓝牙客户端测试

使用蓝牙调试工具测试GATT服务器：

```bash
# 使用bluetoothctl
sudo bluetoothctl
[bluetooth]# scan on
[bluetooth]# connect XX:XX:XX:XX:XX:XX
[bluetooth]# menu gatt
[bluetooth]# list-attributes
[bluetooth]# attribute-info <handle>
[bluetooth]# read <handle>
[bluetooth]# write <handle> <value>
```

### 2. D-Bus调试

使用D-Bus调试工具监控通信：

```bash
# 监控BlueZ D-Bus通信
gdbus monitor --system --dest org.bluez

# 查看注册的对象
gdbus introspect --system --dest org.bluez --object-path /
```

### 3. 日志调试

程序运行时会输出详细日志：

```
=== Minimal BLE GATT Server ===
D-Bus connection established
Registered: /org/bluez/example/gatt (org.bluez.GattApplication1)
Registered: /org/bluez/example/gatt/service0 (org.bluez.GattService1)
Registered: /org/bluez/example/gatt/service0/char0 (org.bluez.GattCharacteristic1)
Registered: /org/bluez/example/advertisement (org.bluez.LEAdvertisement1)
Bluetooth adapter powered on
GATT application registered
Advertisement registered

=== Server Started ===
Battery Service (0x180F)
Battery Level (0x2A19) - Read/Notify
Device Name: BLE Minimal Server
```

## 故障排除

### 1. 权限问题

如果遇到权限错误，尝试：

```bash
# 添加用户到蓝牙组
sudo usermod -a -G bluetooth $USER

# 重启蓝牙服务
sudo systemctl restart bluetooth

# 使用sudo运行（不推荐，仅用于测试）
sudo ./bluetooth_gatt_server_minimal
```

### 2. D-Bus连接问题

检查D-Bus会话：

```bash
# 检查系统D-Bus
echo $DBUS_SYSTEM_BUS_ADDRESS

# 检查BlueZ服务
systemctl --user status bluetooth
```

### 3. 蓝牙适配器问题

检查蓝牙适配器状态：

```bash
# 检查蓝牙设备
hciconfig

# 检查适配器状态
bluetoothctl
[bluetooth]# list
[bluetooth]# show hci0
```

## 扩展开发

### 1. 添加新的服务

1. 在主函数中创建新的服务对象
2. 注册新的D-Bus接口
3. 添加相应的处理逻辑

### 2. 自定义特征值

1. 定义特征值UUID和属性
2. 实现读写回调函数
3. 处理数据格式转换

### 3. 通知机制

实现完整的notify/indicate机制：
- 维护订阅设备列表
- 发送属性更改通知
- 处理订阅和取消订阅

## 参考资料

- [BlueZ官方文档](https://www.bluez.org/)
- [GDBus编程指南](https://developer.gnome.org/gio/stable/gdbus.html)
- [蓝牙核心规范](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
- [GATT服务规范](https://www.bluetooth.com/specifications/gatt/)

## 许可证

本项目仅供学习和研究使用。