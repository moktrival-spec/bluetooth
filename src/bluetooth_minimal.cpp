#include <gio/gio.h>
#include <glib-2.0/glib.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <cstdint>

// g++ -o bluetooth_minimal src/bluetooth_minimal.cpp `pkg-config --cflags --libs gio-2.0 glib-2.0` -std=c++17

// 蓝牙常量
constexpr const char* BLUEZ_SERVICE = "org.bluez";
constexpr const char* ADAPTER_PATH = "/org/bluez/hci0";

constexpr const char* APP_PATH = "/org/example";
constexpr const char* ADVERTISEMENT_PATH = "/org/example/gatt/advertisement";

constexpr const char* DBUS_OBJECT_MANAGER_IFACE = "org.freedesktop.DBus.ObjectManager";

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

// 统一的方法调用处理器 - 处理 ObjectManager 和 GattApplication1 接口
void unified_method_call_handler(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* method_name,
                        GVariant* parameters,
                        GDBusMethodInvocation* invocation,
                        gpointer user_data) {

    std::cerr << "=== 统一方法调用 ===" << std::endl;
    std::cerr << "接口: " << interface_name << std::endl;
    std::cerr << "方法: " << method_name << std::endl;
    std::cerr << "路径: " << object_path << std::endl;

    // ObjectManager 接口
    if (strcmp(interface_name, DBUS_OBJECT_MANAGER_IFACE) == 0) {
        if (strcmp(method_name, "GetManagedObjects") == 0) {
            std::cout << "GetManagedObjects 被调用 - 构建对象结构..." << std::endl;

            GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("a{oa{sa{sv}}}"));

            // 注意：应用根对象不应该包含在GetManagedObjects中
            // 只有服务和特征值对象应该在GetManagedObjects中返回

            // 添加服务对象
            GVariantBuilder* service_iface_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
            GVariantBuilder* service_props_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

            g_variant_builder_add(service_props_builder, "{sv}", "UUID",
                g_variant_new_string("0000180f-0000-1000-8000-00805f9b34fb"));
            g_variant_builder_add(service_props_builder, "{sv}", "Primary",
                g_variant_new_boolean(TRUE));

            g_variant_builder_add(service_iface_builder, "{sa{sv}}", "org.bluez.GattService1", service_props_builder);
            g_variant_builder_add(builder, "{oa{sa{sv}}}", "/org/example/service0", service_iface_builder);
            g_variant_builder_unref(service_props_builder);
            g_variant_builder_unref(service_iface_builder);

            // 添加特征值对象
            GVariantBuilder* char_iface_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
            GVariantBuilder* char_props_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

            g_variant_builder_add(char_props_builder, "{sv}", "UUID",
                g_variant_new_string("00002a19-0000-1000-8000-00805f9b34fb"));
            const gchar* flags[] = {"read", "notify", NULL};
            g_variant_builder_add(char_props_builder, "{sv}", "Flags",
                g_variant_new_strv(flags, -1));
            g_variant_builder_add(char_props_builder, "{sv}", "Notifying",
                g_variant_new_boolean(FALSE));
            g_variant_builder_add(char_props_builder, "{sv}", "Service",
                g_variant_new_object_path("/org/example/service0"));

            g_variant_builder_add(char_iface_builder, "{sa{sv}}", "org.bluez.GattCharacteristic1", char_props_builder);
            g_variant_builder_add(builder, "{oa{sa{sv}}}", "/org/example/service0/char0", char_iface_builder);
            g_variant_builder_unref(char_props_builder);
            g_variant_builder_unref(char_iface_builder);

            GVariant* result = g_variant_builder_end(builder);
            g_variant_builder_unref(builder);

            // 调试：检查返回的数据是否为空
            if (result) {
                std::cout << "GetManagedObjects 返回数据构建完成 (仅服务和特征值对象)" << std::endl;
                // g_variant_new_tuple() 会"偷取"传递给它的 GVariant 的引用
                // 所以不需要手动释放 result，tuple 会在返回时自动管理
                GVariant* tuple_result = g_variant_new_tuple(&result, 1);
                g_dbus_method_invocation_return_value(invocation, tuple_result);
                std::cout << "GetManagedObjects 响应已发送" << std::endl;
                // 不需要手动释放 result 和 tuple_result，它们由 g_dbus_method_invocation_return_value 管理
            } else {
                std::cerr << "GetManagedObjects 返回数据为空！" << std::endl;
                g_dbus_method_invocation_return_error(invocation,
                    G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Empty result");
            }
            return;
        }
    }

    // GattApplication1 接口
    if (strcmp(interface_name, "org.bluez.GattApplication1") == 0) {
        if (strcmp(method_name, "GetServices") == 0) {
            std::cout << "GetServices 被调用" << std::endl;
            GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
            g_variant_builder_add(builder, "o", "/org/example/service0");
            GVariant* result = g_variant_new("(ao)", builder);
            g_variant_builder_unref(builder);
            g_dbus_method_invocation_return_value(invocation, result);
            return;
        }
    }

    std::cerr << "未知方法调用: " << interface_name << "." << method_name << std::endl;
    g_dbus_method_invocation_return_error(invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

// 注册应用根路径的接口（ObjectManager + GattApplication1）
gboolean register_application_root_interfaces() {
    std::cout << "ljw start" << std::endl;
    GError* error = nullptr;

    // 统一的 XML 定义 - 包含 ObjectManager 和 GattApplication1
    const gchar* unified_xml =
        "<node>"
        "  <interface name='org.freedesktop.DBus.ObjectManager'>"
        "    <method name='GetManagedObjects'>"
        "      <arg type='a{oa{sa{sv}}}' name='objects' direction='out'/>"
        "    </method>"
        "    <signal name='InterfacesAdded'>"
        "      <arg type='o' name='object_path'/>"
        "      <arg type='a{sa{sv}}' name='interfaces_and_properties'/>"
        "    </signal>"
        "    <signal name='InterfacesRemoved'>"
        "      <arg type='o' name='object_path'/>"
        "      <arg type='as' name='interfaces'/>"
        "    </signal>"
        "  </interface>"
        "  <interface name='org.bluez.GattApplication1'>"
        "    <method name='GetServices'>"
        "      <arg type='ao' name='services' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(unified_xml, &error);
    if (!info) {
        std::cerr << "Failed to parse unified XML: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    // 统一的 vtable
    GDBusInterfaceVTable unified_vtable;
    unified_vtable.method_call = unified_method_call_handler;
    unified_vtable.get_property = nullptr;
    unified_vtable.set_property = nullptr;

    // 注册 ObjectManager 接口
    guint objmgr_reg_id = g_dbus_connection_register_object(connection,
        "/org/example",
        g_dbus_node_info_lookup_interface(info, "org.freedesktop.DBus.ObjectManager"),
        &unified_vtable, nullptr, nullptr, &error);

    if (objmgr_reg_id == 0) {
        std::cerr << "Failed to register ObjectManager: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(info);
        return FALSE;
    }

    std::cout << "Registered: /org/example (org.freedesktop.DBus.ObjectManager)" << std::endl;

    // 注册 GattApplication1 接口
    error = nullptr;
    guint app_reg_id = g_dbus_connection_register_object(connection,
        "/org/example",
        g_dbus_node_info_lookup_interface(info, "org.bluez.GattApplication1"),
        &unified_vtable, nullptr, nullptr, &error);

    if (app_reg_id == 0) {
        std::cerr << "Failed to register GattApplication1: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(info);
        return FALSE;
    }

    std::cout << "Registered: /org/example (org.bluez.GattApplication1)" << std::endl;

    g_dbus_node_info_unref(info);
    std::cout << "ljw end" << std::endl;
    return TRUE;
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
        g_variant_builder_add(builder, "o", "/org/example/service0");
        GVariant* result = g_variant_new("(ao)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, result);
        return;
    }

    if (g_strcmp0(method_name, "ReadValue") == 0) {
        // 返回模拟数据 (忽略可选参数)
        uint8_t data[] = {85}; // 模拟电池电量 85%
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
        if (g_strcmp0(property_name, "Service") == 0) {
            *value = g_variant_new_object_path("/org/example/service0");
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

// 应用专用的方法调用处理（ObjectManager 已在 RegisterObjManagerMethod 中处理）
void application_method_call_handler(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* method_name,
                        GVariant* parameters,
                        GDBusMethodInvocation* invocation,
                        gpointer user_data) {

    std::cout << "Application Method: " << method_name << " on " << interface_name << std::endl;

    if (g_strcmp0(method_name, "GetServices") == 0) {
        // 返回服务路径列表
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
        g_variant_builder_add(builder, "o", "/org/example/service0");
        GVariant* result = g_variant_new("(ao)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, result);
        return;
    }

    // 默认返回
    g_dbus_method_invocation_return_error(invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

// 注册服务接口
gboolean register_service_interfaces() {
    GError* error = nullptr;

    // GATT服务接口XML定义
    const gchar* service_xml =
        "<node>"
        "<interface name='org.bluez.GattService1'>"
        "<property name='UUID' type='s' access='read'/>"
        "<property name='Primary' type='b' access='read'/>"
        "</interface>"
        "</node>";

    GDBusNodeInfo* service_info = g_dbus_node_info_new_for_xml(service_xml, &error);
    if (!service_info) {
        std::cerr << "Failed to parse service XML: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    // GATT特征值接口XML定义
    const gchar* characteristic_xml =
        "<node>"
        "<interface name='org.bluez.GattCharacteristic1'>"
        "<property name='UUID' type='s' access='read'/>"
        "<property name='Flags' type='as' access='read'/>"
        "<property name='Notifying' type='b' access='read'/>"
        "<property name='Service' type='o' access='read'/>"
        "<method name='ReadValue'>"
        "<arg type='a{sv}' direction='in'/>"
        "<arg type='ay' direction='out'/>"
        "</method>"
        "<method name='WriteValue'>"
        "<arg type='ay' direction='in'/>"
        "<arg type='a{sv}' direction='in'/>"
        "</method>"
        "<method name='StartNotify'/>"
        "<method name='StopNotify'/>"
        "</interface>"
        "</node>";

    GDBusNodeInfo* char_info = g_dbus_node_info_new_for_xml(characteristic_xml, &error);
    if (!char_info) {
        std::cerr << "Failed to parse characteristic XML: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(service_info);
        return FALSE;
    }

    // 注册服务接口
    guint service_reg_id = g_dbus_connection_register_object(connection,
        "/org/example/service0", service_info->interfaces[0], &interface_vtable, nullptr, nullptr, &error);

    if (service_reg_id == 0) {
        std::cerr << "Failed to register service interface: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(service_info);
        g_dbus_node_info_unref(char_info);
        return FALSE;
    }

    std::cout << "Registered: /org/example/service0 (org.bluez.GattService1)" << std::endl;

    // 注册特征值接口
    guint char_reg_id = g_dbus_connection_register_object(connection,
        "/org/example/service0/char0", char_info->interfaces[0], &interface_vtable, nullptr, nullptr, &error);

    if (char_reg_id == 0) {
        std::cerr << "Failed to register characteristic interface: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(service_info);
        g_dbus_node_info_unref(char_info);
        return FALSE;
    }

    std::cout << "Registered: /org/example/service0/char0 (org.bluez.GattCharacteristic1)" << std::endl;

    g_dbus_node_info_unref(service_info);
    g_dbus_node_info_unref(char_info);
    return TRUE;
}

// 注册广告接口
gboolean register_advertisement_interface() {
    GError* error = nullptr;

    const gchar* advertisement_xml =
        "<node>"
        "<interface name='org.bluez.LEAdvertisement1'>"
        "<property name='Type' type='s' access='read'/>"
        "<property name='ServiceUUIDs' type='as' access='read'/>"
        "<property name='LocalName' type='s' access='read'/>"
        "<method name='Release'/>"
        "</interface>"
        "</node>";

    GDBusNodeInfo* ad_info = g_dbus_node_info_new_for_xml(advertisement_xml, &error);
    if (!ad_info) {
        std::cerr << "Failed to parse advertisement XML: " << error->message << std::endl;
        g_error_free(error);
        return FALSE;
    }

    guint ad_reg_id = g_dbus_connection_register_object(connection,
        "/org/example/gatt/advertisement", ad_info->interfaces[0], &interface_vtable, nullptr, nullptr, &error);

    if (ad_reg_id == 0) {
        std::cerr << "Failed to register advertisement interface: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(ad_info);
        return FALSE;
    }

    std::cout << "Registered: /org/example/advertisement (org.bluez.LEAdvertisement1)" << std::endl;

    g_dbus_node_info_unref(ad_info);
    return TRUE;
}

// 注册所有接口
gboolean register_interfaces() {
    // 注册应用根路径的接口（ObjectManager + GattApplication1）
    if (!register_application_root_interfaces()) {
        return FALSE;
    }

    // 注册服务和特征值接口
    if (!register_service_interfaces()) {
        return FALSE;
    }

    // 注册广告接口
    if (!register_advertisement_interface()) {
        return FALSE;
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

    // 注册接口（包括 ObjectManager）
    if (!register_interfaces()) {
        std::cerr << "Failed to register interfaces" << std::endl;
        return 1;
    }

    // // 启用适配器
    // if (!power_on_adapter()) {
    //     std::cerr << "Failed to power on adapter" << std::endl;
    //     std::cerr << "Continuing with adapter registration anyway..." << std::endl;
    //     // 不要直接返回，继续尝试注册应用
    // }

    // 注册GATT应用
    if (!register_gatt_application()) {
        std::cerr << "Failed to register GATT application" << std::endl;
        // return 1;
    }

    // 注册广告
    if (!register_advertisement()) {
        std::cerr << "Failed to register advertisement" << std::endl;
        // return 1;
    }

    std::cout << "\n=== Server Started ===" << std::endl;
    // std::cout << "Battery Service (0x180F)" << std::endl;
    // std::cout << "Battery Level (0x2A19) - Read/Notify" << std::endl;
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
