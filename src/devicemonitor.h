#ifndef DEVICEMONITOR_H
#define DEVICEMONITOR_H
#include <QDebug>
#include <QThread>
#include <idevice++/bindings.hpp>
#include <idevice++/core_device_proxy.hpp>
#include <idevice++/dvt/remote_server.hpp>
#include <idevice++/dvt/screenshot.hpp>
#include <idevice++/ffi.hpp>
#include <idevice++/provider.hpp>
#include <idevice++/readwrite.hpp>
#include <idevice++/rsd.hpp>
#include <idevice++/usbmuxd.hpp>

using namespace IdeviceFFI;

class DeviceMonitorThread : public QThread
{
    Q_OBJECT
public:
    DeviceMonitorThread(QObject *parent = nullptr) : QThread(parent) {}

    void run() override
    {
        while (!isInterruptionRequested()) {
            // Create connection
            UsbmuxdConnectionHandle *usbmuxd_conn;
            IdeviceFfiError *err =
                idevice_usbmuxd_new_default_connection(0, &usbmuxd_conn);
            if (err) {
                idevice_error_free(err);
                QThread::msleep(1000); // Wait before retry
                continue;
            }

            UsbmuxdListenerHandle *listener;
            err = idevice_usbmuxd_listen(usbmuxd_conn, &listener);
            if (err) {
                idevice_usbmuxd_connection_free(usbmuxd_conn);
                idevice_error_free(err);
                QThread::msleep(1000);
                continue;
            }

            // Monitor loop
            while (!isInterruptionRequested()) {
                bool connect = false;
                UsbmuxdDeviceHandle *device;
                uint32_t disconnect_id;

                err = idevice_usbmuxd_listener_next(listener, &connect, &device,
                                                    &disconnect_id);
                qDebug() << "Listening for device connections..." << err
                         << connect;
                if (err) {
                    // Check if it's just the stream ending (no devices)
                    if (err->code == -1) { // Socket error code
                        qDebug() << "Listener stream ended, reconnecting...";
                        idevice_error_free(err);
                        break; // Break inner loop to reconnect
                    } else {
                        qDebug() << "Real error:" << err->message;
                        idevice_error_free(err);
                        break;
                    }
                }

                if (connect) {
                    const char *udid = idevice_usbmuxd_device_get_udid(device);
                    uint32_t device_id =
                        idevice_usbmuxd_device_get_device_id(device);
                    uint8_t conn_type =
                        idevice_usbmuxd_device_get_connection_type(device);

                    // Skip network devices (same as original callback)
                    if (conn_type == CONNECTION_NETWORK) {
                        qDebug() << "Skipping network device:" << QString(udid);
                        idevice_usbmuxd_device_free(device);
                        continue;
                    }

                    deviceMap[device_id] = QString(udid);
                    qDebug()
                        << "[DeviceMonitor] Device connected:" << QString(udid);

                    emit deviceEvent(IDEVICE_DEVICE_ADD, QString(udid),
                                     conn_type, AddType::Regular);
                    idevice_usbmuxd_device_free(device);
                } else {
                    QString udid = deviceMap.value(disconnect_id, QString());
                    if (!udid.isEmpty()) {
                        emit deviceEvent(IDEVICE_DEVICE_REMOVE, udid,
                                         CONNECTION_USB, AddType::Regular);
                        deviceMap.remove(disconnect_id);
                    } else {
                        qDebug() << "Unknown device disconnected please report "
                                    "this issue: "
                                 << disconnect_id;
                    }
                }
            }

            // Cleanup before reconnecting
            idevice_usbmuxd_listener_handle_free(listener);
            idevice_usbmuxd_connection_free(usbmuxd_conn);
        }
    }

    enum IdeviceEventType {
        IDEVICE_DEVICE_ADD = 1,
        IDEVICE_DEVICE_REMOVE = 2,
        IDEVICE_DEVICE_PAIRED = 3
    };

    enum IdeviceConnectionType { CONNECTION_USB = 1, CONNECTION_NETWORK = 2 };

    enum AddType { Regular = 0, Pairing = 1 };

signals:
    void deviceEvent(int event, const QString &udid, int conn_type,
                     int addType);

private:
    QMap<uint32_t, QString> deviceMap;
};
#endif // DEVICEMONITOR_H