#ifndef DOCKMENUAPPMANAGER_H
#define DOCKMENUAPPMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSet>

struct DockAppInfo
{
    QString name;
    QString executablePath;
    QIcon icon;

    bool operator==(const DockAppInfo& other) const {
        return executablePath == other.executablePath;
    }
};

class DockMenuAppManager : public QObject
{
    Q_OBJECT

public:
    explicit DockMenuAppManager(QObject *parent = nullptr);

    void loadPinnedApps();
    void savePinnedApps();
    void pinApp(const DockAppInfo& app);
    void unpinApp(const QString& appExecutablePath);
    QList<DockAppInfo> getPinnedApps() const;
    bool isAppPinned(const QString& appExecutablePath) const;

    // Методы для скрытых приложений
    void loadHiddenApps();
    void saveHiddenApps();
    void addHiddenApp(const QString& appExecutableName);
    void removeHiddenApp(const QString& appExecutableName);
    bool isAppHidden(const QString& appExecutableName) const;
    QSet<QString> getHiddenApps() const;

    // Метод извлечения иконки
    QIcon extractIconFromExecutable(const QString& executablePath) const;

    signals:
        void pinnedAppsChanged();
    void hiddenAppsChanged();

private:
    QList<DockAppInfo> m_pinnedApps;
    QSet<QString> m_hiddenApps;
    QString m_configFilePath;
    QString m_hiddenAppsFilePath;

    DockAppInfo appInfoFromJson(const QJsonObject& jsonObj) const;
    QJsonObject appInfoToJson(const DockAppInfo& appInfo) const;
};

#endif // DOCKMENUAPPMANAGER_H