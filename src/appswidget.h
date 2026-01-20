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

#ifndef APPSWIDGET_H
#define APPSWIDGET_H

#include "appstoremanager.h"
#include "iDescriptor-ui.h"
#include "qprocessindicator.h"
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

struct SponsorType {
    enum Level { None, Bronze, Silver, Gold, Platinum };

    Level level;
    QString name;
    QString color;

    SponsorType(Level l = None) : level(l)
    {
        switch (l) {
        case Platinum:
            name = "PLATINUM";
            color = "#E5E4E2"; // Platinum silver-white
            break;
        case Gold:
            name = "GOLD";
            color = "#FFD700"; // Gold
            break;
        case Silver:
            name = "SILVER";
            color = "#C0C0C0"; // Silver
            break;
        case Bronze:
            name = "BRONZE";
            color = "#CD7F32"; // Bronze
            break;
        default:
            name = "";
            color = "";
            break;
        }
    }

    bool isEmpty() const { return level == None; }
};

class AppsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppsWidget(QWidget *parent = nullptr);
    static AppsWidget *sharedInstance();
    void onAppCardClicked(const QString &appName, const QString &bundleId,
                          const QString &description);
    void init();
private slots:
    void onLoginClicked();
    void onDownloadIpaClicked(const QString &name, const QString &bundleId);
    void onSearchTextChanged();
    void performSearch();
    void onSearchFinished(bool success, const QString &results);
    void onAppStoreInitialized(const QJsonObject &accountInfo);

private:
    void setupUI();
    void createAppCard(const QString &name, const QString &bundleId,
                       const QString &description, const QString &logoUrl,
                       const QString &websiteUrl, QGridLayout *gridLayout,
                       int row, int col, bool useBundleIdForIcon = true,
                       const SponsorType &sponsorType = SponsorType());
    void setupDefaultAppsPage();
    void setupLoadingPage();
    void setupErrorPage();
    void showDefaultApps();
    void showLoading(const QString &message = "Loading...");
    void showError(const QString &message);
    void clearAppGrid();
    void populateDefaultApps();
    void createSponsorCard(QGridLayout *gridLayout, int row, int col);
    void handleInit();
    QStackedWidget *m_stackedWidget;
    QWidget *m_defaultAppsPage;
    QWidget *m_loadingPage;
    QWidget *m_errorPage;
    QProcessIndicator *m_loadingIndicator;
    QLabel *m_loadingLabel;
    QLabel *m_errorLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QPushButton *m_loginButton;
    QLabel *m_statusLabel;
    bool m_isLoggedIn;
    AppStoreManager *m_manager;
    QNetworkAccessManager *m_networkManager = nullptr;

    // Search
    QLineEdit *m_searchEdit;
    QTimer *m_debounceTimer;
    QAction *m_searchAction;

    // Sponsors
    QJsonArray m_platinumSponsors;
    QJsonArray m_goldSponsors;
    QJsonArray m_silverSponsors;
    QJsonArray m_bronzeSponsors;
    ZIcon m_searchIcon;

protected:
    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange) {
            if (m_searchAction && !m_searchIcon.isNull()) {
                m_searchAction->setIcon(
                    m_searchIcon.getThemedPixmap(QSize(16, 16), palette()));
            }
        }
        QWidget::changeEvent(event);
    }
};

#endif // APPSWIDGET_H
