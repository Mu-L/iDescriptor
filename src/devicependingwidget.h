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

#ifndef DEVICEPENDINGWIDGET_H
#define DEVICEPENDINGWIDGET_H

#include <QLabel>
#include <QWidget>

class ResponsiveQLabel;

class DevicePendingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DevicePendingWidget(bool locked, QWidget *parent);
    void next();
signals:
private:
    QLabel *m_label;
    ResponsiveQLabel *m_imageLabel;
    bool m_locked;
};

#endif // DEVICEPENDINGWIDGET_H
