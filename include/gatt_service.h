#ifndef GATT_SERVICE_H
#define GATT_SERVICE_H

#include <gio/gio.h>
#include <vector>
#include <memory>
#include <string>

namespace Bluetooth {

class GattCharacteristic;

/**
 * @brief GATT服务类
 * 实现org.bluez.GattService1 D-Bus接口
 */
class GattService {
public:
    GattService(const std::string& uuid,
                bool primary = true,
                const std::string& object_path_prefix = "/org/bluez/example/service");
    virtual ~GattService();

    // 禁用拷贝构造和赋值
    GattService(const GattService&) = delete;
    GattService& operator=(const GattService&) = delete;

    /**
     * @brief 导出D-Bus接口
     * @param connection D-Bus连接
     * @param application_path 应用对象路径
     * @return true表示成功，false表示失败
     */
    bool exportInterface(GDBusConnection* connection, const std::string& application_path);

    /**
     * @brief 取消导出D-Bus接口
     */
    void unexportInterface();

    /**
     * @brief 添加特征值
     * @param characteristic GATT特征值实例
     * @return true表示成功，false表示失败
     */
    bool addCharacteristic(std::shared_ptr<GattCharacteristic> characteristic);

    /**
     * @brief 获取所有特征值
     * @return 特征值列表
     */
    const std::vector<std::shared_ptr<GattCharacteristic>>& getCharacteristics() const {
        return characteristics_;
    }

    /**
     * @brief 获取服务UUID
     * @return 服务UUID
     */
    const std::string& getUUID() const { return uuid_; }

    /**
     * @brief 获取对象路径
     * @return D-Bus对象路径
     */
    const std::string& getObjectPath() const { return object_path_; }

    /**
     * @brief 判断是否为主服务
     * @return true表示主服务，false表示从服务
     */
    bool isPrimary() const { return primary_; }

protected:
    /**
     * @brief 生成特征值列表
     * @return 包含所有特征值对象路径的GVariant
     */
    GVariant* getCharacteristicList();

private:
    std::string uuid_;
    bool primary_;
    std::string object_path_;
    GDBusConnection* connection_;
    guint registration_id_;
    std::vector<std::shared_ptr<GattCharacteristic>> characteristics_;

    // D-Bus属性获取回调
    static gboolean onGetProperty(GDBusConnection* connection,
                                 const gchar* sender,
                                 const gchar* object_path,
                                 const gchar* interface_name,
                                 const gchar* property_name,
                                 GVariant** value,
                                 GError** error,
                                 gpointer user_data);

    // D-Bus接口定义
    static const GDBusPropertyInfo interface_properties[];
    static const GDBusInterfaceInfo interface_info_;
    static const GDBusInterfaceVTable interface_vtable_;
};

} // namespace Bluetooth

#endif // GATT_SERVICE_H