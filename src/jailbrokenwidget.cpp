#include "jailbrokenwidget.h"
#include "appcontext.h"

#ifdef __linux__
#include "core/services/avahi/avahi_service.h"
#else
#include "core/services/dnssd/dnssd_service.h"
#endif

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QButtonGroup>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <libssh/libssh.h>
#include <qtermwidget6/qtermwidget.h>
#include <unistd.h>

JailbrokenWidget::JailbrokenWidget(QWidget *parent) : QWidget{parent}
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    QGraphicsScene *scene = new QGraphicsScene(this);
    QGraphicsPixmapItem *pixmapItem =
        new QGraphicsPixmapItem(QPixmap(":/resources/iphone.png"));
    scene->addItem(pixmapItem);

    QGraphicsView *graphicsView = new ResponsiveGraphicsView(scene, this);
    graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setMinimumWidth(200);
    graphicsView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    graphicsView->setStyleSheet("background:transparent; border: none;");

    mainLayout->addWidget(graphicsView, 1);

    // Connect to AppContext for device events
    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            &JailbrokenWidget::onWiredDeviceAdded);
    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            &JailbrokenWidget::onWiredDeviceRemoved);

#ifdef __linux__
    m_wirelessProvider = new AvahiService(this);
    connect(m_wirelessProvider, &AvahiService::deviceAdded, this,
            &JailbrokenWidget::onWirelessDeviceAdded);
    connect(m_wirelessProvider, &AvahiService::deviceRemoved, this,
            &JailbrokenWidget::onWirelessDeviceRemoved);
#else
    m_wirelessProvider = new DnssdService(this);
    connect(m_wirelessProvider, &DnssdService::deviceAdded, this,
            &JailbrokenWidget::onWirelessDeviceAdded);
    connect(m_wirelessProvider, &DnssdService::deviceRemoved, this,
            &JailbrokenWidget::onWirelessDeviceRemoved);
#endif

    // Right side: Device selection and Terminal
    QWidget *rightContainer = new QWidget();
    rightContainer->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
    rightContainer->setMinimumWidth(400);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(15, 15, 15, 15);
    rightLayout->setSpacing(10);

    setupDeviceSelectionUI(rightLayout);
    setupTerminal();
    rightLayout->addWidget(m_terminal, 1);

    mainLayout->addWidget(rightContainer, 3);

    // Initialize SSH
    ssh_init();
    m_sshSession = nullptr;
    m_sshChannel = nullptr;

    // Setup timer for checking SSH data
    m_sshTimer = new QTimer(this);
    connect(m_sshTimer, &QTimer::timeout, this,
            &JailbrokenWidget::checkSshData);

    // Start scanning for wireless devices
    m_wirelessProvider->startBrowsing();

    // Populate initial devices
    updateDeviceList();
}

void JailbrokenWidget::setupTerminal()
{
    m_terminal = new QTermWidget(0, this);
    m_terminal->setMinimumHeight(400);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setColorScheme("Linux");
    m_terminal->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_terminal, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                QMenu menu(this);
                QList<QAction *> actions = m_terminal->filterActions(pos);
                if (!actions.isEmpty()) {
                    menu.addActions(actions);
                    menu.exec(m_terminal->mapToGlobal(pos));
                }
            });
    m_terminal->startTerminalTeletype();
    m_terminal->hide();
    m_terminal->setStyleSheet("padding : 10px;");
}

void JailbrokenWidget::setupDeviceSelectionUI(QVBoxLayout *layout)
{
    // Create scroll area for device selection
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(200);
    scrollArea->setMaximumHeight(300);

    QWidget *scrollContent = new QWidget();
    m_deviceLayout = new QVBoxLayout(scrollContent);
    m_deviceLayout->setContentsMargins(5, 5, 5, 5);
    m_deviceLayout->setSpacing(10);

    // Button group for device selection
    m_deviceButtonGroup = new QButtonGroup(this);
    connect(m_deviceButtonGroup,
            QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &JailbrokenWidget::onDeviceSelected);

    // Wired devices group
    m_wiredDevicesGroup = new QGroupBox("Connected Devices");
    m_wiredDevicesLayout = new QVBoxLayout(m_wiredDevicesGroup);
    m_deviceLayout->addWidget(m_wiredDevicesGroup);

    // Wireless devices group
    m_wirelessDevicesGroup = new QGroupBox("Network Devices");
    m_wirelessDevicesLayout = new QVBoxLayout(m_wirelessDevicesGroup);
    m_deviceLayout->addWidget(m_wirelessDevicesGroup);

    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    // Info and connect button
    m_infoLabel = new QLabel("Select a device to connect");
    layout->addWidget(m_infoLabel);

    m_connectButton = new QPushButton("Connect SSH Terminal");
    m_connectButton->setEnabled(false);
    connect(m_connectButton, &QPushButton::clicked, this,
            &JailbrokenWidget::onConnectSSH);
    layout->addWidget(m_connectButton);
}

