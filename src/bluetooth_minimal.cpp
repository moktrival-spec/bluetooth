#include <gio/gio.h>
#include <glib-2.0/glib.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <cstdint>

// 蓝牙常量
constexpr const char* BLUEZ_SERVICE = "org.bluez";
constexpr const char* ADAPTER_PATH = "/org/bluez/hci0";
constexpr const char* APP_PATH = "/org/bluez/example/gatt";
constexpr const char* ADVERTISEMENT_PATH = "/org/bluez/example/advertisement";

// 全局变量
static GMainLoop* main_loop = nullptr;
static GDBusConnection* connection = nullptr;

// 信号处理
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (main_loop) {
        g_main_loop_quit(main_loop);
    }
}

// 方法调用处理
void method_call_handler(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* method_name,
                        GVariant* parameters,
                        GDBusMethodInvocation* invocation,
                        gpointer user_data) {

    std::cout << "Method: " << method_name << " on " << interface_name << std::endl;

    if (g_strcmp0(method_name, "GetServices") == 0) {
        // 返回服务路径
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
        g_variant_builder_add(builder, "o", "/org/bluez/example/gatt/service0");
        GVariant* result = g_variant_new("(ao)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, result);
        return;
    }

    if (g_strcmp0(method_name, "ReadValue") == 0) {
        // 返回模拟数据
        uint8_t data[] = {85}; // 模拟电池电量
        GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data, 1, sizeof(uint8_t));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(ay)", value));
        return;
    }

    if (g_strcmp0(method_name, "WriteValue") == 0) {
        std::cout << "Write operation received" << std::endl;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return;
    }

    if (g_strcmp0(method_name, "StartNotify") == 0) {
        std::cout << "Notifications started" << std::endl;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return;
    }

    if (g_strcmp0(method_name, "StopNotify") == 0) {
        std::cout << "Notifications stopped" << std::endl;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return;
    }

    if (g_strcmp0(method_name, "Release") == 0) {
        std::cout << "Advertisement released" << std::endl;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return;
    }

    // 默认返回
    g_dbus_method_invocation_return_error(invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

// 属性获取处理
gboolean get_property_handler(GDBusConnection* conn,
                             const gchar* sender,
                             const gchar* object_path,
                             const gchar* interface_name,
                             const gchar* property_name,
                             GVariant** value,
                             GError** error,
                             gpointer user_data) {

    if (g_strcmp0(interface_name, "org.bluez.GattService1") == 0) {
        if (g_strcmp0(property_name, "UUID") == 0) {
            *value = g_variant_new_string("0000180f-0000-1000-8000-00805f9b34fb"); // 电池服务
            return TRUE;
        }
        if (g_strcmp0(property_name, "Primary") == 0) {
            *value = g_variant_new_boolean(TRUE);
            return TRUE;
        }
    }

    if (g_strcmp0(interface_name, "org.bluez.GattCharacteristic1") == 0) {
        if (g_strcmp0(property_name, "UUID") == 0) {
            *value = g_variant_new_string("00002a19-0000-1000-8000-00805f9b34fb"); // 电池电量
            return TRUE;
        }
        if (g_strcmp0(property_name, "Flags") == 0) {
            const char* flags[] = {"read", "notify", nullptr};
            *value = g_variant_new_strv(flags, -1);
            return TRUE;
        }
        if (g_strcmp0(property_name, "Notifying") == 0) {
            *value = g_variant_new_boolean(FALSE);
            return TRUE;
        }
    }

    if (g_strcmp0(interface_name, "org.bluez.LEAdvertisement1") == 0) {
        if (g_strcmp0(property_name, "Type") == 0) {
            *value = g_variant_new_string("peripheral");
            return TRUE;
        }
        if (g_strcmp0(property_name, "ServiceUUIDs") == 0) {
            const char* uuids[] = {"0000180f-0000-1000-8000-00805f9b34fb", nullptr};
            *value = g_variant_new_strv(uuids, -1);
            return TRUE;
        }
        if (g_strcmp0(property_name, "LocalName") == 0) {
            *value = g_variant_new_string("BLE Minimal Server");
            return TRUE;
        }
    }

    return FALSE;
}

// 属性设置处理
gboolean set_property_handler(GDBusConnection* conn,
                             const gchar* sender,
                             const gchar* object_path,
                             const gchar* interface_name,
                             const gchar* property_name,
                             GVariant* value,
                             GError** error,
                             gpointer user_data) {
    return FALSE; // 不支持设置属性
}

// D-Bus接口vtable
const GDBusInterfaceVTable interface_vtable = {
    method_call_handler,
    nullptr,
    nullptr,
    nullptr
};

// 注册所有D-Bus接口
gboolean register_interfaces() {
    GError* error = nullptr;

    // 简化的接口XML定义
    const char* interfaces_xml[] = {
        // GATT应用接口
        "<node><interface name='org.bluez.GattApplication1'>"
        "<method name='GetServices'><arg type='ao' name='services' direction='out'/></method>"
        "</interface></node>",

        // GATT服务接口
        "<node><interface name='org.bluez.GattService1'>"
        "<property name='UUID' type='s' access='read'/>"
        "<property name='Primary' type='b' access='read'/>"
        "</interface></node>",

        // GATT特征值接口
        "<node><interface name='org.bluez.GattCharacteristic1'>"
        "<property name='UUID' type='s' access='read'/>"
        "<property name='Flags' type='as' access='read'/>"
        "<property name='Notifying' type='b' access='read'/>"
        "<method name='ReadValue'><arg type='a{sv}' direction='in'/><arg type='ay' direction='out'/></method>"
        "<method name='WriteValue'><arg type='ay' direction='in'/><arg type='a{sv}' direction='in'/></method>"
        "<method name='StartNotify'/><method name='StopNotify'/>"
        "</interface></node>",

        // 广告接口
        "<node><interface name='org.bluez.LEAdvertisement1'>"
        "<property name='Type' type='s' access='read'/>"
        "<property name='ServiceUUIDs' type='as' access='read'/>"
        "<property name='LocalName' type='s' access='read'/>"
        "<method name='Release'/>"
        "</interface></node>"
    };

    // 对象路径和接口名称
    const char* object_paths[] = {
        "/org/bluez/example/gatt",
        "/org/bluez/example/gatt/service0",
        "/org/bluez/example/gatt/service0/char0",
        "/org/bluez/example/advertisement"
    };

    const char* interface_names[] = {
        "org.bluez.GattApplication1",
        "org.bluez.GattService1",
        "org.bluez.GattCharacteristic1",
        "org.bluez.LEAdvertisement1"
    };

    // 注册所有接口
    for (int i = 0; i < 4; i++) {
        GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(interfaces_xml[i], &error);
        if (!info) {
            std::cerr << "Failed to parse XML " << i << ": " << error->message << std::endl;
            g_error_free(error);
            return FALSE;
        }

        guint reg_id = g_dbus_connection_register_object(connection,
            object_paths[i], info->interfaces[0], &interface_vtable, nullptr, nullptr, &error);

        if (reg_id == 0) {
            std::cerr << "Failed to register object " << object_paths[i] << ": " << error->message << std::endl;
            g_error_free(error);
            g_dbus_node_info_unref(info);
            return FALSE;
        }

        g_dbus_node_info_unref(info);
        std::cout << "Registered: " << object_paths[i] << " (" << interface_names[i] << ")" << std::endl;
    }

    return TRUE;
}

// 注册GATT应用
gboolean register_gatt_application() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.bluez.GattManager1",
        "RegisterApplication",
        g_variant_new("(oa{sv})", APP_PATH, nullptr),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (!result) {
        std::cerr << "Failed to register GATT application: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    g_variant_unref(result);
    std::cout << "GATT application registered" << std::endl;
    return TRUE;
}

// 注册广告
gboolean register_advertisement() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.bluez.LEAdvertisingManager1",
        "RegisterAdvertisement",
        g_variant_new("(oa{sv})", ADVERTISEMENT_PATH, nullptr),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (!result) {
        std::cerr << "Failed to register advertisement: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    g_variant_unref(result);
    std::cout << "Advertisement registered" << std::endl;
    return TRUE;
}

// 检查适配器是否存在
gboolean check_adapter_exists() {
    GError* error = nullptr;

    // 尝试获取适配器属性来检查是否存在
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.bluez.Adapter1", "Name"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (!result) {
        std::cerr << "Bluetooth adapter not found at " << ADAPTER_PATH << ": " << error->message << std::endl;
        std::cerr << "Please check if bluetooth adapter is available" << std::endl;
        g_error_free(error);
        return FALSE;
    }

    g_variant_unref(result);
    std::cout << "Bluetooth adapter found at: " << ADAPTER_PATH << std::endl;
    return TRUE;
}

// 启用适配器
gboolean power_on_adapter() {
    GError* error = nullptr;

    // 首先检查当前电源状态
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.bluez.Adapter1", "Powered"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (result) {
        GVariant* powered_variant = g_variant_get_child_value(result, 0);
        GVariant* powered_value = g_variant_get_variant(powered_variant);
        gboolean is_powered = g_variant_get_boolean(powered_value);
        g_variant_unref(powered_value);
        g_variant_unref(powered_variant);
        g_variant_unref(result);

        if (is_powered) {
            std::cout << "Bluetooth adapter already powered on" << std::endl;
            return TRUE;
        }
    }

    // 如果获取状态失败，继续尝试启动
    g_clear_error(&error);

    result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(TRUE)),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (!result) {
        std::cerr << "Failed to power on adapter: " << error->message << std::endl;
        std::cerr << "This might be due to:" << std::endl;
        std::cerr << "  1. Insufficient permissions (try running with sudo)" << std::endl;
        std::cerr << "  2. Bluetooth device is blocked" << std::endl;
        std::cerr << "  3. BlueZ service is not running properly" << std::endl;

        // 尝试提供一些调试信息
        std::cerr << "\nDebugging tips:" << std::endl;
        std::cerr << "  - Check: systemctl status bluetooth" << std::endl;
        std::cerr << "  - Try: sudo rfkill unblock bluetooth" << std::endl;
        std::cerr << "  - Try: sudo systemctl restart bluetooth" << std::endl;

        g_error_free(error);
        return FALSE;
    }

    g_variant_unref(result);

    // 等待一下让适配器完全启动
    g_usleep(500000); // 500ms
    std::cout << "Bluetooth adapter powered on successfully" << std::endl;
    return TRUE;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Minimal BLE GATT Server ===" << std::endl;

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 获取D-Bus连接
    GError* error = nullptr;
    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!connection) {
        std::cerr << "Failed to get D-Bus connection: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    std::cout << "D-Bus connection established" << std::endl;

    // 检查适配器是否存在
    if (!check_adapter_exists()) {
        std::cerr << "Failed to find Bluetooth adapter" << std::endl;
        return 1;
    }

    // 注册接口
    if (!register_interfaces()) {
        std::cerr << "Failed to register interfaces" << std::endl;
        return 1;
    }

    // 启用适配器
    if (!power_on_adapter()) {
        std::cerr << "Failed to power on adapter" << std::endl;
        std::cerr << "Continuing with adapter registration anyway..." << std::endl;
        // 不要直接返回，继续尝试注册应用
    }

    // 注册GATT应用
    if (!register_gatt_application()) {
        std::cerr << "Failed to register GATT application" << std::endl;
        return 1;
    }

    // 注册广告
    if (!register_advertisement()) {
        std::cerr << "Failed to register advertisement" << std::endl;
        return 1;
    }

    std::cout << "\n=== Server Started ===" << std::endl;
    std::cout << "Battery Service (0x180F)" << std::endl;
    std::cout << "Battery Level (0x2A19) - Read/Notify" << std::endl;
    std::cout << "Device Name: BLE Minimal Server" << std::endl;
    std::cout << "\nPress Ctrl+C to stop..." << std::endl;

    // 运行主循环
    main_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(main_loop);

    // 清理
    std::cout << "Shutting down..." << std::endl;
    if (connection) {
        g_object_unref(connection);
    }
    if (main_loop) {
        g_main_loop_unref(main_loop);
    }

    std::cout << "Server stopped" << std::endl;
    return 0;
}