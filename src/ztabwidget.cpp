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

#include "ztabwidget.h"
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QMainWindow>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>

ZTab::ZTab(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
    setCheckable(true);
#ifndef WIN32
    setFixedHeight(50);
#else
    setFixedHeight(40);
#endif

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

ZTabWidget::ZTabWidget(QWidget *parent) : QWidget(parent), m_currentIndex(0)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create tab bar container
    m_tabBar = new QWidget();
#ifndef WIN32
    m_tabBar->setFixedHeight(50);
#else
    m_tabBar->setFixedHeight(40);
#endif
    m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tabLayout = new QHBoxLayout(m_tabBar);
    m_tabLayout->setSpacing(0);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);

    // Add drop shadow effect
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(24, 94, 224, 38)); // rgba(24, 94, 224, 0.15)
    shadow->setOffset(0, 6);
    m_tabBar->setGraphicsEffect(shadow);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // Create stacked widget for content
    m_stackedWidget = new QStackedWidget();
    m_stackedWidget->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);

    // Add widgets to layout
    m_mainLayout->addWidget(m_tabBar);
    m_mainLayout->addWidget(m_stackedWidget, 1);

    setupGlider();
}

void ZTabWidget::setupGlider()
{
    m_glider = new QWidget(m_tabBar);
    m_glider->setStyleSheet("QWidget {"
                            "  background-color: #2b5693;"
                            "  border-radius: 1px;"
                            "}");
    m_glider->hide(); // Hide initially until tabs are added
}

ZTab *ZTabWidget::addTab(QWidget *widget, const QString &label)
{
    ZTab *tab = new ZTab(label, m_tabBar);
    connect(tab, &ZTab::clicked, this, &ZTabWidget::onTabClicked);
    int index = m_tabs.count();
    m_tabs.append(tab);
    m_widgets.append(widget);

    m_tabLayout->addWidget(tab);
    m_stackedWidget->addWidget(widget);
    m_buttonGroup->addButton(tab, index);

    return tab;
}

void ZTabWidget::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.count() || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    m_tabs[index]->setChecked(true);
    m_stackedWidget->setCurrentIndex(index);
    updateTabStyles();
    animateGlider(index);

    emit currentChanged(index);
}

void ZTabWidget::finalizeStyles()
{
    ZTab *tab = m_tabs[0];
    if (tab) {
        tab->setChecked(true);
        QTimer::singleShot(0, [this, tab]() {
            if (tab) {
                m_glider->setFixedSize(tab->size().width(), 2);
                int targetX = tab->pos().x();
                int targetY = tab->pos().y() + tab->size().height() - 2;
                m_glider->move(targetX, targetY);
                m_gliderAnimation = new QPropertyAnimation(m_glider, "pos");
                m_gliderAnimation->setDuration(250);
                m_gliderAnimation->setEasingCurve(QEasingCurve::OutCubic);
                m_glider->show();
            }
        });
    }
    updateTabStyles();
}

int ZTabWidget::currentIndex() const { return m_currentIndex; }

QWidget *ZTabWidget::widget(int index) const
{
    if (index < 0 || index >= m_widgets.count()) {
        return nullptr;
    }
    return m_widgets[index];
}

void ZTabWidget::onTabClicked()
{
    ZTab *clickedTab = qobject_cast<ZTab *>(sender());
    if (!clickedTab)
        return;

    int index = m_tabs.indexOf(clickedTab);
    if (index != -1) {
        setCurrentIndex(index);
    }
}

void ZTabWidget::animateGlider(int index)
{
    if (index < 0 || index >= m_tabs.count())
        return;

    ZTab *targetTab = m_tabs[index];
    if (!targetTab)
        return;

    // Get the actual position and size of the target tab
    QPoint targetTabPos = targetTab->pos();
    QSize targetTabSize = targetTab->size();

    // Set glider width to match tab width and height to 2px for bottom border
    m_glider->setFixedSize(targetTabSize.width(), 2);

    // Position glider at the bottom of the target tab
    int targetX = targetTabPos.x();
    int targetY =
        // targetTabPos.y() + targetTabSize.height() + 6; // Position at bottom
        targetTabPos.y() + targetTabSize.height() - 2; // Position at bottom

    if (m_gliderAnimation == nullptr)
        return;

    m_gliderAnimation->stop();
    m_gliderAnimation->setStartValue(m_glider->pos());
    m_gliderAnimation->setEndValue(QPoint(targetX, targetY));
    m_gliderAnimation->start();
}

void ZTabWidget::updateTabStyles()
{
    for (int i = 0; i < m_tabs.count(); ++i) {
        ZTab *tab = m_tabs[i];
        if (tab->isChecked()) {
            tab->setStyleSheet("ZTab {"
                               "  color: #185ee0;"
                               //    "  color: #d7e1f4ff;"
                               "  font-weight: 500;"
                               "  font-size: 20px;"
                               "  border: none;"
                               "  outline: none;"
                               "  background-color: transparent;"
                               "}"
                               "ZTab:hover {"
                               "  background-color: transparent;"
                               "}");
        } else {
            tab->setStyleSheet("ZTab {"
                               "  color: #666;"
                               //    "  color: #2b5693;"
                               "  font-weight: 500;"
                               "  font-size: 20px;"
                               "  border: none;"
                               "  outline: none;"
                               "  background-color: transparent;"
                               "}"
                               "ZTab:hover {"
                               "  color: #185ee0;"
                               "  background-color: transparent;"
                               "}");
        }
    }
}

// Update glider position when widget is resized
void ZTabWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_currentIndex >= 0 && m_currentIndex < m_tabs.count()) {
        animateGlider(m_currentIndex);
    }
}