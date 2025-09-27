#include "jailbrokenwidget.h"
#include "appcontext.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
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
    graphicsView->setStyleSheet("background: transparent; border: none;");

    mainLayout->addWidget(graphicsView, 1); // Stretch factor 1

    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            [this](iDescriptorDevice *device) { deviceConnected(device); });

    // Right side: Info and Terminal
    QWidget *rightContainer = new QWidget();
    rightContainer->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
    rightContainer->setMinimumWidth(400);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(15, 15, 15, 15);
    rightLayout->setSpacing(10);

    m_infoLabel = new QLabel("Connect a jailbroken device");
    rightLayout->addWidget(m_infoLabel);

    m_connectButton = new QPushButton("Connect SSH Terminal");
    m_connectButton->setEnabled(false);
    connect(m_connectButton, &QPushButton::clicked, this,
            &JailbrokenWidget::onConnectSSH);
    rightLayout->addWidget(m_connectButton);

    setupTerminal();
    rightLayout->addWidget(m_terminal, 1); // Give terminal most of the space

    mainLayout->addWidget(rightContainer, 3); // Stretch factor 3
}

void JailbrokenWidget::setupTerminal()
{
    m_terminal = new QTermWidget(0, this); // 0 = use default shell
    m_terminal->setMinimumHeight(400);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);

    // Set terminal colors and font
    m_terminal->setColorScheme("DarkPastels");

    // Initially hide the terminal
    m_terminal->hide();
}

void JailbrokenWidget::connectTerminalToProcess()
{
    if (!m_terminal || !sshProcess)
        return;

    // Connect terminal input to SSH process stdin
    connect(m_terminal, &QTermWidget::sendData, this,
            [this](const char *data, int size) {
                if (sshProcess && sshProcess->state() == QProcess::Running) {
                    sshProcess->write(data, size);
                }
            });

    // Connect SSH process stdout/stderr to terminal
    connect(sshProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray data = sshProcess->readAllStandardOutput();
        if (m_terminal && !data.isEmpty()) {
            // Write directly to the terminal's PTY
            write(m_terminal->getPtySlaveFd(), data.data(), data.size());
        }
    });

    connect(sshProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray data = sshProcess->readAllStandardError();
        if (m_terminal && !data.isEmpty()) {
            // Write directly to the terminal's PTY
            write(m_terminal->getPtySlaveFd(), data.data(), data.size());
        }
    });
}

void JailbrokenWidget::deviceConnected(iDescriptorDevice *device)
{
    if (device->deviceInfo.jailbroken) {
        m_device = device;
        m_infoLabel->setText("Jailbroken device connected");
        m_connectButton->setEnabled(true);
        m_connectButton->setText("Connect SSH Terminal");
    }
}

void JailbrokenWidget::onConnectSSH()
{
    if (m_sshConnected) {
        // Disconnect SSH
        if (sshProcess) {
            sshProcess->terminate();
            sshProcess->waitForFinished(3000);
            sshProcess = nullptr;
        }
        if (iproxyProcess) {
            iproxyProcess->terminate();
            iproxyProcess->waitForFinished(3000);
            iproxyProcess = nullptr;
        }
        m_terminal->hide();
        m_connectButton->setText("Connect SSH Terminal");
        m_infoLabel->setText("SSH disconnected");
        m_sshConnected = false;
        m_isInitialized = false;
        return;
    }

    initWidget();
}

void JailbrokenWidget::initWidget()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    if (!m_device) {
        m_infoLabel->setText("Device is not jailbroken");
        return;
    }

    m_connectButton->setEnabled(false);
    m_infoLabel->setText("Setting up SSH tunnel...");

    // Start iproxy first
    iproxyProcess = new QProcess(this);
    iproxyProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(iproxyProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                m_infoLabel->setText("Error: " + iproxyProcess->errorString());
                m_connectButton->setEnabled(true);
                qDebug() << "iproxy error:" << error;
            });

    // connect(iproxyProcess, &QProcess::readyReadStandardOutput, this, [this]()
    // {
    //     QByteArray output = iproxyProcess->readAllStandardOutput();
    //     qDebug() << "iproxy output:" << output;

    //     // Once iproxy is running, start SSH terminal after a short delay
    //     if (!m_sshConnected) {
    //         QTimer::singleShot(2000, this, &JailbrokenWidget::startSSH);
    //     }
    // });

    QTimer::singleShot(2000, this, &JailbrokenWidget::startSSH);

    iproxyProcess->start("iproxy", QStringList()
                                       << "-u" << m_device->udid.c_str()
                                       << "3333" << "22");
}

void JailbrokenWidget::startSSH()
{
    if (m_sshConnected)
        return;

    m_infoLabel->setText("Starting SSH terminal...");

    // Show the terminal and start SSH session
    m_terminal->show();

    // Start the terminal with an empty shell first
    m_terminal->startTerminalTeletype();

    // Create SSH process
    sshProcess = new QProcess(this);
    sshProcess->setProcessChannelMode(QProcess::MergedChannels);

    // Check if sshpass is available for automatic password entry
    QProcess *checkSshpass = new QProcess(this);
    checkSshpass->start("which", QStringList() << "sshpass");
    checkSshpass->waitForFinished(1000);

    QStringList sshArgs;
    QString sshProgram;

    if (checkSshpass->exitCode() == 0) {
        // Use sshpass for automatic login
        sshProgram = "sshpass";
        sshArgs << "-p" << "alpine"
                << "ssh"
                << "-t" // Force pseudo-terminal allocation
                << "-o" << "StrictHostKeyChecking=no"
                << "-o" << "UserKnownHostsFile=/dev/null"
                << "-p" << "3333"
                << "root@localhost";
        m_infoLabel->setText(
            "SSH terminal connected on port 3333 (using sshpass)");
    } else {
        // Use regular SSH (user will need to enter password manually)
        sshProgram = "ssh";
        sshArgs << "-t" // Force pseudo-terminal allocation
                << "-o" << "StrictHostKeyChecking=no"
                << "-o" << "UserKnownHostsFile=/dev/null"
                << "-p" << "3333"
                << "root@localhost";
        m_infoLabel->setText("SSH terminal connected on port 3333 (enter "
                             "'alpine' when prompted)");
    }

    checkSshpass->deleteLater();

    // Connect terminal to SSH process
    connectTerminalToProcess();

    // Handle SSH process finish
    connect(sshProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &JailbrokenWidget::onSshProcessFinished);

    // Start SSH process
    sshProcess->start(sshProgram, sshArgs);

    if (!sshProcess->waitForStarted(3000)) {
        m_infoLabel->setText("Failed to start SSH process");
        m_terminal->hide();
        return;
    }

    m_sshConnected = true;
    m_connectButton->setEnabled(true);
    m_connectButton->setText("Disconnect SSH");

    // Set focus to terminal so user can type immediately
    m_terminal->setFocus();
}

void JailbrokenWidget::onSshProcessFinished(int exitCode,
                                            QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    m_infoLabel->setText("SSH connection closed");
    m_sshConnected = false;
    m_connectButton->setText("Connect SSH Terminal");
    m_terminal->hide();

    if (sshProcess) {
        sshProcess->deleteLater();
        sshProcess = nullptr;
    }
}

JailbrokenWidget::~JailbrokenWidget()
{
    if (iproxyProcess) {
        iproxyProcess->terminate();
        iproxyProcess->waitForFinished(3000);
    }
}