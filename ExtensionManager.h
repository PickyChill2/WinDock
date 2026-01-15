#ifndef EXTENSIONMANAGER_H
#define EXTENSIONMANAGER_H

#include <QObject>
#include <QList>
#include <QWidget>
#include <QString>
#include <QMap>
#include <QSet>

// Перечисление для указания места отображения расширения
enum class ExtensionDisplayLocation {
    None = 0,
    TopPanel,    // Верхняя панель
    Calendar,    // Под календарем
    DockLeft,    // Левая часть дока
    DockRight,   // Правая часть дока
    Both         // В обоих местах
};

// Перечисление для типа контекстного меню в доке
enum class DockContextMenuType {
    DockGlobal,      // Контекстное меню всего дока
    DockItem,        // Контекстное меню элемента дока
    RunningAppItem   // Контекстное меню запущенного приложения
};

class DockItem; // Forward declaration

class ExtensionInterface {
public:
    virtual ~ExtensionInterface() = default;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;

    // Новый метод для указания места отображения
    virtual ExtensionDisplayLocation displayLocation() const { return ExtensionDisplayLocation::TopPanel; }

    // Метод для проверки, должно ли расширение отображаться в календаре
    virtual bool shouldDisplayInCalendar() const {
        return displayLocation() == ExtensionDisplayLocation::Calendar ||
               displayLocation() == ExtensionDisplayLocation::Both;
    }

    // Метод для проверки, должно ли расширение отображаться в верхней панели
    virtual bool shouldDisplayInTopPanel() const {
        return displayLocation() == ExtensionDisplayLocation::TopPanel ||
               displayLocation() == ExtensionDisplayLocation::Both;
    }

    // Метод для проверки, должно ли расширение отображаться в левой части дока
    virtual bool shouldDisplayInDockLeft() const {
        return displayLocation() == ExtensionDisplayLocation::DockLeft ||
               displayLocation() == ExtensionDisplayLocation::Both;
    }

    // Метод для проверки, должно ли расширение отображаться в правой части дока
    virtual bool shouldDisplayInDockRight() const {
        return displayLocation() == ExtensionDisplayLocation::DockRight ||
               displayLocation() == ExtensionDisplayLocation::Both;
    }

    // Новые методы для работы с контекстным меню дока
    virtual void populateDockContextMenu(QMenu* menu, DockContextMenuType menuType, DockItem* item = nullptr) {
        Q_UNUSED(menu)
        Q_UNUSED(menuType)
        Q_UNUSED(item)
    }
};

class ExtensionManager : public QObject
{
    Q_OBJECT

public:
    static ExtensionManager& instance();

    void registerExtension(const QString& name, ExtensionInterface* (*creator)());
    QList<QString> extensionNames() const;
    QWidget* createExtensionWidget(const QString& name, QWidget* parent = nullptr);
    void removeExtension(const QString& name);
    void reloadExtensions();

    // Методы для управления расширениями через конфиг
    bool loadExtensionsFromConfig(const QString& configPath = "extensions.txt");
    QSet<QString> getEnabledExtensions() const { return m_enabledExtensions; }
    QSet<QString> getAllAvailableExtensions() const { return m_availableExtensions; }
    bool isExtensionEnabled(const QString& name) const { return m_enabledExtensions.contains(name); }

    // Новый метод для получения расширений для конкретного места отображения
    QList<QString> extensionsForLocation(ExtensionDisplayLocation location) const;

    // Новый метод для получения экземпляра расширения
    ExtensionInterface* getExtensionInstance(const QString& name);

    // Новый метод для отложенной регистрации расширений
    void registerPendingExtensions();

    // Новый метод для заполнения контекстного меню дока
    void populateDockContextMenu(QMenu* menu, DockContextMenuType menuType, DockItem* item = nullptr);

signals:
    void extensionLoaded(const QString& name);
    void extensionUnloaded(const QString& name);
    void extensionsChanged();

private:
    ExtensionManager();
    ~ExtensionManager();

    void scanAndLoadExtensions();
    void loadExtension(const QString& name);

    QMap<QString, ExtensionInterface* (*)()> m_extensionCreators;
    QMap<QString, ExtensionInterface*> m_extensions;
    QSet<QString> m_enabledExtensions;
    QSet<QString> m_availableExtensions;

    // Для отложенной регистрации
    QMap<QString, ExtensionInterface* (*)()> m_pendingExtensions;
};

// Упрощенный макрос для регистрации расширений
#define REGISTER_EXTENSION(ExtensionClass) \
static ExtensionInterface* create##ExtensionClass() { \
    return new ExtensionClass(); \
} \
static struct ExtensionClass##Registry { \
    ExtensionClass##Registry() { \
        ExtensionManager::instance().registerExtension(#ExtensionClass, create##ExtensionClass); \
    } \
} ExtensionClass##Instance;

// Макрос для условной регистрации (использует отложенную регистрацию)
#define REGISTER_EXTENSION_CONDITIONAL(ExtensionClass) \
static ExtensionInterface* create##ExtensionClass() { \
    return new ExtensionClass(); \
} \
static struct ExtensionClass##Registry { \
    ExtensionClass##Registry() { \
        ExtensionManager::instance().registerExtension(#ExtensionClass, create##ExtensionClass); \
    } \
} ExtensionClass##Instance;

#endif // EXTENSIONMANAGER_H