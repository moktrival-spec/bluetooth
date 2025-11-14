#include "gatt_service.h"
#include "gatt_characteristic.h"
#include <iostream>
#include <glib-2.0/glib.h>

namespace Bluetooth {

// D-Bus接口静态成员定义
const GDBusPropertyInfo GattService::interface_properties[] = {
    { "UUID", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Primary", "b", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Characteristics", "ao", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { nullptr, nullptr, G_DBUS_PROPERTY_INFO_FLAGS_NONE }
};

const GDBusInterfaceInfo GattService::interface_info_ = {
    -1,
    "org.bluez.GattService1",
    nullptr,
    nullptr,
    interface_properties,
    nullptr
};

const GDBusInterfaceVTable GattService::interface_vtable_ = {
    nullptr,
    nullptr,
    onGetProperty,
    nullptr,
    nullptr
};

GattService::GattService(const std::string& uuid, bool primary, const std::string& object_path_prefix)
    : uuid_(uuid), primary_(primary), connection_(nullptr), registration_id_(0) {

    // 生成唯一对象路径
    static int service_counter = 0;
    object_path_ = object_path_prefix + std::to_string(service_counter++);
}

GattService::~GattService() {
    unexportInterface();
}

bool GattService::exportInterface(GDBusConnection* connection, const std::string& application_path) {
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
        std::cerr << "Failed to register GATT service: " << error->message << std::endl;
        g_error_free(error);
        connection_ = nullptr;
        return false;
    }

    std::cout << "GATT service exported at path: " << object_path_
              << " with UUID: " << uuid_ << std::endl;
    return true;
}

void GattService::unexportInterface() {
    if (connection_ && registration_id_ != 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
        registration_id_ = 0;
        std::cout << "GATT service unexported: " << uuid_ << std::endl;
    }
    connection_ = nullptr;
}

bool GattService::addCharacteristic(std::shared_ptr<GattCharacteristic> characteristic) {
    if (!characteristic) {
        return false;
    }

    // 导出特征值接口
    if (!characteristic->exportInterface(connection_, object_path_)) {
        std::cerr << "Failed to export characteristic: " << characteristic->getUUID() << std::endl;
        return false;
    }

    characteristics_.push_back(characteristic);
    std::cout << "Added characteristic: " << characteristic->getUUID()
              << " to service: " << uuid_ << std::endl;
    return true;
}

GVariant* GattService::getCharacteristicList() {
    GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));

    for (const auto& characteristic : characteristics_) {
        g_variant_builder_add(builder, "o", characteristic->getObjectPath().c_str());
    }

    GVariant* result = g_variant_new("ao", builder);
    g_variant_builder_unref(builder);
    return result;
}

gboolean GattService::onGetProperty(GDBusConnection* connection,
                                   const gchar* sender,
                                   const gchar* object_path,
                                   const gchar* interface_name,
                                   const gchar* property_name,
                                   GVariant** value,
                                   GError** error,
                                   gpointer user_data) {
    GattService* service = static_cast<GattService*>(user_data);

    if (g_strcmp0(property_name, "UUID") == 0) {
        *value = g_variant_new_string(service->uuid_.c_str());
        return TRUE;
    } else if (g_strcmp0(property_name, "Primary") == 0) {
        *value = g_variant_new_boolean(service->primary_);
        return TRUE;
    } else if (g_strcmp0(property_name, "Characteristics") == 0) {
        *value = service->getCharacteristicList();
        return TRUE;
    }

    return FALSE;
}

} // namespace Bluetooth