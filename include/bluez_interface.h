#ifndef BLUEZ_INTERFACE_H
#define BLUEZ_INTERFACE_H

#include <gio/gio.h>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace Bluetooth {

// BlueZ D-Bus接口路径常量
constexpr const char* BLUEZ_SERVICE = "org.bluez";
constexpr const char* BLUEZ_ADAPTER_PATH = "/org/bluez/hci0";
constexpr const char* GATT_MANAGER_INTERFACE = "org.bluez.GattManager1";
constexpr const char* LE_ADVERTISEMENT_MANAGER_INTERFACE = "org.bluez.LEAdvertisingManager1";
constexpr const char* GATT_SERVICE_INTERFACE = "org.bluez.GattService1";
constexpr const char* GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
constexpr const char* GATT_DESCRIPTOR_INTERFACE = "org.bluez.GattDescriptor1";

// 前向声明
class GattApplication;
class GattService;
class GattCharacteristic;
class AdvertisementManager;

/**
 * @brief BlueZ接口管理器类
 * 负责与BlueZ D-Bus服务通信，管理GATT服务注册和广告
 */
class BluezInterface {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    BluezInterface();
    ~BluezInterface();

    // 禁用拷贝构造和赋值
    BluezInterface(const BluezInterface&) = delete;
    BluezInterface& operator=(const BluezInterface&) = delete;

    /**
     * @brief 初始化D-Bus连接
     * @return true表示成功，false表示失败
     */
    bool initialize();

    /**
     * @brief 注册GATT应用
     * @param application GATT应用实例
     * @param callback 错误回调
     * @return true表示成功，false表示失败
     */
    bool registerApplication(GattApplication* application, ErrorCallback callback = nullptr);

    /**
     * @brief 注销GATT应用
     * @param callback 错误回调
     * @return true表示成功，false表示失败
     */
    bool unregisterApplication(ErrorCallback callback = nullptr);

    /**
     * @brief 获取适配器对象
     * @return GDBusObjectProxy指针
     */
    GDBusObjectProxy* getAdapterProxy() const { return adapter_proxy_; }

    /**
     * @brief 检查BlueZ服务是否可用
     * @return true表示可用，false表示不可用
     */
    bool isBluezAvailable() const;

    /**
     * @brief 启用蓝牙适配器
     * @return true表示成功，false表示失败
     */
    bool powerOnAdapter();

    /**
     * @brief 获取D-Bus连接
     * @return GDBusConnection指针
     */
    GDBusConnection* getConnection() const { return connection_; }

private:
    GDBusConnection* connection_;
    GDBusObjectManager* object_manager_;
    GDBusObjectProxy* adapter_proxy_;
    bool initialized_;
    ErrorCallback error_callback_;

    // D-Bus方法调用回调
    static void onRegisterApplicationReply(GDBusConnection* connection,
                                         const gchar* sender_name,
                                         const gchar* object_path,
                                         const gchar* interface_name,
                                         const gchar* signal_name,
                                         GVariant* parameters,
                                         gpointer user_data);

    static void onRegisterApplicationError(GDBusConnection* connection,
                                         const gchar* sender_name,
                                         const gchar* object_path,
                                         const gchar* interface_name,
                                         const gchar* signal_name,
                                         GVariant* parameters,
                                         gpointer user_data);

    bool setupObjectManager();
    bool findAdapter();
    void onInterfaceAdded(GDBusObjectManager* manager,
                         GDBusObject* object,
                         GDBusInterface* interface);
    void onInterfaceRemoved(GDBusObjectManager* manager,
                           GDBusObject* object,
                           GDBusInterface* interface);

    // 静态回调函数
    static void onInterfaceAddedStatic(GDBusObjectManager* manager,
                                      GDBusObject* object,
                                      GDBusInterface* interface,
                                      gpointer user_data);
    static void onInterfaceRemovedStatic(GDBusObjectManager* manager,
                                        GDBusObject* object,
                                        GDBusInterface* interface,
                                        gpointer user_data);
};

} // namespace Bluetooth

#endif // BLUEZ_INTERFACE_H