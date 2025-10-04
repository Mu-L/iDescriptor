#ifndef AVAHI_SERVICE_H
#define AVAHI_SERVICE_H

#include "../../../iDescriptor.h"
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <map>
#include <string>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>

class AvahiService : public QObject
{
    Q_OBJECT

public:
    explicit AvahiService(QObject *parent = nullptr);
    ~AvahiService();

    void startBrowsing();
    void stopBrowsing();
    QList<NetworkDevice> getNetworkDevices() const;

signals:
    void deviceAdded(const NetworkDevice &device);
    void deviceRemoved(const QString &deviceName);

private slots:
    void pollAvahi();

private:
    void initializeAvahi();
    void cleanupAvahi();

    static void clientCallback(AvahiClient *client, AvahiClientState state,
                               void *userdata);
    static void browseCallback(AvahiServiceBrowser *browser,
                               AvahiIfIndex interface, AvahiProtocol protocol,
                               AvahiBrowserEvent event, const char *name,
                               const char *type, const char *domain,
                               AvahiLookupResultFlags flags, void *userdata);
    static void resolveCallback(AvahiServiceResolver *resolver,
                                AvahiIfIndex interface, AvahiProtocol protocol,
                                AvahiResolverEvent event, const char *name,
                                const char *type, const char *domain,
                                const char *host_name,
                                const AvahiAddress *address, uint16_t port,
                                AvahiStringList *txt,
                                AvahiLookupResultFlags flags, void *userdata);

    AvahiSimplePoll *m_simplePoll;
    AvahiClient *m_client;
    AvahiServiceBrowser *m_serviceBrowser;
    QTimer *m_pollTimer;

    mutable QMutex m_devicesMutex;
    QList<NetworkDevice> m_networkDevices;
    bool m_running;
};

#endif // AVAHI_SERVICE_H