#ifndef GATT_CHARACTERISTIC_H
#define GATT_CHARACTERISTIC_H

#include <gio/gio.h>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <cstdint>
#include <algorithm>

namespace Bluetooth {

// GATT特征值标志
enum class CharacteristicFlags {
    READ = 0x0001,
    WRITE = 0x0002,
    WRITE_WITHOUT_RESPONSE = 0x0004,
    SIGNED_WRITE = 0x0008,
    RELIABLE_WRITE = 0x0010,
    NOTIFY = 0x0020,
    INDICATE = 0x0040
};

// 特征值读写回调函数类型
using ReadCallback = std::function<std::vector<uint8_t>(const std::string& device_path)>;
using WriteCallback = std::function<bool(const std::string& device_path, const std::vector<uint8_t>& value)>;
using NotifyCallback = std::function<void(const std::string& device_path, bool subscribing)>;

/**
 * @brief GATT特征值类
 * 实现org.bluez.GattCharacteristic1 D-Bus接口
 */
class GattCharacteristic {
public:
    GattCharacteristic(const std::string& uuid,
                      const std::vector<CharacteristicFlags>& flags,
                      const std::string& object_path_prefix = "/org/bluez/example/characteristic");
    virtual ~GattCharacteristic();

    // 禁用拷贝构造和赋值
    GattCharacteristic(const GattCharacteristic&) = delete;
    GattCharacteristic& operator=(const GattCharacteristic&) = delete;

    /**
     * @brief 导出D-Bus接口
     * @param connection D-Bus连接
     * @param service_path 服务对象路径
     * @return true表示成功，false表示失败
     */
    bool exportInterface(GDBusConnection* connection, const std::string& service_path);

    /**
     * @brief 取消导出D-Bus接口
     */
    void unexportInterface();

    /**
     * @brief 设置初始值
     * @param value 特征值数据
     */
    void setValue(const std::vector<uint8_t>& value);

    /**
     * @brief 获取当前值
     * @return 特征值数据
     */
    const std::vector<uint8_t>& getValue() const { return value_; }

    /**
     * @brief 通知值已更改（用于NOTIFY/INDICATE）
     */
    void notifyValueChanged();

    /**
     * @brief 获取特征值UUID
     * @return 特征值UUID
     */
    const std::string& getUUID() const { return uuid_; }

    /**
     * @brief 获取对象路径
     * @return D-Bus对象路径
     */
    const std::string& getObjectPath() const { return object_path_; }

    /**
     * @brief 获取标志字符串列表
     * @return 标志字符串列表
     */
    std::vector<std::string> getFlags() const;

    /**
     * @brief 设置读取回调
     * @param callback 读取回调函数
     */
    void setReadCallback(ReadCallback callback) { read_callback_ = callback; }

    /**
     * @brief 设置写入回调
     * @param callback 写入回调函数
     */
    void setWriteCallback(WriteCallback callback) { write_callback_ = callback; }

    /**
     * @brief 设置通知回调
     * @param callback 通知回调函数
     */
    void setNotifyCallback(NotifyCallback callback) { notify_callback_ = callback; }

protected:
    /**
     * @brief D-Bus方法处理：读取值
     * @param options 读取选项
     * @return 读取的值
     */
    virtual GVariant* handleReadValue(GVariant* options);

    /**
     * @brief D-Bus方法处理：写入值
     * @param value 要写入的值
     * @param options 写入选项
     * @return true表示成功，false表示失败
     */
    virtual bool handleWriteValue(GVariant* value, GVariant* options);

    /**
     * @brief D-Bus方法处理：开始通知
     * @param device_path 设备路径
     */
    virtual void handleStartNotify(const std::string& device_path);

    /**
     * @brief D-Bus方法处理：停止通知
     * @param device_path 设备路径
     */
    virtual void handleStopNotify(const std::string& device_path);

private:
    std::string uuid_;
    std::vector<CharacteristicFlags> flags_;
    std::string object_path_;
    GDBusConnection* connection_;
    guint registration_id_;
    std::vector<uint8_t> value_;
    bool notifying_;
    std::vector<std::string> notified_devices_;

    // 回调函数
    ReadCallback read_callback_;
    WriteCallback write_callback_;
    NotifyCallback notify_callback_;

    // D-Bus方法和属性处理
    static GVariant* methodReadValue(GDBusConnection* connection,
                                    const gchar* sender,
                                    const gchar* object_path,
                                    const gchar* interface_name,
                                    const gchar* method_name,
                                    GVariant* parameters,
                                    gpointer user_data,
                                    GDBusMethodInvocation* invocation);

    static GVariant* methodWriteValue(GDBusConnection* connection,
                                     const gchar* sender,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* method_name,
                                     GVariant* parameters,
                                     gpointer user_data,
                                     GDBusMethodInvocation* invocation);

    static GVariant* methodStartNotify(GDBusConnection* connection,
                                      const gchar* sender,
                                      const gchar* object_path,
                                      const gchar* interface_name,
                                      const gchar* method_name,
                                      GVariant* parameters,
                                      gpointer user_data,
                                      GDBusMethodInvocation* invocation);

    static GVariant* methodStopNotify(GDBusConnection* connection,
                                     const gchar* sender,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* method_name,
                                     GVariant* parameters,
                                     gpointer user_data,
                                     GDBusMethodInvocation* invocation);

    static gboolean onGetProperty(GDBusConnection* connection,
                                 const gchar* sender,
                                 const gchar* object_path,
                                 const gchar* interface_name,
                                 const gchar* property_name,
                                 GVariant** value,
                                 GError** error,
                                 gpointer user_data);

    // 辅助函数
    void emitPropertyChanged(const std::string& property_name, GVariant* value);
    std::vector<uint8_t> gvariantToBytes(GVariant* variant);
    GVariant* bytesToGvariant(const std::vector<uint8_t>& bytes);

    // D-Bus方法处理器（静态）
    static void methodCallHandler(GDBusConnection* connection,
                                 const gchar* sender,
                                 const gchar* object_path,
                                 const gchar* interface_name,
                                 const gchar* method_name,
                                 GVariant* parameters,
                                 gpointer user_data,
                                 GDBusMethodInvocation* invocation);

    // D-Bus接口定义
    static const GDBusArgInfo method_read_value_args[];
    static const GDBusArgInfo method_read_value_result[];
    static const GDBusArgInfo method_write_value_args[];
    static const GDBusMethodInfo interface_methods[];
    static const GDBusPropertyInfo interface_properties[];
    static const GDBusInterfaceInfo interface_info_;
    static const GDBusInterfaceVTable interface_vtable_;
};

// 辅助函数：将CharacteristicFlags枚举转换为字符串
std::string characteristicFlagsToString(CharacteristicFlags flag);

} // namespace Bluetooth

#endif // GATT_CHARACTERISTIC_H