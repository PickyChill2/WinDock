#ifndef LangIndicatorExtension_H
#define LangIndicatorExtension_H

#include "../ExtensionManager.h"
#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

const int SHOW_TIMER = 450;
const int W_SIZE = 150;
const int H_SIZE = 150;

class LangOverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LangOverlayWidget(QWidget* parent = nullptr);
    ~LangOverlayWidget();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void checkLanguageChange();
    void showOverlay();
    void hideOverlay();
    void onAnimationFinished();

private:
#ifdef Q_OS_WIN
    void updateCurrentLanguage();
    QString getLanguageName(HKL hkl);
    HKL getCurrentKeyboardLayout();
    bool checkWindowsVersion();
#endif

    void setupOverlay();
    void positionOverlay();

    QWidget* m_overlay;
    QLabel* m_langLabel;
    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    QTimer* m_hideTimer;
    QTimer* m_checkTimer;

    QString m_currentLang;
    QString m_previousLang;
    int m_currentLangIndex;
    bool m_isDarkMode;
    bool m_isVisible;
};

class LangIndicatorExtension : public ExtensionInterface
{
public:
    LangIndicatorExtension() = default;
    ~LangIndicatorExtension() = default;

    QString name() const override { return "LangOverlay"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;

    // Не отображаем нигде - это скрытый виджет для мониторинга языка
    bool shouldDisplayInTopPanel() const override { return false; }
    bool shouldDisplayInCalendar() const override { return false; }
    bool shouldDisplayInDockLeft() const override { return false; }
    bool shouldDisplayInDockRight() const override { return false; }

    ExtensionDisplayLocation displayLocation() const override {
        return ExtensionDisplayLocation::None;
    }
};

REGISTER_EXTENSION(LangIndicatorExtension)

#endif // LangIndicatorExtension_H