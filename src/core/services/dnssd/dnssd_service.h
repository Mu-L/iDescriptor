#ifndef DNSSD_SERVICE_H
#define DNSSD_SERVICE_H

#include "../../../iDescriptor.h"
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <map>
#include <string>

#include <dns_sd.h>

class DnssdService : public QObject
{
    Q_OBJECT

public:
    explicit DnssdService(QObject *parent = nullptr);
    ~DnssdService();

    void startBrowsing();
    void stopBrowsing();
    QList<NetworkDevice> getNetworkDevices() const;

signals:
    void deviceAdded(const NetworkDevice &device);
    void deviceRemoved(const QString &deviceName);

private slots:
    void processDnssdEvents();

private:
    void cleanupDnssd();

    static void DNSSD_API browseCallback(
        DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
        DNSServiceErrorType errorCode, const char *serviceName,
        const char *regtype, const char *replyDomain, void *context);

    static void DNSSD_API resolveCallback(
        DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
        DNSServiceErrorType errorCode, const char *fullname,
        const char *hosttarget, uint16_t port, uint16_t txtLen,
        const unsigned char *txtRecord, void *context);

    static void DNSSD_API addrInfoCallback(
        DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
        DNSServiceErrorType errorCode, const char *hostname,
        const struct sockaddr *address, uint32_t ttl, void *context);

    DNSServiceRef m_browseRef;
    QSocketNotifier *m_socketNotifier;

    mutable QMutex m_devicesMutex;
    QList<NetworkDevice> m_networkDevices;
    bool m_running;

    // Temporary storage for devices being resolved
    struct PendingDevice {
        QString name;
        QString hostname;
        uint16_t port;
        uint32_t interfaceIndex;
        QMap<QString, QString> txt;
    };
    QMap<QString, PendingDevice> m_pendingDevices;
};

#endif // DNSSD_SERVICE_H