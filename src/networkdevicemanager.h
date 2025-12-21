#ifndef NETWORKDEVICEMANAGER_H
#define NETWORKDEVICEMANAGER_H

#include <QObject>

#ifdef __linux__
#include "core/services/avahi/avahi_service.h"
#else
#include "core/services/dnssd/dnssd_service.h"
#endif

class NetworkDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkDeviceManager(QObject *parent = nullptr);

#ifdef __linux__
    AvahiService *m_networkProvider = nullptr;
#else
    DnssdService *m_networkProvider = nullptr;
#endif
    static NetworkDeviceManager *sharedInstance();
signals:
    void deviceAdded(const NetworkDevice &device);
    void deviceRemoved(const QString &deviceName);
};

#endif // NETWORKDEVICEMANAGER_H
