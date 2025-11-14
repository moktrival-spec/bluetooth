#include <gio/gio.h>
#include <glib-2.0/glib.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <cstdint>

// 蓝牙常量定义
constexpr const char* BLUEZ_SERVICE = "org.bluez";
constexpr const char* ADAPTER_PATH = "/org/bluez/hci0";
constexpr const char* GATT_MANAGER_IFACE = "org.bluez.GattManager1";
constexpr const char* LE_ADVERTISING_MANAGER_IFACE = "org.bluez.LEAdvertisingManager1";
constexpr const char* GATT_APPLICATION_IFACE = "org.bluez.GattApplication1";
constexpr const char* GATT_SERVICE_IFACE = "org.bluez.GattService1";
constexpr const char* GATT_CHARACTERISTIC_IFACE = "org.bluez.GattCharacteristic1";
constexpr const char* ADVERTISEMENT_IFACE = "org.bluez.LEAdvertisement1";

// 应用对象路径
constexpr const char* APP_PATH = "/org/bluez/example/gatt";
constexpr const char* ADVERTISEMENT_PATH = "/org/bluez/example/advertisement";

// 全局变量
static GMainLoop* main_loop = nullptr;
static GDBusConnection* connection = nullptr;
static guint reg_id = 0;

// 特征值数据
static uint8_t battery_level = 85;
static uint32_t counter_value = 0;
static bool notifying = false;

// 信号处理
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (main_loop) {
        g_main_loop_quit(main_loop);
    }
}

