#ifndef ADVERTISEMENT_MANAGER_H
#define ADVERTISEMENT_MANAGER_H

#include <gio/gio.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <map>

namespace Bluetooth {

// 广告类型
enum class AdvertisementType {
    PERIPHERAL = 0x00,
    BROADCAST = 0x01
};

/**
 * @brief 蓝牙LE广告管理器
 * 实现org.bluez.LEAdvertisement1 D-Bus接口
 */
class AdvertisementManager {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    AdvertisementManager(const std::string& object_path = "/org/bluez/example/advertisement",
                        AdvertisementType type = AdvertisementType::PERIPHERAL);
    virtual ~AdvertisementManager();

    // 禁用拷贝构造和赋值
    AdvertisementManager(const AdvertisementManager&) = delete;
    AdvertisementManager& operator=(const AdvertisementManager&) = delete;

    /**
     * @brief 导出D-Bus接口
     * @param connection D-Bus连接
     * @return true表示成功，false表示失败
     */
    bool exportInterface(GDBusConnection* connection);

    /**
     * @brief 取消导出D-Bus接口
     */
    void unexportInterface();

    /**
     * @brief 设置设备名称
     * @param name 设备名称
     */
    void setDeviceName(const std::string& name);

    /**
     * @brief 设置制造商数据
     * @param company_id 制造商ID
     * @param data 制造商数据
     */
    void setManufacturerData(uint16_t company_id, const std::vector<uint8_t>& data);

    /**
     * @brief 设置服务UUID
     * @param service_uuids 服务UUID列表
     */
    void setServiceUUIDs(const std::vector<std::string>& service_uuids);

    /**
     * @brief 设置服务数据
     * @param service_uuid 服务UUID
     * @param data 服务数据
     */
    void setServiceData(const std::string& service_uuid, const std::vector<uint8_t>& data);

    /**
     * @brief 设置包含的传输方式
     * @param discoverable 是否可发现
     * @param connectable 是否可连接
     */
    void setTransportSettings(bool discoverable = true, bool connectable = true);

    /**
     * @brief 设置广播间隔（微秒）
     * @param min_interval 最小间隔
     * @param max_interval 最大间隔
     */
    void setAdvertisingInterval(uint16_t min_interval, uint16_t max_interval);

    /**
     * @brief 获取对象路径
     * @return D-Bus对象路径
     */
    const std::string& getObjectPath() const { return object_path_; }

    /**
     * @brief 释放广告（调用BlueZ的UnregisterAdvertisement方法）
     * @param connection D-Bus连接
     * @param callback 错误回调
     * @return true表示成功，false表示失败
     */
    bool releaseAdvertisement(GDBusConnection* connection, ErrorCallback callback = nullptr);

protected:
    /**
     * @brief D-Bus方法处理：释放广告
     */
    virtual void handleRelease();

private:
    std::string object_path_;
    AdvertisementType type_;
    GDBusConnection* connection_;
    guint registration_id_;

    // 广告属性
    std::string device_name_;
    std::vector<std::string> service_uuids_;
    std::map<std::string, std::vector<uint8_t>> service_data_;
    std::map<uint16_t, std::vector<uint8_t>> manufacturer_data_;
    bool discoverable_;
    bool connectable_;
    uint16_t min_advertising_interval_;
    uint16_t max_advertising_interval_;

    // D-Bus方法处理
    static GVariant* methodRelease(GDBusConnection* connection,
                                  const gchar* sender,
                                  const gchar* object_path,
                                  const gchar* interface_name,
                                  const gchar* method_name,
                                  GVariant* parameters,
                                  gpointer user_data,
                                  GDBusMethodInvocation* invocation);

    // D-Bus属性处理
    static gboolean onGetProperty(GDBusConnection* connection,
                                 const gchar* sender,
                                 const gchar* object_path,
                                 const gchar* interface_name,
                                 const gchar* property_name,
                                 GVariant** value,
                                 GError** error,
                                 gpointer user_data);

    // 辅助函数
    std::vector<std::string> getTypeFlags() const;
    std::vector<std::string> getAppearance() const;
    std::map<std::string, GVariant*> getManufacturerDataVariant() const;
    std::map<std::string, GVariant*> getServiceDataVariant() const;

    // D-Bus接口定义
    static const GDBusMethodInfo interface_methods[];
    static const GDBusPropertyInfo interface_properties[];
    static const GDBusInterfaceInfo interface_info_;
    static const GDBusInterfaceVTable interface_vtable_;
};

/**
 * @brief 广告注册器类
 * 负责向BlueZ注册和注销广告
 */
class AdvertisementRegistrar {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    AdvertisementRegistrar();
    ~AdvertisementRegistrar();

    // 禁用拷贝构造和赋值
    AdvertisementRegistrar(const AdvertisementRegistrar&) = delete;
    AdvertisementRegistrar& operator=(const AdvertisementRegistrar&) = delete;

    /**
     * @brief 注册广告
     * @param connection D-Bus连接
     * @param advertisement 广告实例
     * @param callback 错误回调
     * @return true表示成功，false表示失败
     */
    bool registerAdvertisement(GDBusConnection* connection,
                              AdvertisementManager* advertisement,
                              ErrorCallback callback = nullptr);

    /**
     * @brief 注销广告
     * @param connection D-Bus连接
     * @param advertisement 广告实例
     * @param callback 错误回调
     * @return true表示成功，false表示失败
     */
    bool unregisterAdvertisement(GDBusConnection* connection,
                                AdvertisementManager* advertisement,
                                ErrorCallback callback = nullptr);

private:
    ErrorCallback error_callback_;

    static void onRegisterAdvertisementReply(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

    static void onRegisterAdvertisementError(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
};

} // namespace Bluetooth

#endif // ADVERTISEMENT_MANAGER_H