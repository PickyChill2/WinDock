#include "LangIndicatorExtension.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

LangOverlayWidget::LangOverlayWidget(QWidget* parent)
    : QWidget(parent)
    , m_overlay(nullptr)
    , m_langLabel(nullptr)
    , m_fadeAnimation(nullptr)
    , m_opacityEffect(nullptr)
    , m_hideTimer(nullptr)
    , m_checkTimer(nullptr)
    , m_currentLang("EN")
    , m_previousLang("")
    , m_currentLangIndex(0)
    , m_isDarkMode(false)
    , m_isVisible(false)
{
    // Этот виджет невидимый, только мониторит язык
    setFixedSize(1, 1);
    hide();

    // Проверяем версию Windows
#ifdef Q_OS_WIN
    if (!checkWindowsVersion()) {
        qDebug() << "LangOverlay requires Windows 10/11";
        return;
    }
#endif

    // Проверяем тему
    QPalette palette = QApplication::palette();
    m_isDarkMode = palette.window().color().lightness() < 128;

    // Настройка таймеров
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(100); // Проверяем каждые 100мс
    connect(m_checkTimer, &QTimer::timeout, this, &LangOverlayWidget::checkLanguageChange);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &LangOverlayWidget::hideOverlay);

    // Настраиваем оверлей
    setupOverlay();

    // Получаем начальный язык
#ifdef Q_OS_WIN
    updateCurrentLanguage();
    m_previousLang = m_currentLang;
#endif

    // Запускаем проверку
    m_checkTimer->start();

    // Устанавливаем фильтр событий для отслеживания изменений темы
    qApp->installEventFilter(this);
}

LangOverlayWidget::~LangOverlayWidget()
{
    if (m_overlay) {
        m_overlay->deleteLater();
    }
}

void LangOverlayWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    // Этот виджет невидимый, ничего не рисуем
}

