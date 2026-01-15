#ifndef EXPLORERKILLEREXTENSION_H
#define EXPLORERKILLEREXTENSION_H

#include "../ExtensionManager.h"
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

class ExplorerKillerWidget : public QWidget
{
public:
    explicit ExplorerKillerWidget(QWidget* parent = nullptr);
    ~ExplorerKillerWidget();

private slots:
    void killExplorerProcesses();
    void updateExplorerCount();

private:
    QPushButton* m_killButton;
    QTimer* m_timer;

    // Константы размеров
    static constexpr int WIDGET_HEIGHT = 40;
    static constexpr int BUTTON_WIDTH = 200;
    static constexpr int HORIZONTAL_MARGIN = 10;
    static constexpr int VERTICAL_MARGIN = 5;
    static constexpr int FONT_SIZE = 10;
    static constexpr int BORDER_RADIUS = 8;
    static constexpr int PADDING_HORIZONTAL = 15;
    static constexpr int PADDING_VERTICAL = 8;
    static constexpr int UPDATE_INTERVAL_MS = 1000;
};

class ExplorerKillerExtension : public ExtensionInterface
{
public:
    ExplorerKillerExtension() = default;
    ~ExplorerKillerExtension() = default;

    QString name() const override { return "ExplorerKiller"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;

    ExtensionDisplayLocation displayLocation() const override {
        return ExtensionDisplayLocation::Calendar;
    }
};

REGISTER_EXTENSION_CONDITIONAL(ExplorerKillerExtension)

#endif // EXPLORERKILLEREXTENSION_H