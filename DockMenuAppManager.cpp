#include "DockMenuAppManager.h"
#include <QDir>
#include <QFileIconProvider>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

DockMenuAppManager::DockMenuAppManager(QObject *parent)
    : QObject(parent)
{
    // Определяем путь для хранения конфигурации Dock
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    m_configFilePath = configDir + "/dock_pinned_apps.json";
    m_hiddenAppsFilePath = configDir + "/dock_hidden_apps.json";

    loadPinnedApps();
    loadHiddenApps();
}

void DockMenuAppManager::loadPinnedApps()
{
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No dock pinned apps config file found, starting with empty list";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qDebug() << "Invalid dock pinned apps config file";
        return;
    }

    QJsonArray appsArray = doc.array();
    m_pinnedApps.clear();

    for (const QJsonValue& value : appsArray) {
        DockAppInfo appInfo = appInfoFromJson(value.toObject());
        if (!appInfo.name.isEmpty() && !appInfo.executablePath.isEmpty()) {
            m_pinnedApps.append(appInfo);
        }
    }

    qDebug() << "Loaded" << m_pinnedApps.size() << "dock pinned apps";
}

void DockMenuAppManager::savePinnedApps()
{
    QJsonArray appsArray;

    for (const DockAppInfo& appInfo : m_pinnedApps) {
        appsArray.append(appInfoToJson(appInfo));
    }

    QJsonDocument doc(appsArray);
    QFile file(m_configFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "Saved" << m_pinnedApps.size() << "dock pinned apps";
    } else {
        qDebug() << "Failed to save dock pinned apps config:" << file.errorString();
    }
}

void DockMenuAppManager::loadHiddenApps()
{
    QFile file(m_hiddenAppsFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No dock hidden apps config file found, starting with empty list";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qDebug() << "Invalid dock hidden apps config file";
        return;
    }

    QJsonArray hiddenArray = doc.array();
    m_hiddenApps.clear();

    for (const QJsonValue& value : hiddenArray) {
        if (!value.isString()) {
            qDebug() << "Warning: Non-string value in hidden apps array, skipping";
            continue;
        }

        QString appName = value.toString().toLower();
        if (!appName.isEmpty()) {
            m_hiddenApps.insert(appName);
        }
    }

    qDebug() << "Loaded" << m_hiddenApps.size() << "dock hidden apps";
}

void DockMenuAppManager::saveHiddenApps()
{
    QJsonArray hiddenArray;

    for (const QString& appName : m_hiddenApps) {
        hiddenArray.append(appName);
    }

    QJsonDocument doc(hiddenArray);
    QFile file(m_hiddenAppsFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "Saved" << m_hiddenApps.size() << "dock hidden apps";
    } else {
        qDebug() << "Failed to save dock hidden apps config:" << file.errorString();
    }
}

void DockMenuAppManager::pinApp(const DockAppInfo& app)
{
    // Проверяем, нет ли уже такого приложения в списке
    for (const DockAppInfo& pinnedApp : m_pinnedApps) {
        if (pinnedApp.executablePath == app.executablePath) {
            return;
        }
    }

    m_pinnedApps.append(app);
    savePinnedApps();
    emit pinnedAppsChanged();
}

void DockMenuAppManager::unpinApp(const QString& appExecutablePath)
{
    for (int i = 0; i < m_pinnedApps.size(); ++i) {
        if (m_pinnedApps[i].executablePath == appExecutablePath) {
            m_pinnedApps.removeAt(i);
            savePinnedApps();
            emit pinnedAppsChanged();
            return;
        }
    }
}

void DockMenuAppManager::addHiddenApp(const QString& appExecutableName)
{
    QString appName = appExecutableName.toLower();
    if (!m_hiddenApps.contains(appName)) {
        m_hiddenApps.insert(appName);
        saveHiddenApps();
        qDebug() << "Added hidden app:" << appName;
        emit hiddenAppsChanged();
    }
}

void DockMenuAppManager::removeHiddenApp(const QString& appExecutableName)
{
    QString appName = appExecutableName.toLower();
    if (m_hiddenApps.contains(appName)) {
        m_hiddenApps.remove(appName);
        saveHiddenApps();
        qDebug() << "Removed hidden app:" << appName;
        emit hiddenAppsChanged();
    }
}

bool DockMenuAppManager::isAppHidden(const QString& appExecutableName) const
{
    if (appExecutableName.isEmpty()) {
        return false;
    }
    return m_hiddenApps.contains(appExecutableName.toLower());
}

QList<DockAppInfo> DockMenuAppManager::getPinnedApps() const
{
    return m_pinnedApps;
}

QSet<QString> DockMenuAppManager::getHiddenApps() const
{
    return m_hiddenApps;
}

bool DockMenuAppManager::isAppPinned(const QString& appExecutablePath) const
{
    for (const DockAppInfo& app : m_pinnedApps) {
        if (app.executablePath == appExecutablePath) {
            return true;
        }
    }
    return false;
}

QIcon DockMenuAppManager::extractIconFromExecutable(const QString& executablePath) const
{
    if (executablePath.isEmpty()) {
        return QIcon();
    }

#ifdef Q_OS_WIN
    // Используем Windows API для извлечения иконки
    HICON hIcon = nullptr;

    // Пытаемся извлечь иконку большого размера
    SHFILEINFO shFileInfo = { 0 };
    if (SHGetFileInfoW(reinterpret_cast<const WCHAR*>(executablePath.utf16()),
                      0, &shFileInfo, sizeof(shFileInfo),
                      SHGFI_ICON | SHGFI_LARGEICON)) {
        hIcon = shFileInfo.hIcon;
    }

    if (hIcon) {
        QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(hIcon));
        DestroyIcon(hIcon);
        if (!pixmap.isNull()) {
            return QIcon(pixmap);
        }
    }
#endif

    // Fallback: используем QFileIconProvider
    QFileIconProvider iconProvider;
    return iconProvider.icon(QFileInfo(executablePath));
}

DockAppInfo DockMenuAppManager::appInfoFromJson(const QJsonObject& jsonObj) const
{
    DockAppInfo appInfo;
    appInfo.name = jsonObj["name"].toString();
    appInfo.executablePath = jsonObj["executablePath"].toString();

    // Загружаем иконку из файла
    appInfo.icon = extractIconFromExecutable(appInfo.executablePath);

    return appInfo;
}

QJsonObject DockMenuAppManager::appInfoToJson(const DockAppInfo& appInfo) const
{
    QJsonObject jsonObj;
    jsonObj["name"] = appInfo.name;
    jsonObj["executablePath"] = appInfo.executablePath;
    // Иконка не сохраняется в JSON, будет загружаться при запуске
    return jsonObj;
}