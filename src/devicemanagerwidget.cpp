#include "devicemanagerwidget.h"
#include "appcontext.h"
#include "devicemenuwidget.h"
#include <QDebug>

DeviceManagerWidget::DeviceManagerWidget(QWidget *parent)
    : QWidget(parent), m_currentDeviceUuid("")
{
    setupUI();

    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            [this](iDescriptorDevice *device) {
                addDevice(device);
                setCurrentDevice(device->udid);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const std::string &uuid) {
                removeDevice(uuid);
                emit updateNoDevicesConnected();
            });

    // TODO: doesnt seem to work
    // connect(
    //     AppContext::sharedInstance(), &AppContext::devicePairPending, this,
    //     [this](const QString &udid) {
    //         QWidget *placeholderWidget = new QWidget();
    //         QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
    //         QLabel *label = new QLabel(
    //             "Device is not paired. Please pair the device to continue.");
    //         label->setAlignment(Qt::AlignCenter);
    //         layout->addWidget(label);
    //         placeholderWidget->setLayout(layout);
    //         m_device_menu_widgets[udid.toStdString()] = placeholderWidget;

    //         QString tabTitle = QString::fromStdString(udid.toStdString());
    //         int mostRecentDevice =
    //             m_deviceManager->addDevice(placeholderWidget, tabTitle);
    //         m_deviceManager->setCurrentDevice(mostRecentDevice);
    //         ui->stackedWidget->setCurrentIndex(1); // Show device list page
    //     });

    // TODO: could use some refactoring
    // connect(AppContext::sharedInstance(), &AppContext::devicePaired, this,
    //         [this](iDescriptorDevice *device) {
    //             qDebug() << "Device paired:"
    //                      << QString::fromStdString(device->udid);

    //             DeviceMenuWidget *deviceWidget = new
    //             DeviceMenuWidget(device);

    //             QString tabTitle =
    //                 QString::fromStdString(device->deviceInfo.productType);

    //             int mostRecentDevice =
    //                 addDevice(deviceWidget, tabTitle);
    //             setCurrentDevice(mostRecentDevice);
    //             // Makes sense ?
    //             // emit updateNoDevicesConnected()

    //             // Clean up old mapping and update
    //             if (m_device_menu_widgets.count(device->udid)) {
    //                 m_device_menu_widgets[device->udid]->deleteLater();
    //             }
    //             m_device_menu_widgets[device->udid] = deviceWidget;
    //         });

    // connect(AppContext::sharedInstance(), &AppContext::recoveryDeviceRemoved,
    //         this, [this](const QString &ecid) {
    //             qDebug() << "Removing:" << ecid;
    //             std::string ecidStr = ecid.toStdString();
    //             DeviceMenuWidget *deviceWidget =
    //                 qobject_cast<DeviceMenuWidget *>(
    //                     m_device_menu_widgets[ecidStr]);

    //             if (deviceWidget) {
    //                 // TODO: Implement proper removal by device index
    //                 m_device_menu_widgets.erase(ecidStr);
    //                 delete deviceWidget;
    //             }
    //             emit updateNoDevicesConnected();
    //         });
}

void DeviceManagerWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create sidebar
    m_sidebar = new DeviceSidebarWidget();

    // Create stacked widget for device content
    m_stackedWidget = new QStackedWidget();

    // Add to layout
    m_mainLayout->addWidget(m_sidebar);
    m_mainLayout->addWidget(m_stackedWidget,
                            1); // Give stacked widget more space

    // Connect signals
    connect(m_sidebar, &DeviceSidebarWidget::sidebarDeviceChanged, this,
            &DeviceManagerWidget::onSidebarDeviceChanged);
    connect(m_sidebar, &DeviceSidebarWidget::sidebarNavigationChanged, this,
            &DeviceManagerWidget::onSidebarNavigationChanged);
}

int DeviceManagerWidget::addDevice(iDescriptorDevice *device)
{

    qDebug() << "Connect ::deviceAdded Adding:"
             << QString::fromStdString(device->udid);

    DeviceMenuWidget *deviceWidget = new DeviceMenuWidget(device, this);
    QString tabTitle = QString::fromStdString(device->deviceInfo.productType);

    int deviceIndex = m_stackedWidget->addWidget(deviceWidget);
    m_deviceWidgets[device->udid] = std::pair{
        deviceWidget, m_sidebar->addToSidebar(tabTitle, device->udid)};

    // If this is the first device, make it current
    // if (m_currentDeviceIndex == -1) {
    //     setCurrentDevice(deviceIndex);
    // }

    return deviceIndex;
}

void DeviceManagerWidget::removeDevice(const std::string &uuid)
{

    qDebug() << "Removing:" << QString::fromStdString(uuid);
    std::pair<DeviceMenuWidget *, DeviceSidebarItem *> &d =
        m_deviceWidgets[uuid];

    if (d.first != nullptr && d.second != nullptr) {
        // TODO: cleanups
        m_deviceWidgets.remove(uuid);
        delete d.first;
        delete d.second;

        if (m_deviceWidgets.count() > 0) {
            setCurrentDevice(m_deviceWidgets.firstKey());
            m_sidebar->updateSidebar(m_deviceWidgets.firstKey());
        }
    }
}

void DeviceManagerWidget::setCurrentDevice(const std::string &uuid)
{
    qDebug() << "Setting current device to:" << QString::fromStdString(uuid);
    if (m_currentDeviceUuid == uuid)
        return;

    if (!m_deviceWidgets.contains(uuid)) {
        qWarning() << "Device UUID not found:" << QString::fromStdString(uuid);
        return;
    }

    // m_currentDeviceIndex = deviceIndex;
    m_currentDeviceUuid = uuid;

    // // Update sidebar selection
    // m_sidebar->setCurrentDevice(deviceIndex);

    // // Update stacked widget
    QWidget *widget = m_deviceWidgets[uuid].first;
    m_stackedWidget->setCurrentWidget(widget);

    emit deviceChanged(uuid);
}

std::string DeviceManagerWidget::getCurrentDevice() const
{
    return m_currentDeviceUuid;
}

QWidget *DeviceManagerWidget::getDeviceWidget(int deviceIndex) const
{
    // return m_deviceWidgets.value(deviceIndex, nullptr);
}

void DeviceManagerWidget::setDeviceNavigation(int deviceIndex,
                                              const QString &section)
{
    m_sidebar->setDeviceNavigationSection(deviceIndex, section);
    // emit deviceNavigationChanged(deviceIndex, section);
}

void DeviceManagerWidget::onSidebarDeviceChanged(std::string deviceUuid)
{
    setCurrentDevice(deviceUuid);
}

void DeviceManagerWidget::onSidebarNavigationChanged(std::string deviceUuid,
                                                     const QString &section)
{
    if (deviceUuid != m_currentDeviceUuid) {
        setCurrentDevice(deviceUuid);
    }

    QWidget *tabWidget = m_deviceWidgets[deviceUuid].first;
    DeviceMenuWidget *deviceMenuWidget =
        qobject_cast<DeviceMenuWidget *>(tabWidget);

    if (deviceMenuWidget) {
        // Call a method to change the internal tab
        deviceMenuWidget->switchToTab(section);
    }
    // if (deviceIndex != m_currentDeviceIndex) {
    //     setCurrentDevice(deviceIndex);
    // }
    // emit sidebarNavigationChanged(deviceUuid, section);
}
