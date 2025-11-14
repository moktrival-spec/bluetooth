#include "bluez_interface.h"
#include "gatt_application.h"
#include "gatt_service.h"
#include "gatt_characteristic.h"
#include "advertisement_manager.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

// 全局变量用于优雅退出
static GMainLoop* main_loop = nullptr;
static Bluetooth::BluezInterface* bluez_interface = nullptr;
static std::shared_ptr<Bluetooth::GattCharacteristic> battery_characteristic;
static std::shared_ptr<Bluetooth::GattCharacteristic> counter_characteristic;

// 信号处理函数
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (main_loop) {
        g_main_loop_quit(main_loop);
    }
}

// 电池电量特征值读取回调
std::vector<uint8_t> readBatteryLevel(const std::string& device_path) {
    // 模拟电池电量（实际应用中可以从系统获取）
    static uint8_t battery_level = 85;
    battery_level = (battery_level % 100) + 1; // 循环变化

    std::cout << "Battery level requested: " << (int)battery_level << "%" << std::endl;
    return {battery_level};
}

// 电池电量写入回调
bool writeBatteryLevel(const std::string& device_path, const std::vector<uint8_t>& value) {
    if (!value.empty()) {
        std::cout << "Battery level set to: " << (int)value[0] << "%" << std::endl;
        return true;
    }
    return false;
}

// 电池通知回调
void batteryNotifyCallback(const std::string& device_path, bool subscribing) {
    if (subscribing) {
        std::cout << "Device " << device_path << " subscribed to battery notifications" << std::endl;
    } else {
        std::cout << "Device " << device_path << " unsubscribed from battery notifications" << std::endl;
    }
}

// 计数器特征值读取回调
std::vector<uint8_t> readCounter(const std::string& device_path) {
    static uint32_t counter = 0;
    counter++;

    std::cout << "Counter value requested: " << counter << std::endl;

    // 将32位计数器转换为字节数组
    std::vector<uint8_t> result(4);
    result[0] = counter & 0xFF;
    result[1] = (counter >> 8) & 0xFF;
    result[2] = (counter >> 16) & 0xFF;
    result[3] = (counter >> 24) & 0xFF;

    return result;
}

// 计数器写入回调
bool writeCounter(const std::string& device_path, const std::vector<uint8_t>& value) {
    if (value.size() >= 4) {
        uint32_t counter = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);
        std::cout << "Counter set to: " << counter << std::endl;
        return true;
    }
    return false;
}

// 定时器回调：更新电池电量
gboolean updateBatteryLevel(gpointer user_data) {
    static uint8_t battery_level = 85;
    battery_level = (battery_level % 100) + 1;

    if (battery_characteristic) {
        battery_characteristic->setValue({battery_level});
        std::cout << "Battery level updated: " << (int)battery_level << "%" << std::endl;
    }

    return TRUE; // 继续定时器
}

// 定时器回调：更新计数器
gboolean updateCounter(gpointer user_data) {
    static uint32_t counter = 0;
    counter++;

    if (counter_characteristic) {
        // 将32位计数器转换为字节数组
        std::vector<uint8_t> counter_bytes(4);
        counter_bytes[0] = counter & 0xFF;
        counter_bytes[1] = (counter >> 8) & 0xFF;
        counter_bytes[2] = (counter >> 16) & 0xFF;
        counter_bytes[3] = (counter >> 24) & 0xFF;

        counter_characteristic->setValue(counter_bytes);
        std::cout << "Counter updated: " << counter << std::endl;
    }

    return TRUE; // 继续定时器
}

