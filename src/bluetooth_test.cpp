#include <gio/gio.h>
#include <glib-2.0/glib.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <cstdint>

// 蓝牙常量
constexpr const char* BLUEZ_SERVICE = "org.bluez";
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

// 简单的方法调用处理
void method_call_handler(GDBusConnection* conn,
                        const gchar* sender,
                        const gchar* object_path,
                        const gchar* interface_name,
                        const gchar* method_name,
                        GVariant* parameters,
                        GDBusMethodInvocation* invocation,
                        gpointer user_data) {

    std::cout << "Method called: " << method_name << std::endl;

    if (g_strcmp0(method_name, "GetServices") == 0) {
        // 返回服务路径
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
        g_variant_builder_add(builder, "o", "/org/bluez/example/gatt/service0");
        GVariant* result = g_variant_new("(ao)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, result);
        return;
    }

    // 其他方法的简单处理
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

// 简化的vtable
const GDBusInterfaceVTable interface_vtable = {
    method_call_handler,
    nullptr,
    nullptr,
    nullptr
};

int main(int argc, char* argv[]) {
    std::cout << "=== BLE GATT Server Test ===" << std::endl;

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

    // 简单注册一个应用接口
    GDBusNodeInfo* app_info = g_dbus_node_info_new_for_xml(
        "<node>"
        "  <interface name='org.bluez.GattApplication1'>"
        "    <method name='GetServices'>"
        "      <arg type='ao' name='services' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>", &error);

    if (!app_info) {
        std::cerr << "Failed to parse XML: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    guint reg_id = g_dbus_connection_register_object(connection,
        APP_PATH, app_info->interfaces[0], &interface_vtable, nullptr, nullptr, &error);

    if (reg_id == 0) {
        std::cerr << "Failed to register object: " << error->message << std::endl;
        g_error_free(error);
        g_dbus_node_info_unref(app_info);
        return 1;
    }

    g_dbus_node_info_unref(app_info);
    std::cout << "GATT application interface registered at: " << APP_PATH << std::endl;

    // 尝试注册到BlueZ（可能会失败，但我们继续运行）
    std::cout << "Attempting to register with BlueZ..." << std::endl;
    GVariant* result = g_dbus_connection_call_sync(connection,
        BLUEZ_SERVICE, "/org/bluez/hci0", "org.bluez.GattManager1",
        "RegisterApplication",
        g_variant_new("(oa{sv})", APP_PATH, nullptr),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

    if (result) {
        std::cout << "Successfully registered with BlueZ!" << std::endl;
        g_variant_unref(result);
    } else {
        std::cout << "BlueZ registration failed (this is expected if adapter is not powered): " << error->message << std::endl;
        std::cout << "But the GATT server is still running and can be tested" << std::endl;
        g_error_free(error);
    }

    std::cout << "\n=== Test Server Running ===" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

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

    std::cout << "Test completed" << std::endl;
    return 0;
}