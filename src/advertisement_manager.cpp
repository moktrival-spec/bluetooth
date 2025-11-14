#include "advertisement_manager.h"
#include "bluez_interface.h"
#include <iostream>
#include <map>
#include <glib-2.0/glib.h>

namespace Bluetooth {

// D-Bus接口静态成员定义
const GDBusMethodInfo AdvertisementManager::interface_methods[] = {
    {
        "Release",
        nullptr,
        nullptr,
        nullptr
    },
    { nullptr, nullptr, nullptr, nullptr }
};

const GDBusPropertyInfo AdvertisementManager::interface_properties[] = {
    { "Type", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "ServiceUUIDs", "as", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "ManufacturerData", "a{qv}", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "SolicitUUIDs", "as", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "ServiceData", "a{sv}", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "IncludeTxPower", "b", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "LocalName", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Appearance", "q", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Duration", "q", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Timeout", "q", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "SecondaryChannel", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { nullptr, nullptr, G_DBUS_PROPERTY_INFO_FLAGS_NONE }
};

const GDBusInterfaceInfo AdvertisementManager::interface_info_ = {
    -1,
    "org.bluez.LEAdvertisement1",
    interface_methods,
    nullptr,
    interface_properties,
    nullptr
};

const GDBusInterfaceVTable AdvertisementManager::interface_vtable_ = {
    methodRelease,
    nullptr,
    onGetProperty,
    nullptr,
    nullptr
};

AdvertisementManager::AdvertisementManager(const std::string& object_path, AdvertisementType type)
    : object_path_(object_path), type_(type), connection_(nullptr), registration_id_(0),
      discoverable_(true), connectable_(true), min_advertising_interval_(100), max_advertising_interval_(500) {
}

AdvertisementManager::~AdvertisementManager() {
    unexportInterface();
}

bool AdvertisementManager::exportInterface(GDBusConnection* connection) {
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
        std::cerr << "Failed to register advertisement: " << error->message << std::endl;
        g_error_free(error);
        connection_ = nullptr;
        return false;
    }

    std::cout << "Advertisement exported at path: " << object_path_ << std::endl;
    return true;
}

void AdvertisementManager::unexportInterface() {
    if (connection_ && registration_id_ != 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
        registration_id_ = 0;
        std::cout << "Advertisement unexported" << std::endl;
    }
    connection_ = nullptr;
}

void AdvertisementManager::setDeviceName(const std::string& name) {
    device_name_ = name;
    std::cout << "Device name set to: " << name << std::endl;
}

void AdvertisementManager::setManufacturerData(uint16_t company_id, const std::vector<uint8_t>& data) {
    manufacturer_data_[company_id] = data;
    std::cout << "Manufacturer data set for company ID: " << company_id << std::endl;
}

void AdvertisementManager::setServiceUUIDs(const std::vector<std::string>& service_uuids) {
    service_uuids_ = service_uuids;
    std::cout << "Service UUIDs set: ";
    for (const auto& uuid : service_uuids) {
        std::cout << uuid << " ";
    }
    std::cout << std::endl;
}

void AdvertisementManager::setServiceData(const std::string& service_uuid, const std::vector<uint8_t>& data) {
    service_data_[service_uuid] = data;
    std::cout << "Service data set for UUID: " << service_uuid << std::endl;
}

void AdvertisementManager::setTransportSettings(bool discoverable, bool connectable) {
    discoverable_ = discoverable;
    connectable_ = connectable;
    std::cout << "Transport settings - Discoverable: " << discoverable
              << ", Connectable: " << connectable << std::endl;
}

void AdvertisementManager::setAdvertisingInterval(uint16_t min_interval, uint16_t max_interval) {
    min_advertising_interval_ = min_interval;
    max_advertising_interval_ = max_interval;
    std::cout << "Advertising interval set: " << min_interval << "-" << max_interval << "ms" << std::endl;
}

bool AdvertisementManager::releaseAdvertisement(GDBusConnection* connection, ErrorCallback callback) {
    std::cout << "Advertisement release requested" << std::endl;
    return true;
}

void AdvertisementManager::handleRelease() {
    std::cout << "Advertisement released by BlueZ" << std::endl;
}

std::vector<std::string> AdvertisementManager::getTypeFlags() const {
    std::vector<std::string> flags;

    if (discoverable_) {
        flags.push_back("discoverable");
    }
    if (connectable_) {
        flags.push_back("connectable");
    }

    return flags;
}

std::map<std::string, GVariant*> AdvertisementManager::getManufacturerDataVariant() const {
    std::map<std::string, GVariant*> data_map;

    for (const auto& pair : manufacturer_data_) {
        std::string key = std::to_string(pair.first);
        GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                  pair.second.data(),
                                                  pair.second.size(),
                                                  sizeof(uint8_t));
        data_map[key] = value;
    }

