#include "bluez_interface.h"
#include "gatt_application.h"
#include "advertisement_manager.h"
#include <glib-2.0/glib.h>
#include <iostream>

namespace Bluetooth {

BluezInterface::BluezInterface()
    : connection_(nullptr), object_manager_(nullptr), adapter_proxy_(nullptr), initialized_(false) {
}

BluezInterface::~BluezInterface() {
    if (adapter_proxy_) {
        g_object_unref(adapter_proxy_);
    }
    if (object_manager_) {
        g_signal_handlers_disconnect_by_data(object_manager_, this);
        g_object_unref(object_manager_);
    }
    if (connection_) {
        g_object_unref(connection_);
    }
}

bool BluezInterface::initialize() {
    GError* error = nullptr;

    // 获取系统D-Bus连接
    connection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!connection_) {
        std::cerr << "Failed to get D-Bus connection: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 设置对象管理器
    if (!setupObjectManager()) {
        std::cerr << "Failed to setup object manager" << std::endl;
        return false;
    }

    // 查找蓝牙适配器
    if (!findAdapter()) {
        std::cerr << "Failed to find Bluetooth adapter" << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "BlueZ interface initialized successfully" << std::endl;
    return true;
}

bool BluezInterface::setupObjectManager() {
    GError* error = nullptr;

    object_manager_ = g_dbus_object_manager_client_new_sync(
        connection_,
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
        BLUEZ_SERVICE,
        "/",
        nullptr, nullptr, nullptr, nullptr,
        &error
    );

    if (!object_manager_) {
        std::cerr << "Failed to create object manager: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 连接信号处理器
    g_signal_connect(object_manager_, "interface-added",
                    G_CALLBACK(onInterfaceAddedStatic), this);
    g_signal_connect(object_manager_, "interface-removed",
                    G_CALLBACK(onInterfaceRemovedStatic), this);

    return true;
}

bool BluezInterface::findAdapter() {
    GList* objects = g_dbus_object_manager_get_objects(object_manager_);
    bool found = false;

    for (GList* l = objects; l != nullptr; l = g_list_next(l)) {
        GDBusObject* object = G_DBUS_OBJECT(l->data);
        const gchar* object_path = g_dbus_object_get_object_path(object);

        // 查找hci0适配器
        if (g_str_has_suffix(object_path, "hci0")) {
            adapter_proxy_ = G_DBUS_OBJECT_PROXY(g_dbus_object_proxy_new(
                connection_, BLUEZ_SERVICE, object_path));
            found = true;
            std::cout << "Found Bluetooth adapter: " << object_path << std::endl;
            break;
        }
    }

    g_list_free_full(objects, g_object_unref);
    return found;
}

bool BluezInterface::isBluezAvailable() const {
    if (!initialized_ || !connection_) {
        return false;
    }

    GError* error = nullptr;
    GDBusMessage* message = g_dbus_message_new_method_call(
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", BLUEZ_SERVICE)
    );

    GDBusMessage* reply = g_dbus_connection_send_message_with_reply_sync(
        connection_, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, -1, nullptr, nullptr, &error);

    g_object_unref(message);

    if (!reply) {
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    bool has_owner = false;
    GVariant* result = g_dbus_message_get_body(reply);
    if (result) {
        g_variant_get(result, "(b)", &has_owner);
    }

    g_object_unref(reply);
    return has_owner;
}

bool BluezInterface::powerOnAdapter() {
    if (!adapter_proxy_) {
        return false;
    }

    GError* error = nullptr;
    GDBusProxy* adapter = g_dbus_proxy_new_sync(
        connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        g_dbus_object_get_object_path(G_DBUS_OBJECT(adapter_proxy_)),
        "org.bluez.Adapter1",
        nullptr,
        &error
    );

    if (!adapter) {
        std::cerr << "Failed to create adapter proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 设置Powered属性为true
    GVariant* result = g_dbus_proxy_call_sync(
        adapter,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(true)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    g_object_unref(adapter);

    if (!result) {
        std::cerr << "Failed to power on adapter: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    g_variant_unref(result);
    std::cout << "Bluetooth adapter powered on" << std::endl;
    return true;
}

bool BluezInterface::registerApplication(GattApplication* application, ErrorCallback callback) {
    if (!initialized_ || !application) {
        return false;
    }

    error_callback_ = callback;
    GError* error = nullptr;

    GDBusProxy* gatt_manager = g_dbus_proxy_new_sync(
        connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        BLUEZ_ADAPTER_PATH,
        GATT_MANAGER_INTERFACE,
        nullptr,
        &error
    );

    if (!gatt_manager) {
        std::cerr << "Failed to create GATT manager proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 注册应用
    GVariant* options = g_variant_new("a{sv}", nullptr);
    GVariant* result = g_dbus_proxy_call_sync(
        gatt_manager,
        "RegisterApplication",
        g_variant_new("(oa{sv})", application->getObjectPath().c_str(), options),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    g_object_unref(gatt_manager);

    if (!result) {
        std::cerr << "Failed to register GATT application: " << error->message << std::endl;
        g_error_free(error);
        if (error_callback_) {
            error_callback_(error->message);
        }
        return false;
    }

    g_variant_unref(result);
    std::cout << "GATT application registered successfully" << std::endl;
    return true;
}

bool BluezInterface::unregisterApplication(ErrorCallback callback) {
    if (!initialized_) {
        return false;
    }

    // TODO: 实现应用注销逻辑
    std::cout << "Application unregistration requested" << std::endl;
    return true;
}

void BluezInterface::onInterfaceAddedStatic(GDBusObjectManager* manager,
                                           GDBusObject* object,
                                           GDBusInterface* interface,
                                           gpointer user_data) {
    BluezInterface* self = static_cast<BluezInterface*>(user_data);
    self->onInterfaceAdded(manager, object, interface);
}

void BluezInterface::onInterfaceRemovedStatic(GDBusObjectManager* manager,
                                             GDBusObject* object,
                                             GDBusInterface* interface,
                                             gpointer user_data) {
    BluezInterface* self = static_cast<BluezInterface*>(user_data);
    self->onInterfaceRemoved(manager, object, interface);
}

void BluezInterface::onInterfaceAdded(GDBusObjectManager* manager,
                                     GDBusObject* object,
                                     GDBusInterface* interface) {
    const gchar* interface_name = g_dbus_interface_get_info(interface)->name;
    std::cout << "Interface added: " << interface_name << std::endl;
}

void BluezInterface::onInterfaceRemoved(GDBusObjectManager* manager,
                                       GDBusObject* object,
                                       GDBusInterface* interface) {
    const gchar* interface_name = g_dbus_interface_get_info(interface)->name;
    std::cout << "Interface removed: " << interface_name << std::endl;
}

} // namespace Bluetooth