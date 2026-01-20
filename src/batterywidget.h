/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BATTERYWIDGET_H
#define BATTERYWIDGET_H

#include <QEvent>
#include <QWidget>

class BatteryWidget : public QWidget
{
    Q_OBJECT
public:
    BatteryWidget(float value, bool isCharging, QWidget *parent);
    bool getChargingState();
    void setChargingState(bool state);
    void updateContext(bool isCharging, float newValue);

    // New methods for value management
    void setValue(float newValue);
    float getValue() const;

protected:
    void changeEvent(QEvent *event) override;

private:
    QRectF widgetFrame;
    QRectF mainBatteryFrame;
    QRectF tipBatteryFrame;
    QRectF batteryLevelFrame;

    bool m_isCharging = false;

    float m_value = 0.0f;
    float m_minValue = 0.0f;
    float m_maxValue = 100.0f;

    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

#endif // BATTERYWIDGET_H
