#include "ExtensionLayoutManager.h"
#include <QSettings>

ExtensionLayoutManager::ExtensionLayoutManager(QObject* parent)
    : QObject(parent)
    , m_extensionManager(ExtensionManager::instance())
{
    qDebug() << "ExtensionLayoutManager constructor";
}

ExtensionLayoutManager::~ExtensionLayoutManager()
{
    qDebug() << "ExtensionLayoutManager destructor";
    clearExtensions();
}

void ExtensionLayoutManager::initializeTopPanel(QWidget* parent,
                                               QHBoxLayout* leftLayout,
                                               int panelHeight)
{
    qDebug() << "Initializing TopPanel extensions...";
    qDebug() << "Parent:" << parent;
    qDebug() << "Left layout:" << leftLayout;
    qDebug() << "Panel height:" << panelHeight;

    if (!parent) {
        qDebug() << "Error: Parent is null in initializeTopPanel";
        return;
    }

    if (!leftLayout) {
        qDebug() << "Error: Left layout is null in initializeTopPanel";
        return;
    }

    m_topPanelData.parent = parent;
    m_topPanelData.leftLayout = leftLayout;
    m_topPanelData.panelHeight = panelHeight;

    // Загружаем настройки перед загрузкой расширений
    loadExtensionSettings();

    // Загружаем расширения для TopPanel
    loadExtensions(ExtensionDisplayLocation::TopPanel);

    qDebug() << "TopPanel extensions initialization completed";
}

void ExtensionLayoutManager::initializeDock(QWidget* parent,
                                           QHBoxLayout* leftLayout,
                                           QHBoxLayout* rightLayout,
                                           int iconSize)
{
    qDebug() << "Initializing Dock extensions...";
    qDebug() << "Parent:" << parent;
    qDebug() << "Left layout:" << leftLayout;
    qDebug() << "Right layout:" << rightLayout;
    qDebug() << "Icon size:" << iconSize;

    if (!parent) {
        qDebug() << "Error: Parent is null in initializeDock";
        return;
    }

    m_dockData.parent = parent;
    m_dockData.leftLayout = leftLayout;
    m_dockData.rightLayout = rightLayout;
    m_dockData.iconSize = iconSize;

    // Загружаем расширения для Dock
    loadExtensions(ExtensionDisplayLocation::DockLeft);
    loadExtensions(ExtensionDisplayLocation::DockRight);

    qDebug() << "Dock extensions initialization completed";
}

