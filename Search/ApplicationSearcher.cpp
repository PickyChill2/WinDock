#include "ApplicationSearcher.h"
#include "SearchResult.h"

#include <QDebug>
#include <QDirIterator>
#include <QProcess>
#include <QSettings>
#include <QMimeDatabase>
#include <QMimeType>

// Windows API
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>

ApplicationSearcher::ApplicationSearcher(QObject *parent)
    : QObject(parent)
{
    loadSearchLocations();
}

void ApplicationSearcher::loadSearchLocations()
{
    m_searchPaths.clear();

    // Стандартные пути меню "Пуск"
    m_searchPaths.append(QDir::homePath() + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs");
    m_searchPaths.append("C:/ProgramData/Microsoft/Windows/Start Menu/Programs");

    // Пользовательские пути
    m_searchPaths.append(m_customPaths);

    // Убираем дубликаты
    m_searchPaths.removeDuplicates();
}

QStringList ApplicationSearcher::getSearchPaths() const
{
    return m_searchPaths;
}

void ApplicationSearcher::setCustomSearchPaths(const QStringList &paths)
{
    m_customPaths = paths;
    loadSearchLocations(); // Перезагружаем пути с новыми пользовательскими
}

void ApplicationSearcher::cacheAllFiles()
{
    m_cachedFiles.clear();

    QStringList searchPaths = getSearchPaths();
    for (const QString &searchPath : searchPaths) {
        QDir searchDir(searchPath);
        if (!searchDir.exists()) {
            qDebug() << "Search path does not exist:" << searchPath;
            continue;
        }

        // Ищем все файлы и папки
        QDirIterator it(searchPath, QDir::AllEntries | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);

        int fileCount = 0;
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fileInfo(filePath);

            // Ограничим количество кэшируемых файлов для производительности
            if (fileCount++ > 10000) {
                break;
            }

            SearchResult result;
            result.name = fileInfo.fileName();
            result.path = filePath;
            result.type = getFileType(filePath);
            result.description = fileInfo.isDir() ? "Папка" :
                                QString("Файл: %1").arg(fileInfo.suffix().toUpper());

            // Проверяем на дубликаты (по пути)
            bool isDuplicate = false;
            for (const SearchResult &existingFile : m_cachedFiles) {
                if (existingFile.path == result.path) {
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate) {
                m_cachedFiles.append(result);
            }
        }
    }

    qDebug() << "Cached" << m_cachedFiles.size() << "files from" << searchPaths.size() << "paths";
}

QList<SearchResult> ApplicationSearcher::searchAllFiles(const QString &query, int maxResults)
{
    QList<SearchResult> results;

    if (query.isEmpty()) {
        return results;
    }

    QString queryLower = query.toLower();
    int foundCount = 0;

    for (const SearchResult &file : m_cachedFiles) {
        if (foundCount >= maxResults) {
            break;
        }

        // Ищем по имени файла или пути
        if (file.name.toLower().contains(queryLower) ||
            file.path.toLower().contains(queryLower)) {
            results.append(file);
            foundCount++;
        }
    }

    return results;
}

bool ApplicationSearcher::launchApplication(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    QString actualPath = path;

    // Обработка ярлыков (.lnk)
    if (path.endsWith(".lnk", Qt::CaseInsensitive)) {
        HRESULT hres = CoInitialize(NULL);
        if (SUCCEEDED(hres)) {
            IShellLink* pShellLink = NULL;
            hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                   IID_IShellLink, (LPVOID*)&pShellLink);

            if (SUCCEEDED(hres)) {
                IPersistFile* pPersistFile = NULL;
                hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);

                if (SUCCEEDED(hres)) {
                    hres = pPersistFile->Load(path.toStdWString().c_str(), STGM_READ);

                    if (SUCCEEDED(hres)) {
                        wchar_t exePath[MAX_PATH];
                        hres = pShellLink->GetPath(exePath, MAX_PATH, NULL, SLGP_UNCPRIORITY);

                        if (SUCCEEDED(hres)) {
                            actualPath = QString::fromWCharArray(exePath);
                        }
                    }
                    pPersistFile->Release();
                }
                pShellLink->Release();
            }
            CoUninitialize();
        }
    }

    // Запускаем приложение
    if (!actualPath.isEmpty() && QFile::exists(actualPath)) {
        bool success = QProcess::startDetached(actualPath);
        if (success) {
            qDebug() << "Successfully launched:" << actualPath;
        } else {
            qDebug() << "Failed to launch:" << actualPath;
        }
        return success;
    }

    return false;
}

