#include "ProxyExtension.h"
#include <QStyle>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

ProxyWidget::ProxyWidget(QWidget* parent)
    : QWidget(parent)
    , m_proxyLabel(nullptr)
    , m_toggleButton(nullptr)
    , m_contextMenu(nullptr)
    , m_enableAction(nullptr)
    , m_disableAction(nullptr)
    , m_settingsAction(nullptr)
    , m_checkAction(nullptr)  // Добавлен новый элемент
    , m_proxyEnabled(false)
    , m_countryCode("")
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentService(0) // Начинаем с первого сервиса
{
    setFixedHeight(25);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(5);
    layout->setAlignment(Qt::AlignVCenter);

    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFixedSize(16, 16);
    m_toggleButton->setStyleSheet(
        "QPushButton {"
        "    border: 1px solid #666;"
        "    border-radius: 8px;"
        "    background-color: #ff4444;"
        "}"
        "QPushButton:hover {"
        "    border: 1px solid #888;"
        "}"
    );

    m_proxyLabel = new QLabel(this);
    m_proxyLabel->setStyleSheet(
        "color: white; "
        "font-size: 12px; "
        "background: transparent; "
        "padding: 0px; "
        "margin: 0px;"
    );
    m_proxyLabel->setText("Прокси:❌");

    layout->addWidget(m_toggleButton);
    layout->addWidget(m_proxyLabel);

    m_contextMenu = new QMenu(this);

    m_enableAction = new QAction("Включить прокси", this);
    m_disableAction = new QAction("Отключить прокси", this);
    m_settingsAction = new QAction("Настройки прокси", this);
    m_checkAction = new QAction("обновить прокси 🤡", this);

    connect(m_enableAction, &QAction::triggered, this, &ProxyWidget::enableProxy);
    connect(m_disableAction, &QAction::triggered, this, &ProxyWidget::disableProxy);
    connect(m_settingsAction, &QAction::triggered, this, &ProxyWidget::openProxySettings);
    connect(m_checkAction, &QAction::triggered, this, &ProxyWidget::fetchCountryCode);

    m_contextMenu->addAction(m_enableAction);
    m_contextMenu->addAction(m_disableAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_checkAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_settingsAction);

    connect(m_toggleButton, &QPushButton::clicked, this, &ProxyWidget::toggleProxy);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &ProxyWidget::onCountryCodeReceived);

    QTimer::singleShot(100, this, &ProxyWidget::updateProxyStatus);
}

ProxyWidget::~ProxyWidget()
{
}

void ProxyWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        updateContextMenu();
        m_contextMenu->exec(event->globalPosition().toPoint());
    }
    QWidget::mousePressEvent(event);
}

void ProxyWidget::toggleProxy()
{
    setProxyEnabled(!m_proxyEnabled);
    updateProxyStatus();
}

void ProxyWidget::enableProxy()
{
    setProxyEnabled(true);
    updateProxyStatus();
}

void ProxyWidget::disableProxy()
{
    setProxyEnabled(false);
    updateProxyStatus();
}

void ProxyWidget::updateProxyStatus()
{
    m_proxyEnabled = isProxyEnabled();
    m_currentProxy = getCurrentProxy();

    if (m_proxyEnabled) {
        fetchCountryCode();
    } else {
        m_countryCode = "";
    }

    if (m_proxyEnabled) {
        m_toggleButton->setStyleSheet(
            "QPushButton {"
            "    border: 1px solid #666;"
            "    border-radius: 8px;"
            "    background-color: #44ff44;"
            "}"
            "QPushButton:hover {"
            "    border: 1px solid #888;"
            "}"
        );

        if (!m_countryCode.isEmpty()) {
            m_proxyLabel->setText(QString("Прокси:🦕 - %1").arg(m_countryCode));
        } else {
            m_proxyLabel->setText("Прокси:🦕 - ...");
        }
    } else {
        m_toggleButton->setStyleSheet(
            "QPushButton {"
            "    border: 1px solid #666;"
            "    border-radius: 8px;"
            "    background-color: #ff4444;"
            "}"
            "QPushButton:hover {"
            "    border: 1px solid #888;"
            "}"
        );
        m_proxyLabel->setText("Прокси:❌");
    }

    updateContextMenu();
}