void JailbrokenWidget::updateDeviceList()
{
    // Clear existing devices
    clearDeviceButtons();

    // Add wired devices
    QList<iDescriptorDevice *> wiredDevices =
        AppContext::sharedInstance()->getAllDevices();
    for (iDescriptorDevice *device : wiredDevices) {
        addWiredDevice(device);
    }

    // Add wireless devices
    QList<NetworkDevice> wirelessDevices =
        m_wirelessProvider->getNetworkDevices();
    for (const NetworkDevice &device : wirelessDevices) {
        addWirelessDevice(device);
    }
}

void JailbrokenWidget::clearDeviceButtons()
{
    // Remove all buttons from button group and layouts
    for (QAbstractButton *button : m_deviceButtonGroup->buttons()) {
        m_deviceButtonGroup->removeButton(button);
        button->deleteLater();
    }

    // Clear layouts
    QLayoutItem *item;
    while ((item = m_wiredDevicesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    while ((item = m_wirelessDevicesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void JailbrokenWidget::addWiredDevice(iDescriptorDevice *device)
{
    QString deviceName = QString::fromStdString(device->deviceInfo.deviceName);
    QString udid = QString::fromStdString(device->udid);
    QString displayText = QString("%1\n%2").arg(deviceName, udid);

    QRadioButton *radioButton = new QRadioButton(displayText);
    radioButton->setProperty("deviceType", "wired");
    radioButton->setProperty("devicePointer",
                             QVariant::fromValue(static_cast<void *>(device)));
    radioButton->setProperty("udid", udid);

    m_deviceButtonGroup->addButton(radioButton);
    m_wiredDevicesLayout->addWidget(radioButton);
}

void JailbrokenWidget::addWirelessDevice(const NetworkDevice &device)
{
    QString displayText = QString("%1\n%2").arg(device.name, device.address);

    QRadioButton *radioButton = new QRadioButton(displayText);
    radioButton->setProperty("deviceType", "wireless");
    radioButton->setProperty("deviceAddress", device.address);
    radioButton->setProperty("deviceName", device.name);
    radioButton->setProperty("devicePort", device.port);

    m_deviceButtonGroup->addButton(radioButton);
    m_wirelessDevicesLayout->addWidget(radioButton);
}

void JailbrokenWidget::onWiredDeviceAdded(iDescriptorDevice *device)
{
    addWiredDevice(device);
}

void JailbrokenWidget::onWiredDeviceRemoved(const std::string &udid)
{
    QString qudid = QString::fromStdString(udid);

    // Find and remove the corresponding radio button
    for (QAbstractButton *button : m_deviceButtonGroup->buttons()) {
        if (button->property("deviceType").toString() == "wired" &&
            button->property("udid").toString() == qudid) {
            m_deviceButtonGroup->removeButton(button);
            button->deleteLater();
            break;
        }
    }

    // Reset selection if this device was selected
    if (m_selectedDeviceType == DeviceType::Wired && m_selectedWiredDevice &&
        m_selectedWiredDevice->udid == udid) {
        resetSelection();
    }
}

void JailbrokenWidget::onWirelessDeviceAdded(const NetworkDevice &device)
{
    addWirelessDevice(device);
}

void JailbrokenWidget::onWirelessDeviceRemoved(const QString &deviceName)
{
    // Find and remove the corresponding radio button
    for (QAbstractButton *button : m_deviceButtonGroup->buttons()) {
        if (button->property("deviceType").toString() == "wireless" &&
            button->property("deviceName").toString() == deviceName) {
            m_deviceButtonGroup->removeButton(button);
            button->deleteLater();
            break;
        }
    }

    // Reset selection if this device was selected
    if (m_selectedDeviceType == DeviceType::Wireless &&
        m_selectedNetworkDevice.name == deviceName) {
        resetSelection();
    }
}

void JailbrokenWidget::onDeviceSelected(QAbstractButton *button)
{
    QString deviceType = button->property("deviceType").toString();

    if (deviceType == "wired") {
        m_selectedDeviceType = DeviceType::Wired;
        m_selectedWiredDevice = static_cast<iDescriptorDevice *>(
            button->property("devicePointer").value<void *>());

        if (m_selectedWiredDevice->deviceInfo.jailbroken) {
            m_infoLabel->setText("Jailbroken device selected");
        } else {
            m_infoLabel->setText("Device selected (jailbreak status unknown)");
        }
    } else if (deviceType == "wireless") {
        m_selectedDeviceType = DeviceType::Wireless;
        m_selectedNetworkDevice.name =
            button->property("deviceName").toString();
        m_selectedNetworkDevice.address =
            button->property("deviceAddress").toString();
        m_selectedNetworkDevice.port = button->property("devicePort").toUInt();

        m_infoLabel->setText(
            "Network device selected (jailbreak status unknown)");
    }

    m_connectButton->setEnabled(true);
    m_connectButton->setText("Connect SSH Terminal");
}

void JailbrokenWidget::resetSelection()
{
    m_selectedDeviceType = DeviceType::None;
    m_selectedWiredDevice = nullptr;
    m_selectedNetworkDevice = NetworkDevice{};
    m_connectButton->setEnabled(false);
    m_infoLabel->setText("Select a device to connect");

    // Uncheck all radio buttons
    if (m_deviceButtonGroup->checkedButton()) {
        m_deviceButtonGroup->setExclusive(false);
        m_deviceButtonGroup->checkedButton()->setChecked(false);
        m_deviceButtonGroup->setExclusive(true);
    }
}

void JailbrokenWidget::connectLibsshToTerminal()
{
    if (!m_terminal)
        return;

    // Connect terminal input to SSH channel
    connect(m_terminal, &QTermWidget::sendData, this,
            [this](const char *data, int size) {
                if (m_sshChannel && ssh_channel_is_open(m_sshChannel)) {
                    ssh_channel_write(m_sshChannel, data, size);
                }
            });
}

void JailbrokenWidget::deviceConnected(iDescriptorDevice *device)
{
    if (device->deviceInfo.jailbroken) {
        m_infoLabel->setText("Jailbroken device connected");
    } else {
        m_infoLabel->setText(
            "Connected device is not detected as jailbroken. Continue anyway?");
    }
    m_device = device;
    m_connectButton->setEnabled(true);
    m_connectButton->setText("Connect SSH Terminal");
}

void JailbrokenWidget::onConnectSSH()
{
    if (m_sshConnected) {
        disconnectSSH();
        return;
    }

    if (m_selectedDeviceType == DeviceType::None) {
        m_infoLabel->setText("Please select a device first");
        return;
    }

    if (m_selectedDeviceType == DeviceType::Wired) {
        initWiredDevice();
    } else {
        initWirelessDevice();
    }
}

void JailbrokenWidget::initWiredDevice()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    if (!m_selectedWiredDevice) {
        m_infoLabel->setText("No wired device selected");
        return;
    }

    m_connectButton->setEnabled(false);
    m_infoLabel->setText("Setting up SSH tunnel...");

    // Start iproxy first for wired devices
    iproxyProcess = new QProcess(this);
    iproxyProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Add common directories where iproxy might be installed
    env.insert("PATH", env.value("PATH") + ":/usr/local/bin:/opt/homebrew/bin");

    iproxyProcess->setProcessEnvironment(env);

    connect(iproxyProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                m_infoLabel->setText("Error: " + iproxyProcess->errorString());
                m_connectButton->setEnabled(true);
                qDebug() << "iproxy error:" << error;
            });

    // Monitor iproxy output for readiness
    connect(iproxyProcess, &QProcess::readyRead, this, [this]() {
        QByteArray output = iproxyProcess->readAll();
        if (output.contains("waiting for connection")) {
            // iproxy is ready, disconnect the signal to avoid multiple calls
            disconnect(iproxyProcess, &QProcess::readyRead, this, nullptr);
            startSSH("127.0.0.1", 3333);
        }
    });

    QStringList args;
    args << "-u" << m_selectedWiredDevice->udid.c_str() << "3333" << "22";

    qDebug() << "Starting iproxy with args:" << args;

    iproxyProcess->start("iproxy", args);

    // Check if iproxy started successfully
    if (!iproxyProcess->waitForStarted(5000)) {
        m_infoLabel->setText("Failed to start iproxy");
        m_connectButton->setEnabled(true);
        return;
    }
}

void JailbrokenWidget::initWirelessDevice()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    m_connectButton->setEnabled(false);
    m_infoLabel->setText("Connecting to network device...");

    // For wireless devices, connect directly without iproxy
    startSSH(m_selectedNetworkDevice.address, m_selectedNetworkDevice.port);
}

void JailbrokenWidget::startSSH(const QString &host, uint16_t port)
{
    if (m_sshConnected)
        return;

    m_infoLabel->setText("Connecting to SSH server...");
    qDebug() << "Starting SSH connection to" << host << ":" << port;

    // Create SSH session
    m_sshSession = ssh_new();
    if (!m_sshSession) {
        m_infoLabel->setText("Error: Failed to create SSH session");
        m_connectButton->setEnabled(true);
        return;
    }

    // Configure SSH session
    QByteArray hostBytes = host.toUtf8();
    ssh_options_set(m_sshSession, SSH_OPTIONS_HOST, hostBytes.constData());
    int sshPort = static_cast<int>(port);
    ssh_options_set(m_sshSession, SSH_OPTIONS_PORT, &sshPort);
    ssh_options_set(m_sshSession, SSH_OPTIONS_USER, "root");

    // Disable strict host key checking
    int stricthostcheck = 0;
    ssh_options_set(m_sshSession, SSH_OPTIONS_STRICTHOSTKEYCHECK,
                    &stricthostcheck);

    // Set log level for debugging
    int log_level = SSH_LOG_PROTOCOL;
    ssh_options_set(m_sshSession, SSH_OPTIONS_LOG_VERBOSITY, &log_level);

    qDebug() << "SSH session configured, attempting connection...";

    // Connect to SSH server
    int rc = ssh_connect(m_sshSession);
    qDebug() << "SSH connect result:" << rc << "SSH_OK:" << SSH_OK;
    if (rc != SSH_OK) {
        QString errorMsg = QString("SSH connection failed: %1")
                               .arg(ssh_get_error(m_sshSession));
        m_infoLabel->setText(errorMsg);
        qDebug() << errorMsg;
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    qDebug() << "SSH connected successfully, attempting authentication...";

    // Authenticate with password
    rc = ssh_userauth_password(m_sshSession, nullptr, "alpine");
    if (rc != SSH_AUTH_SUCCESS) {
        m_infoLabel->setText(QString("SSH authentication failed: %1")
                                 .arg(ssh_get_error(m_sshSession)));
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Create SSH channel
    m_sshChannel = ssh_channel_new(m_sshSession);
    if (!m_sshChannel) {
        m_infoLabel->setText("Error: Failed to create SSH channel");
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Open SSH channel
    rc = ssh_channel_open_session(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText(QString("Failed to open SSH channel: %1")
                                 .arg(ssh_get_error(m_sshSession)));
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Request a PTY
    rc = ssh_channel_request_pty(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText("Failed to request PTY");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Start shell
    rc = ssh_channel_request_shell(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText("Failed to start shell");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Show terminal and connect to libssh
    m_terminal->show();
    connectLibsshToTerminal();

    // Start timer to check for SSH data
    m_sshTimer->start(50); // Check every 50ms

    m_sshConnected = true;
    m_connectButton->setEnabled(true);
    m_connectButton->setText("Disconnect SSH");
    m_infoLabel->setText("SSH terminal connected");

    // Set focus to terminal
    m_terminal->setFocus();
}

void JailbrokenWidget::checkSshData()
{
    if (!m_sshChannel || !ssh_channel_is_open(m_sshChannel))
        return;

    // Check if SSH channel has data to read
    if (ssh_channel_poll(m_sshChannel, 0) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 0);
        if (nbytes > 0) {
            // Write data to terminal's PTY
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
        }
    }

    // Check for stderr data
    if (ssh_channel_poll(m_sshChannel, 1) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 1);
        if (nbytes > 0) {
            // Write stderr data to terminal's PTY
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
        }
    }

    // Check if channel is closed
    if (ssh_channel_is_eof(m_sshChannel)) {
        disconnectSSH();
    }
}

void JailbrokenWidget::disconnectSSH()
{
    if (m_sshTimer) {
        m_sshTimer->stop();
    }

    if (m_sshChannel) {
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
    }

    if (m_sshSession) {
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
    }

    if (iproxyProcess) {
        iproxyProcess->terminate(); // Ask it to terminate nicely
        if (!iproxyProcess->waitForFinished(1000)) { // Wait 1 sec
            iproxyProcess->kill();                   // Forcefully kill it
            iproxyProcess->waitForFinished(1000);    // Wait for it to die
        }
        delete iproxyProcess; // Avoid memory leak
        iproxyProcess = nullptr;
    }

    m_terminal->hide();
    m_connectButton->setText("Connect SSH Terminal");
    m_infoLabel->setText("SSH disconnected");
    m_sshConnected = false;
    m_isInitialized = false;
    m_connectButton->setEnabled(true);
}

JailbrokenWidget::~JailbrokenWidget() { disconnectSSH(); }