bool LangOverlayWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        QPalette palette = QApplication::palette();
        m_isDarkMode = palette.window().color().lightness() < 128;

        // Обновляем стиль оверлея если он видим
        if (m_overlay && m_isVisible) {
            if (m_isDarkMode) {
                m_overlay->setStyleSheet(
                    "background-color: rgba(30, 30, 30, 200);"
                    "border: 2px solid #444;"
                    "border-radius: 15px;"
                );
                if (m_langLabel) {
                    m_langLabel->setStyleSheet(
                        "color: white;"
                        "font-size: 72px;"
                        "font-weight: bold;"
                        "padding: 20px;"
                    );
                }
            } else {
                m_overlay->setStyleSheet(
                    "background-color: rgba(240, 240, 240, 200);"
                    "border: 2px solid #888;"
                    "border-radius: 15px;"
                );
                if (m_langLabel) {
                    m_langLabel->setStyleSheet(
                        "color: black;"
                        "font-size: 72px;"
                        "font-weight: bold;"
                        "padding: 20px;"
                    );
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void LangOverlayWidget::checkLanguageChange()
{
#ifdef Q_OS_WIN
    updateCurrentLanguage();

    if (m_currentLang != m_previousLang) {
        qDebug() << "Language changed from" << m_previousLang << "to" << m_currentLang;
        showOverlay();
        m_previousLang = m_currentLang;
    }
#endif
}

void LangOverlayWidget::showOverlay()
{
    if (!m_overlay || !m_langLabel) return;

    // Обновляем текст
    m_langLabel->setText(m_currentLang);

    // Позиционируем оверлей
    positionOverlay();

    // Показываем оверлей
    m_overlay->show();
    m_overlay->raise();

    // Запускаем анимацию появления
    m_opacityEffect->setOpacity(0.0);
    m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
    m_fadeAnimation->start();

    m_isVisible = true;

    // Запускаем таймер скрытия
    m_hideTimer->start(SHOW_TIMER);
}

void LangOverlayWidget::hideOverlay()
{
    if (!m_overlay || !m_isVisible) return;

    m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
    m_fadeAnimation->start();
}

void LangOverlayWidget::onAnimationFinished()
{
    if (m_fadeAnimation->direction() == QPropertyAnimation::Backward) {
        m_overlay->hide();
        m_isVisible = false;
    }
}

#ifdef Q_OS_WIN
void LangOverlayWidget::updateCurrentLanguage()
{
    HKL currentHKL = getCurrentKeyboardLayout();
    m_currentLang = getLanguageName(currentHKL);

    // Получаем индекс языка в списке для анимации
    HKL hklList[10];
    int count = GetKeyboardLayoutList(10, hklList);

    m_currentLangIndex = 0;
    for (int i = 0; i < count; i++) {
        if (hklList[i] == currentHKL) {
            m_currentLangIndex = i;
            break;
        }
    }
}

QString LangOverlayWidget::getLanguageName(HKL hkl)
{
    LANGID langId = LOWORD(hkl);

    switch (PRIMARYLANGID(langId)) {
        case LANG_ENGLISH: return "EN";
        case LANG_RUSSIAN: return "RU";
        case LANG_UKRAINIAN: return "UA";
        case LANG_GERMAN: return "DE";
        case LANG_FRENCH: return "FR";
        case LANG_SPANISH: return "ES";
        case LANG_CHINESE: return "CN";
        case LANG_JAPANESE: return "JP";
        case LANG_KOREAN: return "KR";
        case LANG_ARABIC: return "AR";
        case LANG_TURKISH: return "TR";
        case LANG_ITALIAN: return "IT";
        case LANG_PORTUGUESE: return "PT";
        case LANG_POLISH: return "PL";
        case LANG_CZECH: return "CZ";
        case LANG_DUTCH: return "NL";
        case LANG_SWEDISH: return "SE";
        case LANG_GREEK: return "GR";
        case LANG_HUNGARIAN: return "HU";
        case LANG_FINNISH: return "FI";
        case LANG_DANISH: return "DK";
        case LANG_NORWEGIAN: return "NO";
        case LANG_HEBREW: return "HE";
        default: return "??";
    }
}

HKL LangOverlayWidget::getCurrentKeyboardLayout()
{
    // Получаем раскладку активного окна
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow) {
        DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
        return GetKeyboardLayout(threadId);
    }

    // Если активного окна нет, используем текущий поток
    return GetKeyboardLayout(0);
}

bool LangOverlayWidget::checkWindowsVersion()
{
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };
    GetVersionEx((OSVERSIONINFO*)&osvi);

    // Windows 10 = 10.0, Windows 11 = 10.0 (но с build number >= 22000)
    return (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0);
}
#endif

void LangOverlayWidget::setupOverlay()
{
    // Создаем оверлейное окно
    m_overlay = new QWidget();
    m_overlay->setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool |
        Qt::X11BypassWindowManagerHint
    );
    m_overlay->setAttribute(Qt::WA_TranslucentBackground);
    m_overlay->setFixedSize(W_SIZE, H_SIZE);

    // Настраиваем эффект прозрачности
    m_opacityEffect = new QGraphicsOpacityEffect(m_overlay);
    m_overlay->setGraphicsEffect(m_opacityEffect);

    // Настраиваем анимацию
    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", m_overlay);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_fadeAnimation, &QPropertyAnimation::finished,
            this, &LangOverlayWidget::onAnimationFinished);

    // Настраиваем layout
    QVBoxLayout* layout = new QVBoxLayout(m_overlay);
    layout->setContentsMargins(0, 0, 0, 0);

    // Создаем метку для языка
    m_langLabel = new QLabel("EN");
    m_langLabel->setAlignment(Qt::AlignCenter);

    if (m_isDarkMode) {
        m_overlay->setStyleSheet(
            "background-color: rgba(30, 30, 30, 200);"
            "border: 2px solid #444;"
            "border-radius: 15px;"
        );
        m_langLabel->setStyleSheet(
            "color: white;"
            "font-size: 72px;"
            "font-weight: bold;"
            "padding: 20px;"
        );
    } else {
        m_overlay->setStyleSheet(
            "background-color: rgba(240, 240, 240, 200);"
            "border: 2px solid #888;"
            "border-radius: 15px;"
        );
        m_langLabel->setStyleSheet(
            "color: black;"
            "font-size: 72px;"
            "font-weight: bold;"
            "padding: 20px;"
        );
    }

    layout->addWidget(m_langLabel);

    // Изначально скрываем
    m_overlay->hide();
}

void LangOverlayWidget::positionOverlay()
{
    if (!m_overlay) return;

    // Получаем геометрию основного экрана
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    QRect screenGeometry = primaryScreen->geometry();

    // Позиционируем по центру экрана
    int x = screenGeometry.center().x() - m_overlay->width() / 2;
    int y = screenGeometry.center().y() - m_overlay->height() / 2;

    m_overlay->move(x, y);
}

QWidget* LangIndicatorExtension::createWidget(QWidget* parent)
{
    // Создаем виджет, который будет мониторить язык
    LangOverlayWidget* widget = new LangOverlayWidget(parent);

#ifdef Q_OS_WIN
    // Проверяем версию Windows
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };
    GetVersionEx((OSVERSIONINFO*)&osvi);

    // Windows 10 = 10.0, Windows 11 = 10.0
    if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0) {
        return widget;
    } else {
        qDebug() << "LangOverlay extension is only supported on Windows 10/11";
        widget->deleteLater();
        return nullptr;
    }
#else
    qDebug() << "LangOverlay extension is only supported on Windows";
    widget->deleteLater();
    return nullptr;
#endif
}