void ExtensionLayoutManager::loadExtensions(ExtensionDisplayLocation location)
{
    qDebug() << "Loading extensions for location:" << static_cast<int>(location);

    PanelData* panelData = nullptr;

    // Определяем, для какой панели загружаем расширения
    if (location == ExtensionDisplayLocation::TopPanel) {
        panelData = &m_topPanelData;
    } else if (location == ExtensionDisplayLocation::DockLeft ||
               location == ExtensionDisplayLocation::DockRight) {
        panelData = &m_dockData;
    }

    if (!panelData || !panelData->parent) {
        qDebug() << "Error: Panel data or parent is null";
        return;
    }

    QList<QString> extensionNames = m_extensionManager.extensionsForLocation(location);
    qDebug() << "Found extensions for location:" << extensionNames;

    for (const QString& extensionName : extensionNames) {
        // Всегда проверяем настройки для каждого расширения
        QSettings settings("MyCompany", "DockApp");
        bool enabled = settings.value(QString("Extensions/%1").arg(extensionName), true).toBool();

        qDebug() << "Processing extension:" << extensionName << "enabled:" << enabled;

        if (enabled) {
            QWidget* extensionWidget = createExtensionWidget(extensionName, panelData->parent);
            if (extensionWidget) {
                qDebug() << "Successfully created extension widget for:" << extensionName;

                // Устанавливаем размер в зависимости от панели
                if (location == ExtensionDisplayLocation::TopPanel) {
                    extensionWidget->setFixedHeight(panelData->panelHeight);
                    // Сохраняем в карту TopPanel
                    m_topPanelData.extensionWidgets[extensionName] = extensionWidget;
                } else {
                    extensionWidget->setFixedSize(panelData->iconSize, panelData->iconSize);
                }

                // Добавляем в соответствующий layout
                if (location == ExtensionDisplayLocation::DockLeft && panelData->leftLayout) {
                    panelData->leftLayout->addWidget(extensionWidget);
                    qDebug() << "Added to left dock layout:" << extensionName;
                } else if (location == ExtensionDisplayLocation::DockRight && panelData->rightLayout) {
                    panelData->rightLayout->addWidget(extensionWidget);
                    qDebug() << "Added to right dock layout:" << extensionName;
                } else if (panelData->leftLayout) {
                    panelData->leftLayout->addWidget(extensionWidget);
                    qDebug() << "Added to left panel layout:" << extensionName;
                }
            } else {
                qDebug() << "Failed to create extension widget for:" << extensionName;
            }
        } else {
            qDebug() << "Extension is disabled, skipping:" << extensionName;
        }
    }

    // Добавляем растягивающиеся элементы для правильного позиционирования
    if (location == ExtensionDisplayLocation::DockLeft && panelData->leftLayout) {
        panelData->leftLayout->addStretch(1);
        qDebug() << "Added stretch to left dock layout";
    } else if (location == ExtensionDisplayLocation::DockRight && panelData->rightLayout) {
        panelData->rightLayout->insertStretch(0, 1);
        qDebug() << "Added stretch to right dock layout";
    } else if (location == ExtensionDisplayLocation::TopPanel && panelData->leftLayout) {
        panelData->leftLayout->addStretch(1);
        qDebug() << "Added stretch to top panel layout";
    }

    qDebug() << "Finished loading extensions for location:" << static_cast<int>(location);
}

void ExtensionLayoutManager::updateExtensionsVisibility(ExtensionDisplayLocation location)
{
    qDebug() << "Updating extensions visibility for location:" << static_cast<int>(location);

    PanelData* panelData = nullptr;

    if (location == ExtensionDisplayLocation::TopPanel) {
        panelData = &m_topPanelData;
    } else if (location == ExtensionDisplayLocation::DockLeft ||
               location == ExtensionDisplayLocation::DockRight) {
        panelData = &m_dockData;
    }

    if (!panelData || !panelData->leftLayout) {
        qDebug() << "Error: Panel data or left layout is null";
        return;
    }

    // Очищаем текущие layout'ы
    QLayoutItem* item;
    QHBoxLayout* targetLayout = nullptr;

    if (location == ExtensionDisplayLocation::DockLeft) {
        targetLayout = panelData->leftLayout;
    } else if (location == ExtensionDisplayLocation::DockRight) {
        targetLayout = panelData->rightLayout;
    } else {
        targetLayout = panelData->leftLayout;
    }

    if (!targetLayout) {
        qDebug() << "Error: Target layout is null";
        return;
    }

    qDebug() << "Clearing target layout...";
    while ((item = targetLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            // Устанавливаем родителя в nullptr и планируем удаление
            item->widget()->setParent(nullptr);
            item->widget()->deleteLater();
        }
        delete item;
    }
    qDebug() << "Target layout cleared";

    // Перезагружаем расширения
    loadExtensions(location);
    qDebug() << "Extensions visibility updated for location:" << static_cast<int>(location);
}

