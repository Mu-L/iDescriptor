#include "ifusediskunmountbutton.h"
#include <QApplication>
#include <QMessageBox>

iFuseDiskUnmountButton::iFuseDiskUnmountButton(const QString &path,
                                               QWidget *parent)
    : QPushButton{parent}
{
    setIcon(QIcon(":/resources/icons/ClarityHardDiskSolidAlerted.png"));
    setToolTip("Unmount iFuse at " + path);
    setFlat(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(24, 24);
}
