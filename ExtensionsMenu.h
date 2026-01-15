#ifndef EXTENSIONSMENU_H
#define EXTENSIONSMENU_H

#include <QMenu>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QWidgetAction>
#include <QMap>
#include <QSettings>
#include <QProcess>
#include <QApplication>
#include "ExtensionManager.h"
#include "TopPanelConstants.h"

class ExtensionsMenu : public QMenu
{
    Q_OBJECT

public:
    explicit ExtensionsMenu(QWidget* parent = nullptr);

    signals:
        void extensionToggled(const QString& extensionName, bool enabled);
    void restartRequested();

private slots:
    void onExtensionToggled(const QString& extensionName, bool enabled);
    void refreshExtensionsList();
    void restartApplication();

private:
    void setupUI();
    void createExtensionItem(const QString& extensionName);
    void loadExtensionStates();
    void saveExtensionState(const QString& extensionName, bool enabled);
    void updateExtensionsConfigFile(const QString& extensionName, bool enabled);

    QMap<QString, QSlider*> m_extensionSliders;
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_extensionsLayout;
    QWidget* m_extensionsContainer;
    QSettings m_settings;
};

#endif // EXTENSIONSMENU_H