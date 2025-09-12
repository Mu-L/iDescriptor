#ifndef CLICKABLEWIDGET_H
#define CLICKABLEWIDGET_H

#include <QMouseEvent>
#include <QWidget>

class ClickableWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClickableWidget(QWidget *parent = nullptr) : QWidget(parent) {}

signals:
    void clicked();

protected:
    // Override the protected event handler
    void mousePressEvent(QMouseEvent *event) override
    {
        // When the event happens, emit our public signal
        emit clicked();
        // Call the base class implementation if needed
        QWidget::mousePressEvent(event);
    }
};

#endif // CLICKABLEWIDGET_H