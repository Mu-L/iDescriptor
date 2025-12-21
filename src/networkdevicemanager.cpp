#include "networkdevicemanager.h"

NetworkDeviceManager *NetworkDeviceManager::sharedInstance()
{
    static NetworkDeviceManager instance;
    return &instance;
}

NetworkDeviceManager::NetworkDeviceManager(QObject *parent) : QObject{parent}
{

#ifdef __linux__
    m_networkProvider = new AvahiService(this);
    connect(m_networkProvider, &AvahiService::deviceAdded, this,
            &NetworkDeviceManager::deviceAdded);
    connect(m_networkProvider, &AvahiService::deviceRemoved, this,
            &NetworkDeviceManager::deviceRemoved);
#else
    m_networkProvider = new DnssdService(this);
    connect(m_networkProvider, &DnssdService::deviceAdded, this,
            &NetworkDeviceManager::deviceAdded);
    connect(m_networkProvider, &DnssdService::deviceRemoved, this,
            &NetworkDeviceManager::deviceRemoved);
#endif

    // Start scanning for network devices
    m_networkProvider->startBrowsing();
}
