#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "detailwindow.h"
#include "settingswidget.h"
#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QMessageBox>
#include <QSvgRenderer>
#include <QtSvg>

#include <libimobiledevice/libimobiledevice.h>
#include <stdio.h>
#include <unistd.h>

#include "appswidget.h"
#include "devicemanagerwidget.h"
#include "iDescriptor.h"
#include "libirecovery.h"
#include "toolboxwidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStack>
#include <QVBoxLayout>
#include <QWidget>

#include "appcontext.h"
#include "deviceinfowidget.h"
#include "devicemenuwidget.h"
#include "fileexplorerwidget.h"
#include "jailbrokenwidget.h"
#include "recoverydeviceinfowidget.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>

void handleCallback(const idevice_event_t *event, void *userData)
{
    printf("Device event received: ");

    switch (event->event) {
    case IDEVICE_DEVICE_ADD: {
        /* this should never happen iDescriptor does not support network devices
        but for some reason even though we are only listening for USB devices,
        we still get network devices on macOS*/
        if (event->conn_type == CONNECTION_NETWORK) {
            return;
        }
        qDebug() << "Device added: " << QString::fromUtf8(event->udid);

        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "addDevice", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(event->udid)),
            Q_ARG(idevice_connection_type, event->conn_type),
            Q_ARG(AddType, AddType::Regular));
        break;
    }

    case IDEVICE_DEVICE_REMOVE: {
        QMetaObject::invokeMethod(AppContext::sharedInstance(), "removeDevice",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, QString(event->udid)));
        break;
    }

    case IDEVICE_DEVICE_PAIRED: {
        if (event->conn_type == CONNECTION_NETWORK) {
            warn("Network devices are not supported but a network device was "
                 "received in event listener. Please report this issue.");
            return;
        }
        qDebug() << "Device paired: " << QString::fromUtf8(event->udid);

        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "addDevice", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(event->udid)),
            Q_ARG(idevice_connection_type, event->conn_type),
            Q_ARG(AddType, AddType::Pairing));
        break;
    }
    default:
        qDebug() << "Unhandled event: " << event->event;
    }
    // return;
}

void handleCallbackRecovery(const irecv_device_event_t *event, void *userData)
{

    switch (event->type) {
    case IRECV_DEVICE_ADD:
        qDebug() << "Recovery device added: ";
        // TODO: handle recovery device addition
        //  QMetaObject::invokeMethod(ctx->mainWindow, "onRecoveryDeviceAdded",
        //                            Qt::QueuedConnection,
        //                            Q_ARG(QObject *, new
        //                            RecoveryDeviceInfo(event)));
        break;
    case IRECV_DEVICE_REMOVE:
        qDebug() << "Recovery device removed: ";
        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "onRecoveryDeviceRemoved",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::number(event->device_info->ecid)));
        break;
    default:
        printf("Unhandled recovery event: %d\n", event->type);
    }
}
irecv_device_event_context_t context;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("iDescriptor");

    m_deviceManager = new DeviceManagerWidget(this);
    ui->stackedWidget->insertWidget(1, m_deviceManager);
    connect(m_deviceManager, &DeviceManagerWidget::updateNoDevicesConnected,
            this, &MainWindow::updateNoDevicesConnected);

    // settings button
    QPushButton *settingsButton = new QPushButton();
    settingsButton->setIcon(QIcon::fromTheme("preferences-system"));
    settingsButton->setToolTip("Settings");
    settingsButton->setFlat(true);
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setFixedSize(24, 24);
    connect(settingsButton, &QPushButton::clicked, this, [this]() {
        QDialog settingsDialog(this);
        settingsDialog.setWindowTitle("Settings");
        settingsDialog.setModal(true);
        settingsDialog.resize(400, 300);
        QVBoxLayout *layout = new QVBoxLayout(&settingsDialog);
        SettingsWidget *settingsWidget = new SettingsWidget(&settingsDialog);
        layout->addWidget(settingsWidget);
        settingsDialog.setLayout(layout);
        settingsDialog.exec();
    });
    // ui->centralwidget->layout()->addWidget(settingsButton);
    ui->mainTabWidget->widget(1)->layout()->addWidget(new AppsWidget(this));
    ui->mainTabWidget->widget(2)->layout()->addWidget(new ToolboxWidget(this));
    auto *jailbrokenWidget = new JailbrokenWidget(this);
    ui->mainTabWidget->widget(3)->layout()->addWidget(jailbrokenWidget);

    // TODO: is this a good idea?
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(ui->mainTabWidget, &QTabWidget::currentChanged, this,
                          [this, jailbrokenWidget, connection](int index) {
                              if (index == 3) { // Jailbroken tab
                                  jailbrokenWidget->initWidget();
                                  QObject::disconnect(*connection);
                              }
                          });

    ui->statusbar->addPermanentWidget(settingsButton);
    irecv_error_t res_recovery =
        irecv_device_event_subscribe(&context, handleCallbackRecovery, nullptr);

    if (res_recovery != IRECV_E_SUCCESS) {
        printf("ERROR: Unable to subscribe to recovery device events.\n");
    }

    idevice_error_t res = idevice_event_subscribe(handleCallback, nullptr);
    if (res != IDEVICE_E_SUCCESS) {
        printf("ERROR: Unable to subscribe to device events.\n");
    }
}

