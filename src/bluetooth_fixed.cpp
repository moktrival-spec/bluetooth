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

// 方法调用处理 - GATT应用
void gatt_app_method_handler(GDBusConnection* conn,
                             const gchar* sender,
                             const gchar* object_path,
                             const gchar* interface_name,
                             const gchar* method_name,
                             GVariant* parameters,
                             GDBusMethodInvocation* invocation,
                             gpointer user_data) {

    std::cout << "GATT App Method: " << method_name << std::endl;

    if (g_strcmp0(method_name, "GetServices") == 0) {
        // 返回服务路径
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
        g_variant_builder_add(builder, "o", "/org/bluez/example/gatt/service0");
        GVariant* result = g_variant_new("(ao)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, result);
        return;
    }
}

// 属性设置处理 - GATT应用
gboolean gatt_app_set_property(GDBusConnection* conn,
                               const gchar* sender,
                               const gchar* object_path,
                               const gchar* interface_name,
                               const gchar* property_name,
                               GVariant* value,
                               GError** error,
                               gpointer user_data) {
    return FALSE; // GATT应用不支持设置属性
}

// 方法调用处理 - GATT服务
void gatt_service_method_handler(GDBusConnection* conn,
                                const gchar* sender,
                                const gchar* object_path,
                                const gchar* interface_name,
                                const gchar* method_name,
                                GVariant* parameters,
                                GDBusMethodInvocation* invocation,
                                gpointer user_data) {
    std::cout << "GATT Service Method: " << method_name << std::endl;
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

// 属性获取处理 - GATT服务
gboolean gatt_service_get_property(GDBusConnection* conn,
                                   const gchar* sender,
                                   const gchar* object_path,
                                   const gchar* interface_name,
                                   const gchar* property_name,
                                   GVariant** value,
                                   GError** error,
                                   gpointer user_data) {

    if (g_strcmp0(property_name, "UUID") == 0) {
        *value = g_variant_new_string("0000180f-0000-1000-8000-00805f9b34fb"); // 电池服务
        return TRUE;
    }
    if (g_strcmp0(property_name, "Primary") == 0) {
        *value = g_variant_new_boolean(TRUE);
        return TRUE;
    }
    if (g_strcmp0(property_name, "Characteristics") == 0) {
        const char* characteristics[] = { "/org/bluez/example/gatt/service0/char0", nullptr };
        *value = g_variant_new_strv(characteristics, 1);
        return TRUE;
    }

    return FALSE;
}

// 属性设置处理 - GATT服务
gboolean gatt_service_set_property(GDBusConnection* conn,
                                   const gchar* sender,
                                   const gchar* object_path,
                                   const gchar* interface_name,
                                   const gchar* property_name,
                                   GVariant* value,
                                   GError** error,
                                   gpointer user_data) {
    return FALSE; // GATT服务属性都是只读的
}

// 方法调用处理 - GATT特征值
void gatt_char_method_handler(GDBusConnection* conn,
                              const gchar* sender,
                              const gchar* object_path,
                              const gchar* interface_name,
                              const gchar* method_name,
                              GVariant* parameters,
                              GDBusMethodInvocation* invocation,
                              gpointer user_data) {

    std::cout << "GATT Characteristic Method: " << method_name << std::endl;

    if (g_strcmp0(method_name, "ReadValue") == 0) {
        // 返回电池电量
        uint8_t battery_level = 85;
        GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, &battery_level, 1, sizeof(uint8_t));
        g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&value, 1));
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

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

// 属性获取处理 - GATT特征值
gboolean gatt_char_get_property(GDBusConnection* conn,
                                const gchar* sender,
                                const gchar* object_path,
                                const gchar* interface_name,
                                const gchar* property_name,
                                GVariant** value,
                                GError** error,
                                gpointer user_data) {

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

    return FALSE;
}

// 属性设置处理 - GATT特征值
gboolean gatt_char_set_property(GDBusConnection* conn,
                                const gchar* sender,
                                const gchar* object_path,
                                const gchar* interface_name,
                                const gchar* property_name,
                                GVariant* value,
                                GError** error,
                                gpointer user_data) {
    return FALSE; // GATT特征值属性都是只读的
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

    g_clear_error(&error);

    result = g_dbus_connection_call_sync(connection,
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
    g_usleep(500000); // 500ms
    std::cout << "Bluetooth adapter powered on successfully" << std::endl;
    return TRUE;
}

// 注册GATT应用
gboolean register_gatt_application() {
    GError* error = nullptr;

    // 等待一下确保适配器完全启动
    g_usleep(1000000); // 1秒

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
    std::cout << "GATT application registered successfully" << std::endl;
    return TRUE;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Fixed BLE GATT Server ===" << std::endl;

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

    // 启用适配器
    if (!power_on_adapter()) {
        std::cerr << "Failed to power on adapter" << std::endl;
        return 1;
    }

    // 创建GATT应用接口
    GDBusNodeInfo* app_info = g_dbus_node_info_new_for_xml(
        "<node>"
        "  <interface name='org.bluez.GattApplication1'>"
        "    <method name='GetServices'>"
        "      <arg type='ao' name='services' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>", &error);

    if (!app_info) {
        std::cerr << "Failed to parse app XML: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    // GATT应用vtable
    GDBusInterfaceVTable app_vtable = {
        gatt_app_method_handler,
        nullptr,
        nullptr,
        gatt_app_set_property
    };

    guint app_reg_id = g_dbus_connection_register_object(connection,
        APP_PATH, app_info->interfaces[0], &app_vtable, nullptr, nullptr, &error);

    if (app_reg_id == 0) {
        std::cerr << "Failed to register GATT application: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(app_info);
        return 1;
    }

    std::cout << "GATT application registered at: " << APP_PATH << std::endl;

    // 创建GATT服务接口
    GDBusNodeInfo* service_info = g_dbus_node_info_new_for_xml(
        "<node>"
        "  <interface name='org.bluez.GattService1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Primary' type='b' access='read'/>"
        "    <property name='Characteristics' type='ao' access='read'/>"
        "  </interface>"
        "</node>", nullptr);

    // GATT服务vtable
    GDBusInterfaceVTable service_vtable = {
        gatt_service_method_handler,
        nullptr,
        gatt_service_get_property,
        gatt_service_set_property
    };

    guint service_reg_id = g_dbus_connection_register_object(connection,
        "/org/bluez/example/gatt/service0", service_info->interfaces[0], &service_vtable, nullptr, nullptr, nullptr);

    if (service_reg_id == 0) {
        std::cerr << "Failed to register GATT service" << std::endl;
        g_dbus_node_info_unref(app_info);
        g_dbus_node_info_unref(service_info);
        return 1;
    }

    std::cout << "GATT service registered at: /org/bluez/example/gatt/service0" << std::endl;

    // 创建GATT特征值接口
    GDBusNodeInfo* char_info = g_dbus_node_info_new_for_xml(
        "<node>"
        "  <interface name='org.bluez.GattCharacteristic1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Flags' type='as' access='read'/>"
        "    <property name='Notifying' type='b' access='read'/>"
        "    <method name='ReadValue'>"
        "      <arg type='a{sv}' direction='in'/>"
        "      <arg type='ay' direction='out'/>"
        "    </method>"
        "    <method name='WriteValue'>"
        "      <arg type='ay' direction='in'/>"
        "      <arg type='a{sv}' direction='in'/>"
        "    </method>"
        "    <method name='StartNotify'/>"
        "    <method name='StopNotify'/>"
        "  </interface>"
        "</node>", nullptr);

    // GATT特征值vtable
    GDBusInterfaceVTable char_vtable = {
        gatt_char_method_handler,
        nullptr,
        gatt_char_get_property,
        gatt_char_set_property
    };

    guint char_reg_id = g_dbus_connection_register_object(connection,
        "/org/bluez/example/gatt/service0/char0", char_info->interfaces[0], &char_vtable, nullptr, nullptr, nullptr);

    if (char_reg_id == 0) {
        std::cerr << "Failed to register GATT characteristic" << std::endl;
        g_dbus_node_info_unref(app_info);
        g_dbus_node_info_unref(service_info);
        g_dbus_node_info_unref(char_info);
        return 1;
    }

    std::cout << "GATT characteristic registered at: /org/bluez/example/gatt/service0/char0" << std::endl;

    // 注册GATT应用到BlueZ
    if (!register_gatt_application()) {
        std::cerr << "Failed to register GATT application with BlueZ" << std::endl;
        // 不要直接返回，继续运行用于调试
    }

    // 清理接口信息
    g_dbus_node_info_unref(app_info);
    g_dbus_node_info_unref(service_info);
    g_dbus_node_info_unref(char_info);

    std::cout << "\n=== GATT Server Setup Complete ===" << std::endl;
    std::cout << "Battery Service (0x180F)" << std::endl;
    std::cout << "Battery Level (0x2A19) - Read/Notify" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

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

    std::cout << "GATT Server stopped" << std::endl;
    return 0;
}