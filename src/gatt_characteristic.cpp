#include "gatt_characteristic.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <glib-2.0/glib.h>

namespace Bluetooth {

// D-Bus接口静态成员定义
const GDBusArgInfo GattCharacteristic::method_read_value_args[] = {
    { "options", "a{sv}", nullptr },
    { nullptr, nullptr, nullptr }
};

const GDBusArgInfo GattCharacteristic::method_read_value_result[] = {
    { "value", "ay", nullptr },
    { nullptr, nullptr, nullptr }
};

const GDBusArgInfo GattCharacteristic::method_write_value_args[] = {
    { "value", "ay", nullptr },
    { "options", "a{sv}", nullptr },
    { nullptr, nullptr, nullptr }
};

const GDBusMethodInfo GattCharacteristic::interface_methods[] = {
    {
        "ReadValue",
        method_read_value_args,
        method_read_value_result,
        nullptr
    },
    {
        "WriteValue",
        method_write_value_args,
        nullptr,
        nullptr
    },
    {
        "StartNotify",
        nullptr,
        nullptr,
        nullptr
    },
    {
        "StopNotify",
        nullptr,
        nullptr,
        nullptr
    },
    { nullptr, nullptr, nullptr, nullptr }
};

const GDBusPropertyInfo GattCharacteristic::interface_properties[] = {
    { "UUID", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Flags", "as", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Notifying", "b", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { "Value", "ay", G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
    { nullptr, nullptr, G_DBUS_PROPERTY_INFO_FLAGS_NONE }
};

const GDBusInterfaceInfo GattCharacteristic::interface_info_ = {
    -1,
    "org.bluez.GattCharacteristic1",
    interface_methods,
    nullptr,
    interface_properties,
    nullptr
};

const GDBusInterfaceVTable GattCharacteristic::interface_vtable_ = {
    methodCallHandler,
    nullptr,
    onGetProperty,
    nullptr,
    nullptr
};

GattCharacteristic::GattCharacteristic(const std::string& uuid,
                                     const std::vector<CharacteristicFlags>& flags,
                                     const std::string& object_path_prefix)
    : uuid_(uuid), flags_(flags), connection_(nullptr), registration_id_(0), notifying_(false) {

    // 生成唯一对象路径
    static int characteristic_counter = 0;
    object_path_ = object_path_prefix + std::to_string(characteristic_counter++);
}

GattCharacteristic::~GattCharacteristic() {
    unexportInterface();
}

bool GattCharacteristic::exportInterface(GDBusConnection* connection, const std::string& service_path) {
    if (!connection || registration_id_ != 0) {
        return false;
    }

    connection_ = connection;
    GError* error = nullptr;

    // 创建方法调用的vtable
    GDBusInterfaceVTable vtable = {
        methodCallHandler,
        nullptr,
        onGetProperty,
        nullptr,
        nullptr
    };

    registration_id_ = g_dbus_connection_register_object(
        connection,
        object_path_.c_str(),
        const_cast<GDBusInterfaceInfo*>(&interface_info_),
        &vtable,
        this,
        nullptr,
        &error
    );

    if (registration_id_ == 0) {
        std::cerr << "Failed to register GATT characteristic: " << error->message << std::endl;
        g_error_free(error);
        connection_ = nullptr;
        return false;
    }

    std::cout << "GATT characteristic exported at path: " << object_path_
              << " with UUID: " << uuid_ << std::endl;
    return true;
}

void GattCharacteristic::unexportInterface() {
    if (connection_ && registration_id_ != 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
        registration_id_ = 0;
        std::cout << "GATT characteristic unexported: " << uuid_ << std::endl;
    }
    connection_ = nullptr;
}

void GattCharacteristic::setValue(const std::vector<uint8_t>& value) {
    value_ = value;
    if (notifying_) {
        notifyValueChanged();
    }
}

void GattCharacteristic::notifyValueChanged() {
    if (!notifying_ || !connection_) {
        return;
    }

    GVariant* value_variant = bytesToGvariant(value_);
    emitPropertyChanged("Value", value_variant);
    g_variant_unref(value_variant);
}

std::vector<std::string> GattCharacteristic::getFlags() const {
    std::vector<std::string> flags_str;
    for (const auto& flag : flags_) {
        flags_str.push_back(characteristicFlagsToString(flag));
    }
    return flags_str;
}

GVariant* GattCharacteristic::handleReadValue(GVariant* options) {
    std::cout << "ReadValue called on characteristic: " << uuid_ << std::endl;

    // 如果设置了读取回调，调用回调获取值
    if (read_callback_) {
        // 从选项中获取设备路径（如果有的话）
        std::string device_path = "";
        value_ = read_callback_(device_path);
    }

    return bytesToGvariant(value_);
}

bool GattCharacteristic::handleWriteValue(GVariant* value, GVariant* options) {
    std::cout << "WriteValue called on characteristic: " << uuid_ << std::endl;

    std::vector<uint8_t> new_value = gvariantToBytes(value);

    // 如果设置了写入回调，调用回调
    if (write_callback_) {
        std::string device_path = "";
        if (!write_callback_(device_path, new_value)) {
            std::cerr << "Write rejected by callback" << std::endl;
            return false;
        }
    }

    value_ = new_value;
    std::cout << "Characteristic value updated" << std::endl;

    // 如果启用了通知，发送值更改通知
    if (notifying_) {
        notifyValueChanged();
    }

    return true;
}

void GattCharacteristic::handleStartNotify(const std::string& device_path) {
    std::cout << "StartNotify called on characteristic: " << uuid_
              << " from device: " << device_path << std::endl;

    notifying_ = true;
    notified_devices_.push_back(device_path);

    if (notify_callback_) {
        notify_callback_(device_path, true);
    }
}

void GattCharacteristic::handleStopNotify(const std::string& device_path) {
    std::cout << "StopNotify called on characteristic: " << uuid_
              << " from device: " << device_path << std::endl;

    auto it = std::find(notified_devices_.begin(), notified_devices_.end(), device_path);
    if (it != notified_devices_.end()) {
        notified_devices_.erase(it);
    }

    if (notified_devices_.empty()) {
        notifying_ = false;
    }

    if (notify_callback_) {
        notify_callback_(device_path, false);
    }
}

void GattCharacteristic::emitPropertyChanged(const std::string& property_name, GVariant* value) {
    if (!connection_) {
        return;
    }

    GError* error = nullptr;
    GDBusMessage* message = g_dbus_message_new_signal(
        object_path_.c_str(),
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged"
    );

    GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(builder, "{sv}", property_name.c_str(), value);
    GVariant* properties = g_variant_new("a{sv}", builder);
    g_variant_builder_unref(builder);

    GVariant* parameters = g_variant_new("(sa{sv}as)",
        "org.bluez.GattCharacteristic1", properties, nullptr);

    g_dbus_message_set_body(message, parameters);

    gboolean sent = g_dbus_connection_send_message(connection_, message,
        G_DBUS_SEND_MESSAGE_FLAGS_NONE, &error);

    g_object_unref(message);

    if (!sent) {
        std::cerr << "Failed to emit PropertyChanged signal: " << error->message << std::endl;
        g_error_free(error);
    }
}

std::vector<uint8_t> GattCharacteristic::gvariantToBytes(GVariant* variant) {
    std::vector<uint8_t> bytes;
    gsize n_elements;
    const uint8_t* data = static_cast<const uint8_t*>(g_variant_get_fixed_array(variant, &n_elements, sizeof(uint8_t)));

    if (data) {
        bytes.assign(data, data + n_elements);
    }

    return bytes;
}

GVariant* GattCharacteristic::bytesToGvariant(const std::vector<uint8_t>& bytes) {
    return g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                    bytes.data(), bytes.size(), sizeof(uint8_t));
}

std::string characteristicFlagsToString(CharacteristicFlags flag) {
    switch (flag) {
        case CharacteristicFlags::READ:
            return "read";
        case CharacteristicFlags::WRITE:
            return "write";
        case CharacteristicFlags::WRITE_WITHOUT_RESPONSE:
            return "write-without-response";
        case CharacteristicFlags::SIGNED_WRITE:
            return "signed-write";
        case CharacteristicFlags::RELIABLE_WRITE:
            return "reliable-write";
        case CharacteristicFlags::NOTIFY:
            return "notify";
        case CharacteristicFlags::INDICATE:
            return "indicate";
        default:
            return "unknown";
    }
}

// D-Bus方法处理器
void GattCharacteristic::methodCallHandler(GDBusConnection* connection,
                                           const gchar* sender,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* method_name,
                                           GVariant* parameters,
                                           gpointer user_data,
                                           GDBusMethodInvocation* invocation) {
    GattCharacteristic* characteristic = static_cast<GattCharacteristic*>(user_data);

    if (g_strcmp0(method_name, "ReadValue") == 0) {
        GVariant* options = g_variant_get_child_value(parameters, 0);
        GVariant* result = characteristic->handleReadValue(options);
        g_dbus_method_invocation_return_value(invocation, result);
        g_variant_unref(result);
        g_variant_unref(options);
    } else if (g_strcmp0(method_name, "WriteValue") == 0) {
        GVariant* value = g_variant_get_child_value(parameters, 0);
        GVariant* options = g_variant_get_child_value(parameters, 1);

        bool success = characteristic->handleWriteValue(value, options);
        if (success) {
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else {
            g_dbus_method_invocation_return_error(invocation,
                G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Write operation failed");
        }

        g_variant_unref(value);
        g_variant_unref(options);
    } else if (g_strcmp0(method_name, "StartNotify") == 0) {
        characteristic->handleStartNotify(sender);
        g_dbus_method_invocation_return_value(invocation, nullptr);
    } else if (g_strcmp0(method_name, "StopNotify") == 0) {
        characteristic->handleStopNotify(sender);
        g_dbus_method_invocation_return_value(invocation, nullptr);
    } else {
        g_dbus_method_invocation_return_error(invocation,
            G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    }

    return nullptr;
}

gboolean GattCharacteristic::onGetProperty(GDBusConnection* connection,
                                         const gchar* sender,
                                         const gchar* object_path,
                                         const gchar* interface_name,
                                         const gchar* property_name,
                                         GVariant** value,
                                         GError** error,
                                         gpointer user_data) {
    GattCharacteristic* characteristic = static_cast<GattCharacteristic*>(user_data);

    if (g_strcmp0(property_name, "UUID") == 0) {
        *value = g_variant_new_string(characteristic->uuid_.c_str());
        return TRUE;
    } else if (g_strcmp0(property_name, "Flags") == 0) {
        std::vector<std::string> flags = characteristic->getFlags();
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        for (const auto& flag : flags) {
            g_variant_builder_add(builder, "s", flag.c_str());
        }
        *value = g_variant_new("as", builder);
        g_variant_builder_unref(builder);
        return TRUE;
    } else if (g_strcmp0(property_name, "Notifying") == 0) {
        *value = g_variant_new_boolean(characteristic->notifying_);
        return TRUE;
    } else if (g_strcmp0(property_name, "Value") == 0) {
        *value = characteristic->bytesToGvariant(characteristic->value_);
        return TRUE;
    }

    return FALSE;
}

} // namespace Bluetooth