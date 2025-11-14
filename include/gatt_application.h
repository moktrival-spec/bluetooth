#ifndef GATT_APPLICATION_H
#define GATT_APPLICATION_H

#include <gio/gio.h>
#include <vector>
#include <memory>
#include <string>

namespace Bluetooth {

class GattService;

/**
 * @brief GATT应用基类
 * 实现org.bluez.GattApplication1 D-Bus接口
 */
class GattApplication {
public:
    explicit GattApplication(const std::string& object_path = "/org/bluez/example/gatt");
    virtual ~GattApplication();

    // 禁用拷贝构造和赋值
    GattApplication(const GattApplication&) = delete;
    GattApplication& operator=(const GattApplication&) = delete;

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
     * @brief 添加GATT服务
     * @param service GATT服务实例
     * @return true表示成功，false表示失败
     */
    bool addService(std::shared_ptr<GattService> service);

    /**
     * @brief 获取所有服务
     * @return 服务列表
     */
    const std::vector<std::shared_ptr<GattService>>& getServices() const { return services_; }

    /**
     * @brief 获取对象路径
     * @return D-Bus对象路径
     */
    const std::string& getObjectPath() const { return object_path_; }

    /**
     * @brief 获取D-Bus连接
     * @return GDBusConnection指针
     */
    GDBusConnection* getConnection() const { return connection_; }

protected:
    /**
     * @brief D-Bus方法处理：获取服务
     */
    virtual GVariant* handleGetServices();

private:
    std::string object_path_;
    GDBusConnection* connection_;
    guint registration_id_;
    std::vector<std::shared_ptr<GattService>> services_;

    // D-Bus接口vtable
    static GVariant* methodGetServices(GDBusConnection* connection,
                                      const gchar* sender,
                                      const gchar* object_path,
                                      const gchar* interface_name,
                                      const gchar* method_name,
                                      GVariant* parameters,
                                      gpointer user_data,
                                      GDBusMethodInvocation* invocation);

    // D-Bus接口定义
    static const GDBusInterfaceVTable interface_vtable_;
    static const GDBusArgInfo method_get_services_args[];
    static const GDBusMethodInfo interface_methods[];
    static const GDBusInterfaceInfo interface_info_;
};

} // namespace Bluetooth

#endif // GATT_APPLICATION_H