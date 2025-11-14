#include "gatt_application.h"
#include "gatt_service.h"
#include <iostream>
#include <glib-2.0/glib.h>

namespace Bluetooth {

// D-Bus接口静态成员定义
const GDBusArgInfo GattApplication::method_get_services_args[] = {
    { nullptr, nullptr, nullptr, nullptr }
};

const GDBusArgInfo GattApplication::method_get_services_result[] = {
    { nullptr, "services", "ao", nullptr },
    { nullptr, nullptr, nullptr, nullptr }
};

const GDBusMethodInfo GattApplication::interface_methods[] = {
    {
        "GetServices",
        method_get_services_args,
        method_get_services_result,
        nullptr
    },
    { nullptr, nullptr, nullptr, nullptr }
};

const GDBusInterfaceInfo GattApplication::interface_info_ = {
    -1,
    "org.bluez.GattApplication1",
    interface_methods,
    nullptr,
    nullptr,
    nullptr
};

const GDBusInterfaceVTable GattApplication::interface_vtable_ = {
    methodGetServices,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

GattApplication::GattApplication(const std::string& object_path)
    : object_path_(object_path), connection_(nullptr), registration_id_(0) {
}

GattApplication::~GattApplication() {
    unexportInterface();
}

bool GattApplication::exportInterface(GDBusConnection* connection) {
    if (!connection || registration_id_ != 0) {
        return false;
    }

    connection_ = connection;
    GError* error = nullptr;

    registration_id_ = g_dbus_connection_register_object(
        connection,
        object_path_.c_str(),
        const_cast<GDBusInterfaceInfo*>(&interface_info_),
        const_cast<GDBusInterfaceVTable*>(&interface_vtable_),
        this,
        nullptr,
        &error
    );

    if (registration_id_ == 0) {
        std::cerr << "Failed to register GATT application: " << error->message << std::endl;
        g_error_free(error);
        connection_ = nullptr;
        return false;
    }

    std::cout << "GATT application exported at path: " << object_path_ << std::endl;
    return true;
}

void GattApplication::unexportInterface() {
    if (connection_ && registration_id_ != 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
        registration_id_ = 0;
        std::cout << "GATT application unexported" << std::endl;
    }
    connection_ = nullptr;
}

bool GattApplication::addService(std::shared_ptr<GattService> service) {
    if (!service) {
        return false;
    }

    // 导出服务接口
    if (!service->exportInterface(connection_, object_path_)) {
        std::cerr << "Failed to export service: " << service->getUUID() << std::endl;
        return false;
    }

    services_.push_back(service);
    std::cout << "Added service: " << service->getUUID() << std::endl;
    return true;
}

GVariant* GattApplication::handleGetServices() {
    // 创建包含所有服务对象路径的数组
    GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));

    for (const auto& service : services_) {
        g_variant_builder_add(builder, "o", service->getObjectPath().c_str());
    }

    GVariant* result = g_variant_new("ao", builder);
    g_variant_builder_unref(builder);

    return result;
}

GVariant* GattApplication::methodGetServices(GDBusConnection* connection,
                                            const gchar* sender,
                                            const gchar* object_path,
                                            const gchar* interface_name,
                                            const gchar* method_name,
                                            GVariant* parameters,
                                            gpointer user_data,
                                            GDBusMethodInvocation* invocation) {
    GattApplication* app = static_cast<GattApplication*>(user_data);
    GVariant* result = app->handleGetServices();
    g_dbus_method_invocation_return_value(invocation, result);
    g_variant_unref(result);
    return nullptr;
}

} // namespace Bluetooth