    return data_map;
}

std::map<std::string, GVariant*> AdvertisementManager::getServiceDataVariant() const {
    std::map<std::string, GVariant*> data_map;

    for (const auto& pair : service_data_) {
        GVariant* value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                  pair.second.data(),
                                                  pair.second.size(),
                                                  sizeof(uint8_t));
        data_map[pair.first] = value;
    }

    return data_map;
}

GVariant* AdvertisementManager::methodRelease(GDBusConnection* connection,
                                             const gchar* sender,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* method_name,
                                             GVariant* parameters,
                                             gpointer user_data,
                                             GDBusMethodInvocation* invocation) {
    AdvertisementManager* ad = static_cast<AdvertisementManager*>(user_data);
    ad->handleRelease();
    g_dbus_method_invocation_return_value(invocation, nullptr);
    return nullptr;
}

gboolean AdvertisementManager::onGetProperty(GDBusConnection* connection,
                                            const gchar* sender,
                                            const gchar* object_path,
                                            const gchar* interface_name,
                                            const gchar* property_name,
                                            GVariant** value,
                                            GError** error,
                                            gpointer user_data) {
    AdvertisementManager* ad = static_cast<AdvertisementManager*>(user_data);

    if (g_strcmp0(property_name, "Type") == 0) {
        const char* type_str = (ad->type_ == AdvertisementType::PERIPHERAL) ? "peripheral" : "broadcast";
        *value = g_variant_new_string(type_str);
        return TRUE;
    } else if (g_strcmp0(property_name, "ServiceUUIDs") == 0) {
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        for (const auto& uuid : ad->service_uuids_) {
            g_variant_builder_add(builder, "s", uuid.c_str());
        }
        *value = g_variant_new("as", builder);
        g_variant_builder_unref(builder);
        return TRUE;
    } else if (g_strcmp0(property_name, "ManufacturerData") == 0) {
        // 构建制造商数据字典
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("a{qv}"));
        for (const auto& pair : ad->manufacturer_data_) {
            GVariant* data = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                      pair.second.data(),
                                                      pair.second.size(),
                                                      sizeof(uint8_t));
            g_variant_builder_add(builder, "{qv}", g_variant_new_uint16(pair.first), data);
        }
        *value = g_variant_new("a{qv}", builder);
        g_variant_builder_unref(builder);
        return TRUE;
    } else if (g_strcmp0(property_name, "ServiceData") == 0) {
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        for (const auto& pair : ad->service_data_) {
            GVariant* data = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                      pair.second.data(),
                                                      pair.second.size(),
                                                      sizeof(uint8_t));
            g_variant_builder_add(builder, "{sv}", pair.first.c_str(), data);
        }
        *value = g_variant_new("a{sv}", builder);
        g_variant_builder_unref(builder);
        return TRUE;
    } else if (g_strcmp0(property_name, "LocalName") == 0) {
        *value = g_variant_new_string(ad->device_name_.c_str());
        return TRUE;
    }

    return FALSE;
}

AdvertisementRegistrar::AdvertisementRegistrar() : error_callback_(nullptr) {
}

AdvertisementRegistrar::~AdvertisementRegistrar() {
}

bool AdvertisementRegistrar::registerAdvertisement(GDBusConnection* connection,
                                                 AdvertisementManager* advertisement,
                                                 ErrorCallback callback) {
    if (!connection || !advertisement) {
        return false;
    }

    error_callback_ = callback;
    GError* error = nullptr;

    GDBusProxy* ad_manager = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        BLUEZ_ADAPTER_PATH,
        LE_ADVERTISEMENT_MANAGER_INTERFACE,
        nullptr,
        &error
    );

    if (!ad_manager) {
        std::cerr << "Failed to create advertisement manager proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 注册广告
    GVariant* options = g_variant_new("a{sv}", nullptr);
    GVariant* result = g_dbus_proxy_call_sync(
        ad_manager,
        "RegisterAdvertisement",
        g_variant_new("(oa{sv})", advertisement->getObjectPath().c_str(), options),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    g_object_unref(ad_manager);

    if (!result) {
        std::cerr << "Failed to register advertisement: " << error->message << std::endl;
        g_error_free(error);
        if (error_callback_) {
            error_callback_(error->message);
        }
        return false;
    }

    g_variant_unref(result);
    std::cout << "Advertisement registered successfully" << std::endl;
    return true;
}

bool AdvertisementRegistrar::unregisterAdvertisement(GDBusConnection* connection,
                                                   AdvertisementManager* advertisement,
                                                   ErrorCallback callback) {
    if (!connection || !advertisement) {
        return false;
    }

    // TODO: 实现广告注销逻辑
    std::cout << "Advertisement unregistration requested" << std::endl;
    return true;
}

} // namespace Bluetooth