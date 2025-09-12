#ifndef DEVICEMANAGERWIDGET_H
#define DEVICEMANAGERWIDGET_H

#include "devicemenuwidget.h"
#include "devicesidebarwidget.h"
#include "iDescriptor.h"
#include <QHBoxLayout>
#include <QMap>
#include <QStackedWidget>
#include <QWidget>

class DeviceManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceManagerWidget(QWidget *parent = nullptr);

    int addDevice(iDescriptorDevice *device);
    void removeDevice(const std::string &uuid);
    void setCurrentDevice(const std::string &uuid);
    std::string getCurrentDevice() const;

    // Get the device widget at a specific index
    QWidget *getDeviceWidget(int deviceIndex) const;

    // Navigation methods
    void setDeviceNavigation(int deviceIndex, const QString &section);

signals:
    void deviceChanged(std::string deviceUuid);
    void sidebarNavigationChanged(std::string deviceUuid,
                                  const QString &section);
    void updateNoDevicesConnected();
private slots:
    void onSidebarDeviceChanged(std::string deviceUuid);
    void onSidebarNavigationChanged(std::string deviceUuid,
                                    const QString &section);

private:
    void setupUI();

    QHBoxLayout *m_mainLayout;
    DeviceSidebarWidget *m_sidebar;
    QStackedWidget *m_stackedWidget;

    QMap<std::string, std::pair<DeviceMenuWidget *, DeviceSidebarItem *>>
        m_deviceWidgets; // Map to store devices by UDID
    std::string m_currentDeviceUuid;
};

#endif // DEVICEMANAGERWIDGET_H