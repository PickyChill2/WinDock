#ifndef EXTENSIONLAYOUTMANAGER_H
#define EXTENSIONLAYOUTMANAGER_H

#include <QObject>
#include <QHBoxLayout>
#include <QWidget>
#include <QMap>
#include <QDebug>
#include "ExtensionManager.h"

class ExtensionLayoutManager : public QObject
{
    Q_OBJECT

public:
    explicit ExtensionLayoutManager(QObject* parent = nullptr);
    ~ExtensionLayoutManager();

    // Инициализация для TopPanel
    void initializeTopPanel(QWidget* parent,
                           QHBoxLayout* leftLayout,
                           int panelHeight);

    // Инициализация для Dock
    void initializeDock(QWidget* parent,
                       QHBoxLayout* leftLayout,
                       QHBoxLayout* rightLayout,
                       int iconSize);

    // Загрузка расширений для указанного расположения
    void loadExtensions(ExtensionDisplayLocation location);

    // Обновление видимости расширений
    void updateExtensionsVisibility(ExtensionDisplayLocation location);

    // Обработка переключения расширения
    void onExtensionToggled(const QString& extensionName, bool enabled);

    // Получение виджета расширения по имени
    QWidget* getExtensionWidget(const QString& extensionName) const;

    // Получение состояния расширения
    bool getExtensionState(const QString& extensionName) const;

    // Очистка всех расширений
    void clearExtensions();

private:
    // Создание виджета расширения
    QWidget* createExtensionWidget(const QString& extensionName, QWidget* parent);

    // Загрузка настроек расширений
    void loadExtensionSettings();

    // Сохранение настроек расширения
    void saveExtensionSetting(const QString& extensionName, bool enabled);

    // Удаление виджета расширения из дока
    void removeDockExtensionWidget(const QString& extensionName, ExtensionDisplayLocation location);

    // Структура для хранения данных панели
    struct PanelData {
        QWidget* parent = nullptr;
        QHBoxLayout* leftLayout = nullptr;
        QHBoxLayout* rightLayout = nullptr;
        QMap<QString, QWidget*> extensionWidgets;
        QMap<QString, bool> extensionStates;
        int panelHeight = 0;
        int iconSize = 0;

        PanelData()
            : parent(nullptr)
            , leftLayout(nullptr)
            , rightLayout(nullptr)
            , panelHeight(0)
            , iconSize(0)
        {}
    };

    PanelData m_topPanelData;
    PanelData m_dockData;
    ExtensionManager& m_extensionManager;
};

#endif // EXTENSIONLAYOUTMANAGER_H