void MainWindow::updateNoDevicesConnected()
{
    qDebug() << "Is there no devices connected? "
             << AppContext::sharedInstance()->noDevicesConnected();
    if (AppContext::sharedInstance()->noDevicesConnected())
        return ui->stackedWidget->setCurrentIndex(
            0);                            // Show "No Devices Connected" page
    ui->stackedWidget->setCurrentIndex(1); // Show device list page
}

void MainWindow::onRecoveryDeviceAdded(QObject *recoveryDeviceInfoObj)
{
    if (!recoveryDeviceInfoObj)
        // TODO: handle
        return;
    try {
        ui->stackedWidget->setCurrentIndex(1);
        RecoveryDeviceInfo *device =
            qobject_cast<RecoveryDeviceInfo *>(recoveryDeviceInfoObj);
        if (!device) {
            qDebug() << "Invalid recovery device info object";
            return;
        }
        // IDescriptorInitDeviceResultRecovery initResult=
        // init_idescriptor_recovery_device(deviceInfo);

        // IDescriptorInitDeviceResult initResult =
        // init_idescriptor_device(udid.toStdString().c_str());

        qDebug() << "Recovery device initialized: " << device->ecid;

        std::string added_ecid =
            AppContext::sharedInstance()->addRecoveryDevice(device);

        // Create device info widget
        RecoveryDeviceInfoWidget *recoveryDeviceInfoWidget =
            new RecoveryDeviceInfoWidget(device);
        QPixmap recoveryIcon(16, 16);
        recoveryIcon.fill(Qt::transparent);
        QPainter painter(&recoveryIcon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(255, 59, 48)); // Red for recovery mode
        painter.drawRoundedRect(2, 2, 12, 12, 2, 2);

        // int mostRecentDevice =
        // customTabWidget->addTabWithIcon(recoveryDeviceInfoWidget,
        // recoveryIcon, "Recovery Mode");

        // m_device_menu_widgets[added_ecid] = recoveryDeviceInfoWidget;
        // Get device icon and product type for tab
        // QString tabTitle =
        // QString::fromStdString(device->product.toStdString());
        QString tabTitle = QString::fromStdString("recovery mode device");

        // Add tab with custom icon
        // int mostRecentDevice =
        //     m_deviceManager->addDevice(recoveryDeviceInfoWidget, tabTitle);
        // m_deviceManager->setCurrentDevice(mostRecentDevice);
    } catch (const std::exception &e) {
        qDebug() << "Exception in onDeviceAdded: " << e.what();
        QMessageBox::critical(
            this, "Error",
            "An error occurred while processing device information");
    }
}

