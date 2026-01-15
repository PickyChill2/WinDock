#include "ExtensionManager.h"
#include "Dock.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QMenu>

ExtensionInterface* ExtensionManager::getExtensionInstance(const QString& name)
{
    if (m_extensionCreators.contains(name) && m_enabledExtensions.contains(name)) {
        if (!m_extensions.contains(name)) {
            ExtensionInterface* (*creator)() = m_extensionCreators[name];
            m_extensions[name] = creator();
        }
        return m_extensions[name];
    }
    return nullptr;
}

ExtensionManager& ExtensionManager::instance()
{
    static ExtensionManager instance;
    return instance;
}

ExtensionManager::ExtensionManager()
{
    // НЕ загружаем расширения из конфига здесь - статические объекты еще не созданы
}

ExtensionManager::~ExtensionManager()
{
    for (ExtensionInterface* extension : m_extensions) {
        delete extension;
    }
    m_extensions.clear();
}

void ExtensionManager::registerExtension(const QString& name, ExtensionInterface* (*creator)())
{
    if (creator) {
        m_extensionCreators[name] = creator;
        qDebug() << "Registered extension:" << name;
    }
}

QList<QString> ExtensionManager::extensionNames() const
{
    // Возвращаем только включенные расширения
    QList<QString> enabled;
    for (const QString& name : m_extensionCreators.keys()) {
        if (m_enabledExtensions.contains(name)) {
            enabled.append(name);
        }
    }
    return enabled;
}

QList<QString> ExtensionManager::extensionsForLocation(ExtensionDisplayLocation location) const
{
    QList<QString> result;

    for (const QString& name : m_extensionCreators.keys()) {
        if (m_enabledExtensions.contains(name)) {
            if (!m_extensions.contains(name)) {
                // Создаем временный экземпляр для проверки расположения
                ExtensionInterface* (*creator)() = m_extensionCreators[name];
                ExtensionInterface* tempExtension = creator();

                bool shouldDisplay = false;
                switch (location) {
                    case ExtensionDisplayLocation::TopPanel:
                        shouldDisplay = tempExtension->shouldDisplayInTopPanel();
                        break;
                    case ExtensionDisplayLocation::Calendar:
                        shouldDisplay = tempExtension->shouldDisplayInCalendar();
                        break;
                    case ExtensionDisplayLocation::DockLeft:
                        shouldDisplay = tempExtension->shouldDisplayInDockLeft();
                        break;
                    case ExtensionDisplayLocation::DockRight:
                        shouldDisplay = tempExtension->shouldDisplayInDockRight();
                        break;
                    case ExtensionDisplayLocation::Both:
                        shouldDisplay = tempExtension->shouldDisplayInTopPanel() ||
                                       tempExtension->shouldDisplayInCalendar() ||
                                       tempExtension->shouldDisplayInDockLeft() ||
                                       tempExtension->shouldDisplayInDockRight();
                        break;
                }

                delete tempExtension;

                if (shouldDisplay) {
                    result.append(name);
                }
            } else {
                // Используем существующий экземпляр
                ExtensionInterface* extension = m_extensions[name];
                bool shouldDisplay = false;

                switch (location) {
                    case ExtensionDisplayLocation::TopPanel:
                        shouldDisplay = extension->shouldDisplayInTopPanel();
                        break;
                    case ExtensionDisplayLocation::Calendar:
                        shouldDisplay = extension->shouldDisplayInCalendar();
                        break;
                    case ExtensionDisplayLocation::DockLeft:
                        shouldDisplay = extension->shouldDisplayInDockLeft();
                        break;
                    case ExtensionDisplayLocation::DockRight:
                        shouldDisplay = extension->shouldDisplayInDockRight();
                        break;
                    case ExtensionDisplayLocation::Both:
                        shouldDisplay = extension->shouldDisplayInTopPanel() ||
                                       extension->shouldDisplayInCalendar() ||
                                       extension->shouldDisplayInDockLeft() ||
                                       extension->shouldDisplayInDockRight();
                        break;
                }

                if (shouldDisplay) {
                    result.append(name);
                }
            }
        }
    }

    return result;
}

QWidget* ExtensionManager::createExtensionWidget(const QString& name, QWidget* parent)
{
    qDebug() << "Creating extension widget for:" << name;

    if (m_extensionCreators.contains(name) && m_enabledExtensions.contains(name)) {
        qDebug() << "Extension found and enabled:" << name;

        if (!m_extensions.contains(name)) {
            ExtensionInterface* (*creator)() = m_extensionCreators[name];
            m_extensions[name] = creator();
            qDebug() << "Extension instance created:" << name;
        }

        ExtensionInterface* extension = m_extensions[name];
        QWidget* widget = extension->createWidget(parent);
        qDebug() << "Extension widget created:" << name << "widget:" << widget;
        return widget;
    } else {
        qDebug() << "Extension not available or disabled. Available:" << m_extensionCreators.contains(name)
                 << "Enabled:" << m_enabledExtensions.contains(name);
    }
    return nullptr;
}