void ProxyWidget::updateContextMenu()
{
    m_enableAction->setVisible(!m_proxyEnabled);
    m_disableAction->setVisible(m_proxyEnabled);
    // Действие "🤡" всегда видимо, но активно только при включенном прокси
    m_checkAction->setEnabled(m_proxyEnabled);
}

void ProxyWidget::openProxySettings()
{
    bool success = false;
    success = QDesktopServices::openUrl(QUrl("start winsetting:proxy"));

    if (!success) {
        success = QDesktopServices::openUrl(QUrl("winsetting:network"));
    }

    if (!success) {
        success = QProcess::startDetached("control.exe", QStringList() << "inetcpl.cpl,,4");
    }

    if (!success) {
        success = QProcess::startDetached("start", QStringList() << "start winsetting:proxy");
    }

    if (!success) {
        success = QProcess::startDetached("rundll32.exe", QStringList() << "shell32.dll,Control_RunDLL" << "inetcpl.cpl,,4");
    }
}

void ProxyWidget::fetchCountryCode()
{
    QNetworkRequest request;
    QString url;

    // Список сервисов для определения местоположения
    switch (m_currentService) {
    case 0:
        url = "http://ip-api.com/json/?fields=status,message,countryCode";
        break;
    case 1:
        url = "https://api.country.is/";
        break;
    case 2:
        url = "https://ipapi.co/json/";
        break;
    case 3:
        url = "https://api.ip.sb/geoip";
        break;
    default:
        // Если все сервисы не работают, сбрасываем к первому
        m_currentService = 0;
        url = "http://ip-api.com/json/?fields=status,message,countryCode";
        break;
    }

    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    m_networkManager->get(request);
}

void ProxyWidget::onCountryCodeReceived(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        QJsonObject jsonObj = jsonDoc.object();

        QString countryCode;
        bool success = false;

        // Парсим ответ в зависимости от используемого сервиса
        switch (m_currentService) {
        case 0: // ip-api.com
            if (jsonObj.contains("status") && jsonObj["status"].toString() == "success") {
                countryCode = jsonObj["countryCode"].toString();
                success = true;
            }
            break;
        case 1: // country.is
            if (jsonObj.contains("country")) {
                countryCode = jsonObj["country"].toString();
                success = true;
            }
            break;
        case 2: // ipapi.co
            if (jsonObj.contains("country_code")) {
                countryCode = jsonObj["country_code"].toString();
                success = true;
            }
            break;
        case 3: // ip.sb
            if (jsonObj.contains("country_code")) {
                countryCode = jsonObj["country_code"].toString();
                success = true;
            }
            break;
        }

        if (success) {
            m_countryCode = countryCode;
            if (m_proxyEnabled) {
                m_proxyLabel->setText(QString("Прокси:🦕 - %1").arg(m_countryCode));
            }
        } else {
            // Если текущий сервис не сработал, пробуем следующий
            tryNextService();
        }
    } else {
        // Если ошибка сети, пробуем следующий сервис
        tryNextService();
    }

    reply->deleteLater();
}

void ProxyWidget::tryNextService()
{
    // Переходим к следующему сервису
    m_currentService = (m_currentService + 1) % 4;

    // Если прокси все еще включен, пробуем снова
    if (m_proxyEnabled) {
        QTimer::singleShot(500, this, &ProxyWidget::fetchCountryCode);
    }
}

bool ProxyWidget::isProxyEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", QSettings::NativeFormat);
    return settings.value("ProxyEnable", 0).toInt() == 1;
#else
    return false;
#endif
}

QString ProxyWidget::getCurrentProxy()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", QSettings::NativeFormat);
    return settings.value("ProxyServer", "").toString();
#else
    return "";
#endif
}

void ProxyWidget::setProxyEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", QSettings::NativeFormat);
    settings.setValue("ProxyEnable", enabled ? 1 : 0);

    InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);
#endif
}

QWidget* ProxyExtension::createWidget(QWidget* parent)
{
    return new ProxyWidget(parent);
}