void MainWindow::onRecoveryDeviceRemoved(QObject *deviceInfoObj)
{
    auto *info = qobject_cast<RecoveryDeviceInfo *>(deviceInfoObj);
    if (!info)
        return;

    qDebug() << "Recovery device removed: " << info->ecid;

    // TODO: Implement proper device removal in DeviceManagerWidget
    // For now, we'll just log the removal
    qDebug() << "Recovery device cleanup not yet implemented";
}

MainWindow::~MainWindow()
{
    idevice_event_unsubscribe();
    irecv_device_event_unsubscribe(context);
    // TODO:Clean up all devices
    // for (unsigned i = 0; i < idescriptor_devices.size(); ++i)
    // {
    //     cleanDevice(idescriptor_devices.at(i));
    // }
    // idescriptor_devices.clear();
    delete ui;
    sleep(1); // Give some time for cleanup to finish
}

void MainWindow::onDeviceInitFailed(QString udid, lockdownd_error_t err)
{
    QString errorTitle = "Device Connection Error";
    QString errorMessage;

    switch (err) {
    case LOCKDOWN_E_PASSWORD_PROTECTED:
        errorMessage =
            QString(
                "Could not validate device %1 because a passcode is set.\n\n"
                "Please enter the passcode on your device and try again.")
                .arg(udid);
        qDebug() << "ERROR: Could not validate with device" << udid
                 << "because a passcode is set. Please enter the passcode on "
                    "the device and retry.";
        break;
    case LOCKDOWN_E_INVALID_CONF:
    case LOCKDOWN_E_INVALID_HOST_ID:
        errorMessage = QString("Device %1 is not paired with this computer.\n\n"
                               "Please check your device settings.")
                           .arg(udid);
        qDebug() << "ERROR: Device" << udid << "is not paired with this host";
        break;
    case LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING:
        errorMessage =
            QString(
                "Trust dialog is waiting for your response.\n\n"
                "Please accept the trust dialog on the screen of device %1,\n"
                "then attempt to pair again.")
                .arg(udid);
        qDebug()
            << "ERROR: Please accept the trust dialog on the screen of device"
            << udid << ", then attempt to pair again.";
        break;
    case LOCKDOWN_E_USER_DENIED_PAIRING:
        errorMessage = QString("Pairing rejected.\n\n"
                               "You denied the trust dialog on device %1.")
                           .arg(udid);
        qDebug() << "ERROR: Device" << udid
                 << "said that the user denied the trust dialog.";
        break;
    case LOCKDOWN_E_PAIRING_FAILED:
        errorMessage = QString("Pairing with device %1 failed.\n\n"
                               "Please try again or restart your device.")
                           .arg(udid);
        qDebug() << "ERROR: Pairing with device" << udid << "failed.";
        break;
    case LOCKDOWN_E_GET_PROHIBITED:
    case LOCKDOWN_E_PAIRING_PROHIBITED_OVER_THIS_CONNECTION:
        errorMessage = "Pairing is not possible over this connection.\n\n"
                       "Please try using a USB connection.";
        qDebug() << "ERROR: Pairing is not possible over this connection.";
        break;
    default:
        errorMessage = QString("Unknown error occurred with device %1.\n\n"
                               "Error code: %2")
                           .arg(udid)
                           .arg(err);
        qDebug() << "ERROR: Device" << udid << "returned unhandled error code"
                 << err;
        break;
    }
    QMessageBox errorDialog(this);
    errorDialog.setWindowTitle(errorTitle);
    errorDialog.setText(errorMessage);
    errorDialog.setIcon(QMessageBox::Warning);
    errorDialog.setStandardButtons(QMessageBox::Ok);
    errorDialog.exec();
}