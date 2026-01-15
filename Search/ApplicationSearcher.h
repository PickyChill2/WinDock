#ifndef APPLICATIONSEARCHER_H
#define APPLICATIONSEARCHER_H

#include <QObject>
#include <QStringList>
#include <QList>

#include "SearchResult.h"

class ApplicationSearcher : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationSearcher(QObject *parent = nullptr);

    // Загружает пути для поиска приложений
    void loadSearchLocations();

    // Возвращает список путей для поиска
    QStringList getSearchPaths() const;

    // Устанавливает пользовательские пути
    void setCustomSearchPaths(const QStringList &paths);

    // Кэширует все файлы из указанных путей
    void cacheAllFiles();

    // Ищет все файлы по запросу
    QList<SearchResult> searchAllFiles(const QString &query, int maxResults = 50);

    // Запускает приложение по пути
    static bool launchApplication(const QString &path);

    // Проверяет, существует ли путь
    static bool isValidPath(const QString &path);

private:
    QString getFileType(const QString &filePath);
    QStringList getFileExtensionsByType(const QString &type);

    QStringList m_searchPaths;
    QStringList m_customPaths;
    QList<SearchResult> m_cachedFiles;
};

#endif // APPLICATIONSEARCHER_H