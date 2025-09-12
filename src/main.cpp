#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("iDescriptor");
    // QCoreApplication::setOrganizationDomain("iDescriptor.com");
    QCoreApplication::setApplicationName("iDescriptor");

    MainWindow w;
    w.show();
    return a.exec();
}
