#ifndef JAILBROKENWIDGET_H
#define JAILBROKENWIDGET_H

#ifdef __linux__
#include "core/services/avahi/avahi_service.h"
#else
#include "core/services/dnssd/dnssd_service.h"
#endif

#include "iDescriptor.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <libssh/libssh.h>

class QTermWidget;

enum class DeviceType { None, Wired, Wireless };

class JailbrokenWidget : public QWidget
{
    Q_OBJECT

public:
    JailbrokenWidget(QWidget *parent = nullptr);
    ~JailbrokenWidget();

private slots:
    void onConnectSSH();
    void checkSshData();
    void onWiredDeviceAdded(iDescriptorDevice *device);
    void onWiredDeviceRemoved(const std::string &udid);
    void onWirelessDeviceAdded(const NetworkDevice &device);
    void onWirelessDeviceRemoved(const QString &deviceName);
    void onDeviceSelected(QAbstractButton *button);

private:
    void setupTerminal();
    void setupDeviceSelectionUI(QVBoxLayout *layout);
    void updateDeviceList();
    void clearDeviceButtons();
    void addWiredDevice(iDescriptorDevice *device);
    void addWirelessDevice(const NetworkDevice &device);
    void resetSelection();

    void initWiredDevice();
    void initWirelessDevice();
    void startSSH(const QString &host, uint16_t port);
    void disconnectSSH();
    void connectLibsshToTerminal();
    void deviceConnected(iDescriptorDevice *device);

    QTermWidget *m_terminal;
    QLabel *m_infoLabel;
    QPushButton *m_connectButton;

    // Device selection UI
    QVBoxLayout *m_deviceLayout;
    QGroupBox *m_wiredDevicesGroup;
    QGroupBox *m_wirelessDevicesGroup;
    QVBoxLayout *m_wiredDevicesLayout;
    QVBoxLayout *m_wirelessDevicesLayout;
    QButtonGroup *m_deviceButtonGroup;

#ifdef Q_OS_LINUX
    AvahiService *m_wirelessProvider = nullptr;
#endif

#ifdef __APPLE__
    DnssdService *m_wirelessProvider = nullptr;
#endif

    DeviceType m_selectedDeviceType = DeviceType::None;
    iDescriptorDevice *m_selectedWiredDevice = nullptr;
    NetworkDevice m_selectedNetworkDevice;

    // Legacy device pointer (kept for compatibility)
    iDescriptorDevice *m_device = nullptr;

    // SSH components
    ssh_session m_sshSession;
    ssh_channel m_sshChannel;
    QTimer *m_sshTimer;
    QProcess *iproxyProcess = nullptr;

    bool m_sshConnected = false;
    bool m_isInitialized = false;
};

#endif // JAILBROKENWIDGET_H