void ExtensionLayoutManager::onExtensionToggled(const QString& extensionName, bool enabled)
{
    qDebug() << "Extension toggled:" << extensionName << "enabled:" << enabled;

    ExtensionInterface* extension = m_extensionManager.getExtensionInstance(extensionName);
    if (!extension) {
        qDebug() << "Error: Extension instance not found:" << extensionName;
        return;
    }

    // Сохраняем настройку независимо от типа расширения
    saveExtensionSetting(extensionName, enabled);

    // Проверяем, должно ли расширение отображаться в верхней панели
    if (extension->shouldDisplayInTopPanel()) {
        // Обработка для TopPanel
        if (enabled) {
            // Включаем: создаем виджет если не существует
            if (!m_topPanelData.extensionWidgets.contains(extensionName)) {
                QWidget* extensionWidget = createExtensionWidget(extensionName, m_topPanelData.parent);
                if (extensionWidget) {
                    extensionWidget->setFixedHeight(m_topPanelData.panelHeight);
                    m_topPanelData.extensionWidgets[extensionName] = extensionWidget;
                    qDebug() << "Enabled and created TopPanel extension:" << extensionName;
                }
            }
        } else {
            // Отключаем: удаляем виджет
            if (m_topPanelData.extensionWidgets.contains(extensionName)) {
                QWidget* extensionWidget = m_topPanelData.extensionWidgets.take(extensionName);
                if (m_topPanelData.leftLayout) {
                    m_topPanelData.leftLayout->removeWidget(extensionWidget);
                }
                if (extensionWidget) {
                    extensionWidget->setParent(nullptr);
                    extensionWidget->deleteLater();
                }
                qDebug() << "Disabled and removed TopPanel extension:" << extensionName;
            }
        }

        // Сохраняем состояние
        m_topPanelData.extensionStates[extensionName] = enabled;

        // Обновляем видимость
        updateExtensionsVisibility(ExtensionDisplayLocation::TopPanel);
    }

    // Обработка для Dock расширений
    if (extension->shouldDisplayInDockLeft() || extension->shouldDisplayInDockRight()) {
        qDebug() << "Processing Dock extension:" << extensionName;

        // Получаем расположение расширения в доке
        ExtensionDisplayLocation dockLocation;
        bool locationFound = false;

        QList<QString> leftExtensions = m_extensionManager.extensionsForLocation(ExtensionDisplayLocation::DockLeft);
        QList<QString> rightExtensions = m_extensionManager.extensionsForLocation(ExtensionDisplayLocation::DockRight);

        if (leftExtensions.contains(extensionName)) {
            dockLocation = ExtensionDisplayLocation::DockLeft;
            locationFound = true;
            qDebug() << "Extension found in DockLeft:" << extensionName;
        } else if (rightExtensions.contains(extensionName)) {
            dockLocation = ExtensionDisplayLocation::DockRight;
            locationFound = true;
            qDebug() << "Extension found in DockRight:" << extensionName;
        }

        if (locationFound) {
            if (enabled) {
                qDebug() << "Enabling Dock extension:" << extensionName << "in location:" << static_cast<int>(dockLocation);
                // Включаем расширение в доке - полностью перезагружаем layout
                updateExtensionsVisibility(dockLocation);
            } else {
                qDebug() << "Disabling Dock extension:" << extensionName << "in location:" << static_cast<int>(dockLocation);
                // Отключаем расширение в доке - полностью перезагружаем layout
                updateExtensionsVisibility(dockLocation);
            }
        } else {
            qDebug() << "Extension not found in Dock locations:" << extensionName;
        }
    }
}

void ExtensionLayoutManager::removeDockExtensionWidget(const QString& extensionName, ExtensionDisplayLocation location)
{
    qDebug() << "Removing Dock extension widget:" << extensionName << "from location:" << static_cast<int>(location);

    // Этот метод больше не нужен, так как мы полностью перезагружаем layout
    // Оставляем его для обратной совместимости, но логика теперь в updateExtensionsVisibility
    Q_UNUSED(extensionName)
    Q_UNUSED(location)

    qDebug() << "Note: removeDockExtensionWidget is deprecated, using updateExtensionsVisibility instead";
}

QWidget* ExtensionLayoutManager::getExtensionWidget(const QString& extensionName) const
{
    if (m_topPanelData.extensionWidgets.contains(extensionName)) {
        return m_topPanelData.extensionWidgets.value(extensionName);
    }
    return nullptr;
}