void ExtensionManager::removeExtension(const QString& name)
{
    if (m_extensions.contains(name)) {
        delete m_extensions[name];
        m_extensions.remove(name);
        qDebug() << "Removed extension:" << name;
    }
}

void ExtensionManager::reloadExtensions()
{
    // Очищаем текущие расширения
    for (ExtensionInterface* extension : m_extensions) {
        delete extension;
    }
    m_extensions.clear();

    // Перезагружаем из конфига
    loadExtensionsFromConfig();
    emit extensionsChanged();
}

void ExtensionManager::populateDockContextMenu(QMenu* menu, DockContextMenuType menuType, DockItem* item)
{
    for (const QString& name : m_extensionCreators.keys()) {
        if (m_enabledExtensions.contains(name)) {
            ExtensionInterface* extension = getExtensionInstance(name);
            if (extension) {
                extension->populateDockContextMenu(menu, menuType, item);
            }
        }
    }
}

bool ExtensionManager::loadExtensionsFromConfig(const QString& configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open extensions config file:" << configPath;
        return false;
    }

    QSet<QString> newEnabledExtensions;
    QSet<QString> newAvailableExtensions; // Все расширения из файла
    QTextStream in(&configFile);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Пропускаем пустые строки
        if (line.isEmpty()) {
            continue;
        }

        // Пропускаем строки, начинающиеся с "🤡" (это заголовки разделов)
        if (line.startsWith("🤡")) {
            continue;
        }

        // Обрабатываем как комментарии, так и обычные строки
        bool isCommented = line.startsWith('#');
        QString processingLine = isCommented ? line.mid(1).trimmed() : line;

        // Пропускаем, если после удаления комментария строка пустая
        if (processingLine.isEmpty()) {
            continue;
        }

        // Пропускаем строки, которые после удаления комментария начинаются с "🤡"
        if (processingLine.startsWith("🤡")) {
            continue;
        }

        // Разбираем строку: ExtensionName=Path/To/File.cpp,Path/To/File.h
        int equalsPos = processingLine.indexOf('=');
        if (equalsPos > 0) {
            QString extensionName = processingLine.left(equalsPos).trimmed();
            newAvailableExtensions.insert(extensionName);

            // Если исходная строка не была закомментирована - расширение включено
            if (!isCommented) {
                newEnabledExtensions.insert(extensionName);
                qDebug() << "Enabled extension from config:" << extensionName;
            } else {
                qDebug() << "Disabled extension from config:" << extensionName;
            }
        } else {
            // Если нет =, считаем всю строку именем расширения
            newAvailableExtensions.insert(processingLine);
            if (!isCommented) {
                newEnabledExtensions.insert(processingLine);
                qDebug() << "Enabled extension from config:" << processingLine;
            } else {
                qDebug() << "Disabled extension from config:" << processingLine;
            }
        }
    }

    configFile.close();

    // Сохраняем все доступные расширения
    m_availableExtensions = newAvailableExtensions;

    // Обновляем список включенных расширений
    m_enabledExtensions = newEnabledExtensions;

    // Удаляем расширения, которых нет в конфиге
    QList<QString> toRemove;
    for (const QString& name : m_extensions.keys()) {
        if (!m_availableExtensions.contains(name)) {
            toRemove.append(name);
        }
    }

    for (const QString& name : toRemove) {
        removeExtension(name);
    }

    // Также удаляем из creators, если они не включены
    QList<QString> creatorsToRemove;
    for (const QString& name : m_extensionCreators.keys()) {
        if (!m_availableExtensions.contains(name)) {
            creatorsToRemove.append(name);
        }
    }

    for (const QString& name : creatorsToRemove) {
        m_extensionCreators.remove(name);
    }

    qDebug() << "Loaded" << m_enabledExtensions.size() << "enabled extensions from config";
    qDebug() << "Available extensions:" << m_availableExtensions;
    qDebug() << "Enabled extensions:" << m_enabledExtensions;

    emit extensionsChanged();

    return true;
}

void ExtensionManager::scanAndLoadExtensions()
{
    // Может использоваться для автоматического сканирования папок
}

void ExtensionManager::loadExtension(const QString& name)
{
    // Загружает конкретное расширение по имени
}

void ExtensionManager::registerPendingExtensions()
{
    // Переносим ожидающие расширения в основные
    for (auto it = m_pendingExtensions.begin(); it != m_pendingExtensions.end(); ++it) {
        m_extensionCreators[it.key()] = it.value();
    }
    m_pendingExtensions.clear();

    // Перезагружаем расширения
    reloadExtensions();
}