// D-Bus方法调用回调
void on_method_call(GDBusConnection* conn,
                   const gchar* sender,
                   const gchar* object_path,
                   const gchar* interface_name,
                   const gchar* method_name,
                   GVariant* parameters,
                   GDBusMethodInvocation* invocation,
                   gpointer user_data) {

    std::cout << "Method call: " << method_name << " on " << object_path << std::endl;

    if (g_strcmp0(interface_name, GATT_APPLICATION_IFACE) == 0) {
        if (g_strcmp0(method_name, "GetServices") == 0) {
            // 返回服务列表
            const char* services[] = {
                "/org/bluez/example/gatt/service0",
                "/org/bluez/example/gatt/service1",
                nullptr
            };
            GVariant* result = g_variant_new_strv(services, -1);
            g_dbus_method_invocation_return_value(invocation, result);
            return;
        }
    } else if (g_strcmp0(interface_name, GATT_CHARACTERISTIC_IFACE) == 0) {
        if (g_strcmp0(method_name, "ReadValue") == 0) {
            // 读取特征值
            if (g_str_has_suffix(object_path, "char0")) {
                // 电池电量特征值
                battery_level = (battery_level % 100) + 1;
                std::cout << "Battery level read: " << (int)battery_level << "%" << std::endl;
                GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, &battery_level, 1, sizeof(uint8_t));
                g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&value, 1));
                return;
            } else if (g_str_has_suffix(object_path, "char1")) {
                // 计数器特征值
                counter_value++;
                std::cout << "Counter read: " << counter_value << std::endl;
                GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, &counter_value, 4, sizeof(uint32_t));
                g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&value, 1));
                return;
            }
        } else if (g_strcmp0(method_name, "WriteValue") == 0) {
            // 写入特征值
            GVariant* value = g_variant_get_child_value(parameters, 0);
            gsize n_elements;
            const uint8_t* data = (const uint8_t*)g_variant_get_fixed_array(value, &n_elements, sizeof(uint8_t));

            if (g_str_has_suffix(object_path, "char1") && n_elements >= 4) {
                counter_value = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
                std::cout << "Counter written: " << counter_value << std::endl;
            }

            g_variant_unref(value);
            g_dbus_method_invocation_return_value(invocation, nullptr);
            return;
        } else if (g_strcmp0(method_name, "StartNotify") == 0) {
            notifying = true;
            std::cout << "Notification started" << std::endl;
            g_dbus_method_invocation_return_value(invocation, nullptr);
            return;
        } else if (g_strcmp0(method_name, "StopNotify") == 0) {
            notifying = false;
            std::cout << "Notification stopped" << std::endl;
            g_dbus_method_invocation_return_value(invocation, nullptr);
            return;
        }
    } else if (g_strcmp0(interface_name, ADVERTISEMENT_IFACE) == 0) {
        if (g_strcmp0(method_name, "Release") == 0) {
            std::cout << "Advertisement released" << std::endl;
            g_dbus_method_invocation_return_value(invocation, nullptr);
            return;
        }
    }

    // 未知方法
    g_dbus_method_invocation_return_error(invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

// D-Bus属性获取回调
gboolean on_get_property(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* property_name,
                        GVariant** value,
                        GError** error,
                        gpointer user_data) {

    if (g_strcmp0(interface_name, GATT_SERVICE_IFACE) == 0) {
        if (g_strcmp0(property_name, "UUID") == 0) {
            if (g_str_has_suffix(object_path, "service0")) {
                *value = g_variant_new_string("0000180f-0000-1000-8000-00805f9b34fb"); // Battery Service
            } else if (g_str_has_suffix(object_path, "service1")) {
                *value = g_variant_new_string("12345678-1234-1234-1234-123456789abc"); // Custom Service
            }
            return TRUE;
        } else if (g_strcmp0(property_name, "Primary") == 0) {
            *value = g_variant_new_boolean(TRUE);
            return TRUE;
        } else if (g_strcmp0(property_name, "Characteristics") == 0) {
            const char* characteristics[] = { nullptr };
            if (g_str_has_suffix(object_path, "service0")) {
                characteristics[0] = "/org/bluez/example/gatt/service0/char0";
            } else if (g_str_has_suffix(object_path, "service1")) {
                characteristics[0] = "/org/bluez/example/gatt/service1/char1";
            }
            *value = g_variant_new_strv(characteristics, 1);
            return TRUE;
        }
    } else if (g_strcmp0(interface_name, GATT_CHARACTERISTIC_IFACE) == 0) {
        if (g_strcmp0(property_name, "UUID") == 0) {
            if (g_str_has_suffix(object_path, "char0")) {
                *value = g_variant_new_string("00002a19-0000-1000-8000-00805f9b34fb"); // Battery Level
            } else if (g_str_has_suffix(object_path, "char1")) {
                *value = g_variant_new_string("12345678-1234-1234-1234-123456789abd"); // Custom Characteristic
            }
            return TRUE;
        } else if (g_strcmp0(property_name, "Flags") == 0) {
            const char* flags[] = { "read", "notify", nullptr };
            if (g_str_has_suffix(object_path, "char1")) {
                flags[1] = "write";
                flags[2] = "notify";
                flags[3] = nullptr;
            }
            *value = g_variant_new_strv(flags, -1);
            return TRUE;
        } else if (g_strcmp0(property_name, "Notifying") == 0) {
            *value = g_variant_new_boolean(notifying);
            return TRUE;
        }
    } else if (g_strcmp0(interface_name, ADVERTISEMENT_IFACE) == 0) {
        if (g_strcmp0(property_name, "Type") == 0) {
            *value = g_variant_new_string("peripheral");
            return TRUE;
        } else if (g_strcmp0(property_name, "ServiceUUIDs") == 0) {
            const char* uuids[] = {
                "0000180f-0000-1000-8000-00805f9b34fb",
                "12345678-1234-1234-1234-123456789abc",
                nullptr
            };
            *value = g_variant_new_strv(uuids, -1);
            return TRUE;
        } else if (g_strcmp0(property_name, "LocalName") == 0) {
            *value = g_variant_new_string("BLE GATT Server Demo");
            return TRUE;
        }
    }

    return FALSE;
}

// D-Bus接口vtable
const GDBusInterfaceVTable app_vtable = {
    on_method_call,
    nullptr,
    nullptr,
};

// D-Bus属性设置回调
gboolean on_set_property(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* property_name,
                        GVariant* value,
                        GError** error,
                        gpointer user_data) {
    // 目前不支持属性设置
    return FALSE;
}

// 初始化D-Bus接口
gboolean register_interfaces() {
    GError* error = nullptr;

    // 创建应用接口XML
    const char* app_xml =
        "<node>"
        "  <interface name='org.bluez.GattApplication1'>"
        "    <method name='GetServices'>"
        "      <arg type='ao' name='services' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

    // 创建服务接口XML
    const char* service_xml =
        "<node>"
        "  <interface name='org.bluez.GattService1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Primary' type='b' access='read'/>"
        "    <property name='Characteristics' type='ao' access='read'/>"
        "  </interface>"
        "</node>";

    // 创建特征值接口XML
    const char* char_xml =
        "<node>"
        "  <interface name='org.bluez.GattCharacteristic1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Flags' type='as' access='read'/>"
        "    <property name='Notifying' type='b' access='read'/>"
        "    <method name='ReadValue'>"
        "      <arg type='a{sv}' name='options' direction='in'/>"
        "      <arg type='ay' name='value' direction='out'/>"
        "    </method>"
        "    <method name='WriteValue'>"
        "      <arg type='ay' name='value' direction='in'/>"
        "      <arg type='a{sv}' name='options' direction='in'/>"
        "    </method>"
        "    <method name='StartNotify'/>"
        "    <method name='StopNotify'/>"
        "  </interface>"
        "</node>";

    // 创建广告接口XML
    const char* ad_xml =
        "<node>"
        "  <interface name='org.bluez.LEAdvertisement1'>"
        "    <property name='Type' type='s' access='read'/>"
        "    <property name='ServiceUUIDs' type='as' access='read'/>"
        "    <property name='LocalName' type='s' access='read'/>"
        "    <method name='Release'/>"
        "  </interface>"
        "</node>";

    // 创建完整的vtable
    GDBusInterfaceVTable vtable = {
        on_method_call,
        nullptr,
        on_get_property,
        on_set_property,
    };

    // 注册应用接口
    GDBusNodeInfo* app_info = g_dbus_node_info_new_for_xml(app_xml, &error);
    if (!app_info) {
        std::cerr << "Failed to parse app XML: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    reg_id = g_dbus_connection_register_object(connection, APP_PATH,
        app_info->interfaces[0], &vtable, nullptr, nullptr, &error);

    if (reg_id == 0) {
        std::cerr << "Failed to register application interface: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(app_info);
        return FALSE;
    }

    // 注册服务接口
    GDBusNodeInfo* service_info = g_dbus_node_info_new_for_xml(service_xml, nullptr);
    g_dbus_connection_register_object(connection, "/org/bluez/example/gatt/service0",
        service_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);
    g_dbus_connection_register_object(connection, "/org/bluez/example/gatt/service1",
        service_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);

    // 注册特征值接口
    GDBusNodeInfo* char_info = g_dbus_node_info_new_for_xml(char_xml, nullptr);
    g_dbus_connection_register_object(connection, "/org/bluez/example/gatt/service0/char0",
        char_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);
    g_dbus_connection_register_object(connection, "/org/bluez/example/gatt/service1/char1",
        char_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);

    // 注册广告接口
    GDBusNodeInfo* ad_info = g_dbus_node_info_new_for_xml(ad_xml, nullptr);
    g_dbus_connection_register_object(connection, ADVERTISEMENT_PATH,
        ad_info->interfaces[0], &vtable, nullptr, nullptr, nullptr);

    // 清理
    g_dbus_node_info_unref(app_info);
    g_dbus_node_info_unref(service_info);
    g_dbus_node_info_unref(char_info);
    g_dbus_node_info_unref(ad_info);

    return TRUE;
}

// 注册GATT应用
gboolean register_gatt_application() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, GATT_MANAGER_IFACE,
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
    std::cout << "GATT application registered successfully" << std::endl;
    return TRUE;
}

