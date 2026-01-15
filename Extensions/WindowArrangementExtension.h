#ifndef WINDOWARRANGEMENTEXTENSION_H
#define WINDOWARRANGEMENTEXTENSION_H

#include "../ExtensionManager.h"
#include <QObject>
#include <QSettings>
#include <QTimer>

class WindowArrangementExtension : public QObject, public ExtensionInterface
{
    Q_OBJECT

public:
    WindowArrangementExtension();
    ~WindowArrangementExtension();

    // ExtensionInterface implementation
    QString name() const override { return "WindowArrangementExtension"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;
    ExtensionDisplayLocation displayLocation() const override { return ExtensionDisplayLocation::None; }

private slots:
    void onSettingsChanged();

private:
    void setEnabled(bool enabled);

    bool m_enabled;
    QTimer* m_settingsTimer;
    QSettings* m_settings;
};

#endif // WINDOWARRANGEMENTEXTENSION_H