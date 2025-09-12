#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "devicemanagerwidget.h"
#include "devicemenuwidget.h"
#include "iDescriptor.h"
#include "libirecovery.h"
#include <QMainWindow>
#include <libimobiledevice/libimobiledevice.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void deviceAdded(QString udid); // Signal for device connections

public slots:
    void onRecoveryDeviceAdded(
        QObject *device_info); // Slot for recovery device connections
    void onRecoveryDeviceRemoved(
        QObject *device_info); // Slot for recovery device disconnections
    void onDeviceInitFailed(QString udid, lockdownd_error_t err);

private:
    void updateNoDevicesConnected();

    Ui::MainWindow *ui;
    DeviceManagerWidget *m_deviceManager; // Add this member
};
#endif // MAINWINDOW_H