// 注册广告
gboolean register_advertisement() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, LE_ADVERTISING_MANAGER_IFACE,
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
    std::cout << "Advertisement registered successfully" << std::endl;
    return TRUE;
}

// 启用蓝牙适配器
gboolean power_on_adapter() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, ADAPTER_PATH, "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(TRUE)),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (!result) {
        std::cerr << "Failed to power on adapter: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    g_variant_unref(result);
    std::cout << "Bluetooth adapter powered on" << std::endl;
    return TRUE;
}

// 定时器回调：更新电池电量
gboolean update_battery(gpointer user_data) {
    battery_level = (battery_level % 100) + 1;
    if (notifying) {
        std::cout << "Battery level updated: " << (int)battery_level << "%" << std::endl;
        // 这里可以发送属性更改通知
    }
    return TRUE;
}

// 定时器回调：更新计数器
gboolean update_counter(gpointer user_data) {
    counter_value++;
    if (notifying) {
        std::cout << "Counter updated: " << counter_value << std::endl;
        // 这里可以发送属性更改通知
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Simple Bluetooth GATT Server ===" << std::endl;
    std::cout << "Starting BLE GATT Server..." << std::endl;

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

    // 注册接口
    if (!register_interfaces()) {
        std::cerr << "Failed to register interfaces" << std::endl;
        return 1;
    }

    // 启用适配器
    if (!power_on_adapter()) {
        std::cerr << "Failed to power on adapter" << std::endl;
        return 1;
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

    std::cout << "\n=== GATT Server Setup Complete ===" << std::endl;
    std::cout << "Services:" << std::endl;
    std::cout << "  - Battery Service (0x180F)" << std::endl;
    std::cout << "    - Battery Level (0x2A19) - Read/Notify" << std::endl;
    std::cout << "  - Custom Service" << std::endl;
    std::cout << "    - Counter - Read/Write/Notify" << std::endl;
    std::cout << std::endl;
    std::cout << "Advertisement:" << std::endl;
    std::cout << "  - Device Name: BLE GATT Server Demo" << std::endl;
    std::cout << "  - Type: Connectable Peripheral" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;

    // 启动定时器
    g_timeout_add_seconds(10, update_battery, nullptr);
    g_timeout_add_seconds(5, update_counter, nullptr);

    // 运行主循环
    main_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(main_loop);

    // 清理
    std::cout << "Shutting down..." << std::endl;
    if (reg_id > 0) {
        g_dbus_connection_unregister_object(connection, reg_id);
    }
    if (connection) {
        g_object_unref(connection);
    }
    if (main_loop) {
        g_main_loop_unref(main_loop);
    }

    std::cout << "GATT Server stopped" << std::endl;
    return 0;
}