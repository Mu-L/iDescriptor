/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "ZDownloader.h"
#include "ZUpdater.h"
#include "devicemanagerwidget.h"
#include "iDescriptor.h"
#include <QMainWindow>
// #ifdef ENABLE_RECOVERY_DEVICE_SUPPORT
// #include "libirecovery.h"
// #endif
#include "devicemonitor.h"
#include "ztabwidget.h"
#include <QLabel>
#include <QStackedWidget>

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
    static MainWindow *sharedInstance();
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    ZUpdater *m_updater = nullptr;
public slots:
    void updateNoDevicesConnected();

private:
    void createMenus();

    Ui::MainWindow *ui;
    ZTabWidget *m_ZTabWidget;
    DeviceManagerWidget *m_deviceManager;
    QStackedWidget *m_mainStackedWidget;
    QLabel *m_connectedDeviceCountLabel;
    DeviceMonitorThread *m_deviceMonitor;
};
#endif // MAINWINDOW_H