bool ApplicationSearcher::isValidPath(const QString &path)
{
    return QFile::exists(path);
}

QString ApplicationSearcher::getFileType(const QString &filePath)
{
    QFileInfo fileInfo(filePath);

    if (fileInfo.isDir()) {
        return "folder";
    }

    QString suffix = fileInfo.suffix().toLower();

    // Приложения
    if (suffix == "exe" || suffix == "lnk" || suffix == "bat" || suffix == "cmd" ||
        suffix == "msi" || suffix == "app") {
        return "app";
    }

    if (suffix == "cmd" || suffix == "ps1" || suffix == "sh") {
        return "terminal";
    }

    // Изображения
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "png" || suffix == "bmp" ||
        suffix == "gif" || suffix == "ico" || suffix == "svg" || suffix == "tiff" ||
        suffix == "webp" || suffix == "raw") {
        return "image";
    }

    // Видео
    if (suffix == "mp4" || suffix == "avi" || suffix == "mkv" || suffix == "mov" ||
        suffix == "wmv" || suffix == "flv" || suffix == "mpeg" || suffix == "mpg" ||
        suffix == "m4v" || suffix == "webm" || suffix == "3gp") {
        return "video";
    }

    // Аудио
    if (suffix == "mp3" || suffix == "wav" || suffix == "flac" || suffix == "aac" ||
        suffix == "ogg" || suffix == "wma" || suffix == "m4a" || suffix == "opus") {
        return "audio";
    }

    // Документы
    if (suffix == "pdf" || suffix == "doc" || suffix == "docx" || suffix == "txt" ||
        suffix == "rtf" || suffix == "odt" || suffix == "xls" || suffix == "xlsx" ||
        suffix == "ppt" || suffix == "pptx" || suffix == "csv") {
        return "document";
    }

    // Архивы
    if (suffix == "zip" || suffix == "rar" || suffix == "7z" || suffix == "tar" ||
        suffix == "gz" || suffix == "bz2" || suffix == "iso") {
        return "archive";
    }

    // Конфигурационные файлы
    if (suffix == "ini" || suffix == "cfg" || suffix == "conf" || suffix == "xml" ||
        suffix == "json" || suffix == "yml" || suffix == "yaml") {
        return "config";
    }

    // Код
    if (suffix == "cpp" || suffix == "h" || suffix == "hpp" || suffix == "c" ||
        suffix == "cs" || suffix == "java" || suffix == "py" || suffix == "js" ||
        suffix == "html" || suffix == "css" || suffix == "php" || suffix == "rb" ||
        suffix == "go" || suffix == "rs") {
        return "code";
    }

    // По умолчанию
    return "file";
}

QStringList ApplicationSearcher::getFileExtensionsByType(const QString &type)
{
    if (type == "image") {
        return {"jpg", "jpeg", "png", "bmp", "gif", "ico", "svg", "tiff", "webp", "raw"};
    } else if (type == "video") {
        return {"mp4", "avi", "mkv", "mov", "wmv", "flv", "mpeg", "mpg", "m4v", "webm", "3gp"};
    } else if (type == "audio") {
        return {"mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus"};
    } else if (type == "document") {
        return {"pdf", "doc", "docx", "txt", "rtf", "odt", "xls", "xlsx", "ppt", "pptx", "csv"};
    } else if (type == "archive") {
        return {"zip", "rar", "7z", "tar", "gz", "bz2", "iso"};
    } else if (type == "app") {
        return {"exe", "lnk", "bat", "cmd", "msi", "app"};
    } else if (type == "terminal") {
        return {"cmd", "ps1", "sh"};
    }

    return {};
}