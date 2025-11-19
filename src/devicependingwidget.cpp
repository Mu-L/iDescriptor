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

#include "devicependingwidget.h"
#include "responsiveqlabel.h"
#include <QLabel>
#include <QVBoxLayout>

DevicePendingWidget::DevicePendingWidget(bool locked, QWidget *parent)
    : QWidget{parent}, m_label{nullptr}, m_imageLabel{nullptr}, m_locked{locked}
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addStretch();
    m_label = new QLabel(
        m_locked ? "Please unlock the screen and click on trust on the popup"
                 : "Please click on trust on the popup",
        this);
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label);

    m_imageLabel = new ResponsiveQLabel(this);
    m_imageLabel->setPixmap(QPixmap(":/resources/trust.png"));
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->setMinimumSize(400, 200);
    m_imageLabel->setScaledContents(true);
    layout->addWidget(m_imageLabel, 1, Qt::AlignHCenter);
    layout->addStretch();
    setLayout(layout);
}

void DevicePendingWidget::next()
{
    m_label->setText("Please click on trust on the popup");
}