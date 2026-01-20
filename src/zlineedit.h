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

#pragma once

#include <QApplication>
#include <QEvent>
#include <QLineEdit>

class ZLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ZLineEdit(QWidget *parent = nullptr);
    explicit ZLineEdit(const QString &text, QWidget *parent = nullptr);

private slots:
    void updateStyles();

private:
    void setupStyles();

protected:
    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange) {
            updateStyles();
        }
        QLineEdit::changeEvent(event);
    }
};