#ifndef DEVICEINFOWIDGET_H
#define DEVICEINFOWIDGET_H
#include "batterywidget.h"
#include "deviceimagewidget.h"
#include "iDescriptor.h"
#include <QLabel>
#include <QTimer>
#include <QWidget>
class DeviceInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceInfoWidget(iDescriptorDevice *device,
                              QWidget *parent = nullptr);
    ~DeviceInfoWidget(); // added destructor

private slots:
    void onBatteryMoreClicked();

private:
    iDescriptorDevice *m_device;
    QTimer *m_updateTimer;
    void updateBatteryInfo();
    void updateChargingStatusIcon();
    QLabel *m_chargingStatusLabel;
    QLabel *m_chargingWattsWithCableTypeLabel;
    BatteryWidget *m_batteryWidget;
    QLabel *m_lightningIconLabel;

    DeviceImageWidget *m_deviceImageWidget;
};

#endif // DEVICEINFOWIDGET_H