bool ExtensionLayoutManager::getExtensionState(const QString& extensionName) const
{
    if (m_topPanelData.extensionStates.contains(extensionName)) {
        return m_topPanelData.extensionStates.value(extensionName);
    }
    return false;
}

void ExtensionLayoutManager::clearExtensions()
{
    qDebug() << "Clearing extensions...";

    // Очищаем TopPanel расширения
    qDebug() << "Clearing TopPanel extension widgets...";
    for (auto it = m_topPanelData.extensionWidgets.begin(); it != m_topPanelData.extensionWidgets.end(); ++it) {
        QWidget* widget = it.value();
        if (widget && widget->parent() == m_topPanelData.parent) {
            // Виджет будет удален родителем, просто удаляем из карты
            it.value() = nullptr;
        } else if (widget) {
            // Если родитель другой, удаляем вручную
            widget->deleteLater();
        }
    }
    m_topPanelData.extensionWidgets.clear();

    // Очищаем Dock расширения
    if (m_dockData.leftLayout) {
        qDebug() << "Clearing left dock layout...";
        QLayoutItem* item;
        while ((item = m_dockData.leftLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                // Устанавливаем родителя в nullptr и удаляем
                item->widget()->setParent(nullptr);
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    if (m_dockData.rightLayout) {
        qDebug() << "Clearing right dock layout...";
        QLayoutItem* item;
        while ((item = m_dockData.rightLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->setParent(nullptr);
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    qDebug() << "Extensions cleared";
}

QWidget* ExtensionLayoutManager::createExtensionWidget(const QString& extensionName, QWidget* parent)
{
    qDebug() << "Creating extension widget:" << extensionName << "for parent:" << parent;

    if (!parent) {
        qDebug() << "Error: Parent is null for extension:" << extensionName;
        return nullptr;
    }

    QWidget* widget = nullptr;
    try {
        widget = m_extensionManager.createExtensionWidget(extensionName, parent);
    } catch (const std::exception& e) {
        qDebug() << "Exception creating extension widget:" << e.what();
        return nullptr;
    } catch (...) {
        qDebug() << "Unknown exception creating extension widget";
        return nullptr;
    }

    if (widget) {
        qDebug() << "Successfully created widget for:" << extensionName;
        // Устанавливаем объектное имя для идентификации
        widget->setObjectName(extensionName + "_Widget");
        // Также сохраняем имя расширения в property для легкого доступа
        widget->setProperty("extensionName", extensionName);
    } else {
        qDebug() << "Failed to create widget for:" << extensionName;
    }

    return widget;
}

void ExtensionLayoutManager::loadExtensionSettings()
{
    qDebug() << "Loading extension settings...";

    QSettings settings("MyCompany", "DockApp");
    QList<QString> extensionNames;

    try {
        extensionNames = m_extensionManager.extensionsForLocation(ExtensionDisplayLocation::TopPanel);
    } catch (const std::exception& e) {
        qDebug() << "Exception in extensionsForLocation:" << e.what();
        return;
    } catch (...) {
        qDebug() << "Unknown exception in extensionsForLocation";
        return;
    }

    qDebug() << "Loading settings for extensions:" << extensionNames;

    for (const QString& extensionName : extensionNames) {
        bool enabled = settings.value(QString("Extensions/%1").arg(extensionName), true).toBool();
        m_topPanelData.extensionStates[extensionName] = enabled;
        qDebug() << "Extension" << extensionName << "enabled:" << enabled;
    }

    qDebug() << "Extension settings loaded";
}

void ExtensionLayoutManager::saveExtensionSetting(const QString& extensionName, bool enabled)
{
    qDebug() << "Saving extension setting:" << extensionName << "enabled:" << enabled;

    QSettings settings("MyCompany", "DockApp");
    settings.setValue(QString("Extensions/%1").arg(extensionName), enabled);

    qDebug() << "Extension setting saved";
}