int main(int argc, char* argv[]) {
    std::cout << "=== Bluetooth GATT Server Demo ===" << std::endl;
    std::cout << "Starting BLE GATT Server with Battery Service..." << std::endl;

    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 初始化GLib
    g_main_loop_new(nullptr, FALSE);
    main_loop = g_main_loop_new(nullptr, FALSE);

    try {
        // 创建BlueZ接口
        bluez_interface = new Bluetooth::BluezInterface();

        // 初始化BlueZ接口
        if (!bluez_interface->initialize()) {
            std::cerr << "Failed to initialize BlueZ interface" << std::endl;
            return 1;
        }

        // 检查BlueZ是否可用
        if (!bluez_interface->isBluezAvailable()) {
            std::cerr << "BlueZ service is not available" << std::endl;
            return 1;
        }

        // 启用蓝牙适配器
        if (!bluez_interface->powerOnAdapter()) {
            std::cerr << "Failed to power on Bluetooth adapter" << std::endl;
            return 1;
        }

        // 创建GATT应用
        auto app = std::make_shared<Bluetooth::GattApplication>("/org/bluez/example/gatt");

        // 创建电池服务
        auto battery_service = std::make_shared<Bluetooth::GattService>(
            "0000180f-0000-1000-8000-00805f9b34fb", // 电池服务UUID
            true
        );

        // 创建电池电量特征值
        battery_characteristic = std::make_shared<Bluetooth::GattCharacteristic>(
            "00002a19-0000-1000-8000-00805f9b34fb", // 电池电量特征值UUID
            std::vector<Bluetooth::CharacteristicFlags>{
                Bluetooth::CharacteristicFlags::READ,
                Bluetooth::CharacteristicFlags::NOTIFY
            }
        );

        // 设置回调函数
        battery_characteristic->setReadCallback(readBatteryLevel);
        battery_characteristic->setWriteCallback(writeBatteryLevel);
        battery_characteristic->setNotifyCallback(batteryNotifyCallback);

        // 设置初始值
        battery_characteristic->setValue({85});

        // 创建自定义服务（计数器服务）
        auto counter_service = std::make_shared<Bluetooth::GattService>(
            "12345678-1234-1234-1234-123456789abc", // 自定义服务UUID
            true
        );

        // 创建计数器特征值
        counter_characteristic = std::make_shared<Bluetooth::GattCharacteristic>(
            "12345678-1234-1234-1234-123456789abd", // 自定义特征值UUID
            std::vector<Bluetooth::CharacteristicFlags>{
                Bluetooth::CharacteristicFlags::READ,
                Bluetooth::CharacteristicFlags::WRITE,
                Bluetooth::CharacteristicFlags::NOTIFY
            }
        );

        // 设置回调函数
        counter_characteristic->setReadCallback(readCounter);
        counter_characteristic->setWriteCallback(writeCounter);

        // 设置初始值
        counter_characteristic->setValue({0, 0, 0, 1});

        // 将特征值添加到服务
        battery_service->addCharacteristic(battery_characteristic);
        counter_service->addCharacteristic(counter_characteristic);

        // 将服务添加到应用
        app->addService(battery_service);
        app->addService(counter_service);

        // 导出GATT应用
        if (!app->exportInterface(bluez_interface->getConnection())) {
            std::cerr << "Failed to export GATT application" << std::endl;
            return 1;
        }

        // 注册GATT应用
        if (!bluez_interface->registerApplication(app.get())) {
            std::cerr << "Failed to register GATT application" << std::endl;
            return 1;
        }

        // 创建广告
        auto advertisement = std::make_shared<Bluetooth::AdvertisementManager>(
            "/org/bluez/example/advertisement",
            Bluetooth::AdvertisementType::PERIPHERAL
        );

        // 设置广告属性
        advertisement->setDeviceName("BLE GATT Server Demo");
        advertisement->setServiceUUIDs({
            "0000180f-0000-1000-8000-00805f9b34fb", // 电池服务
            "12345678-1234-1234-1234-123456789abc"  // 自定义服务
        });
        advertisement->setTransportSettings(true, true);

        // 设置制造商数据（示例：公司ID 0x05F1 = 蓝牙SIG）
        std::vector<uint8_t> manufacturer_data = {0x01, 0x02, 0x03, 0x04};
        advertisement->setManufacturerData(0x05F1, manufacturer_data);

        // 导出广告
        if (!advertisement->exportInterface(bluez_interface->getConnection())) {
            std::cerr << "Failed to export advertisement" << std::endl;
            return 1;
        }

        // 注册广告
        Bluetooth::AdvertisementRegistrar ad_registrar;
        if (!ad_registrar.registerAdvertisement(bluez_interface->getConnection(), advertisement.get())) {
            std::cerr << "Failed to register advertisement" << std::endl;
            return 1;
        }

        std::cout << "=== GATT Server Setup Complete ===" << std::endl;
        std::cout << "Services:" << std::endl;
        std::cout << "  - Battery Service (0x180F)" << std::endl;
        std::cout << "    - Battery Level (0x2A19) - Read/Notify" << std::endl;
        std::cout << "  - Custom Service" << std::endl;
        std::cout << "    - Counter (Custom UUID) - Read/Write/Notify" << std::endl;
        std::cout << std::endl;
        std::cout << "Advertisement:" << std::endl;
        std::cout << "  - Device Name: BLE GATT Server Demo" << std::endl;
        std::cout << "  - Type: Connectable" << std::endl;
        std::cout << "  - Services: Battery Service + Custom Service" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Ctrl+C to stop the server..." << std::endl;

        // 启动定时器更新特征值
        g_timeout_add_seconds(10, updateBatteryLevel, nullptr);
        g_timeout_add_seconds(5, updateCounter, nullptr);

        // 运行主循环
        g_main_loop_run(main_loop);

        std::cout << "Shutting down GATT Server..." << std::endl;

        // 清理
        g_main_loop_unref(main_loop);
        delete bluez_interface;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "GATT Server stopped" << std::endl;
    return 0;
}