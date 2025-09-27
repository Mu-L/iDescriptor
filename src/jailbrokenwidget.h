#ifndef JAILBROKENWIDGET_H
#define JAILBROKENWIDGET_H

#include "iDescriptor.h"
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QWidget>
#include <qtermwidget6/qtermwidget.h>

class JailbrokenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit JailbrokenWidget(QWidget *parent = nullptr);
    ~JailbrokenWidget();
    void initWidget();

private slots:
    void deviceConnected(iDescriptorDevice *device);
    void onConnectSSH();
    void startSSH();
    void onSshProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupTerminal();
    void connectTerminalToProcess();

    QLabel *m_infoLabel;
    iDescriptorDevice *m_device = nullptr;
    QProcess *iproxyProcess = nullptr;
    QProcess *sshProcess = nullptr;

    // Terminal widgets
    QTermWidget *m_terminal;
    QPushButton *m_connectButton;

    bool m_isInitialized = false;
    bool m_sshConnected = false;
};

#endif // JAILBROKENWIDGET_H