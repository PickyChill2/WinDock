#include "TimerExtension.h"
#include "../TimerNotification.h"
#include <QApplication>
#include "../TopPanelConstants.h"

TimerWidget::TimerWidget(QWidget* parent)
    : QWidget(parent)
    , m_timerLabel(nullptr)
    , m_countdownTimer(new QTimer(this))
    , m_blinkTimer(new QTimer(this))
    , m_timerMenu(nullptr)
    , m_timeInput(nullptr)
    , m_stopButton(nullptr)
    , m_remainingSeconds(0)
    , m_timerActive(false)
    , m_blinkState(false)
    , m_notification5MinShown(false)
    , m_notification1MinShown(false)
    , m_notification30SecShown(false)
{
    // Используем высоту панели из TopPanelConstants
    setFixedHeight(TopPanelConstants::PANEL_HEIGHT);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(5);
    layout->setAlignment(Qt::AlignVCenter);

    m_timerLabel = new QLabel(this);
    m_timerLabel->setStyleSheet(
        "color: white; "
        "font-size: 12px; "
        "background: transparent; "
        "padding: 0px; "
        "margin: 0px;"
    );
    m_timerLabel->setText("Таймер: --:--");

    layout->addWidget(m_timerLabel);

    setupMenu();

    connect(m_countdownTimer, &QTimer::timeout, this, &TimerWidget::updateTimerDisplay);
    connect(m_blinkTimer, &QTimer::timeout, this, &TimerWidget::updateBlinkEffect);
}

TimerWidget::~TimerWidget()
{
    if (m_timerMenu) {
        m_timerMenu->deleteLater();
    }
}

void TimerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        updateMenuPosition();
        m_timerMenu->exec(mapToGlobal(QPoint(0, height())));
    }
    QWidget::mousePressEvent(event);
}

void TimerWidget::setupMenu()
{
    m_timerMenu = new QMenu(this);
    m_timerMenu->setStyleSheet(
        "QMenu {"
        "    background-color: #2b2b2b;"
        "    border: 1px solid #666;"
        "    border-radius: 5px;"
        "    padding: 5px;"
        "}"
        "QLineEdit {"
        "    background: white;"
        "    color: black;"
        "    border: 1px solid #ccc;"
        "    border-radius: 3px;"
        "    padding: 2px 5px;"
        "    font-size: 12px;"
        "}"
        "QPushButton {"
        "    background: #555;"
        "    color: white;"
        "    border: 1px solid #666;"
        "    border-radius: 3px;"
        "    padding: 5px;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background: #666;"
        "}"
        "QPushButton:disabled {"
        "    background: #333;"
        "    color: #777;"
        "}"
    );

    QWidget* menuWidget = new QWidget();
    QVBoxLayout* menuLayout = new QVBoxLayout(menuWidget);
    menuLayout->setSpacing(5);

    // Input field - теперь в минутах
    m_timeInput = new QLineEdit();
    m_timeInput->setPlaceholderText("Введите время (минуты)");
    m_timeInput->setValidator(new QIntValidator(0, 1440, this)); // Максимум 24 часа
    menuLayout->addWidget(m_timeInput);

    // Control buttons
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    QPushButton* decreaseBtn = new QPushButton("-5");
    QPushButton* startBtn = new QPushButton("Старт");
    QPushButton* increaseBtn = new QPushButton("+5");
    m_stopButton = new QPushButton("Стоп");

    decreaseBtn->setFixedWidth(40);
    increaseBtn->setFixedWidth(40);
    startBtn->setMinimumWidth(60);
    m_stopButton->setMinimumWidth(60);
    m_stopButton->setEnabled(false);

    buttonsLayout->addWidget(decreaseBtn);
    buttonsLayout->addWidget(startBtn);
    buttonsLayout->addWidget(increaseBtn);
    buttonsLayout->addWidget(m_stopButton);
    menuLayout->addLayout(buttonsLayout);

    connect(decreaseBtn, &QPushButton::clicked, this, &TimerWidget::decreaseTime);
    connect(increaseBtn, &QPushButton::clicked, this, &TimerWidget::increaseTime);
    connect(startBtn, &QPushButton::clicked, this, &TimerWidget::startTimer);
    connect(m_stopButton, &QPushButton::clicked, this, &TimerWidget::stopTimer);
    connect(m_timeInput, &QLineEdit::returnPressed, this, &TimerWidget::handleTextInput);

    QWidgetAction* widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(menuWidget);
    m_timerMenu->addAction(widgetAction);
}

void TimerWidget::updateMenuPosition()
{
    if (m_timerMenu) {
        QPoint pos = mapToGlobal(QPoint(0, height()));

        QScreen* screen = window()->screen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();

            if (pos.y() + m_timerMenu->sizeHint().height() > screenGeometry.bottom()) {
                pos.setY(screenGeometry.bottom() - m_timerMenu->sizeHint().height());
            }

            if (pos.x() + m_timerMenu->sizeHint().width() > screenGeometry.right()) {
                pos.setX(screenGeometry.right() - m_timerMenu->sizeHint().width());
            }
        }

        m_timerMenu->move(pos);
    }
}

void TimerWidget::updateTimerDisplay()
{
    if (m_timerActive && m_remainingSeconds > 0) {
        m_remainingSeconds--;

        int minutes = m_remainingSeconds / 60;
        int seconds = m_remainingSeconds % 60;

        m_timerLabel->setText(QString("Таймер: %1:%2")
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0')));

        // Проверка для уведомлений
        if (m_remainingSeconds <= 5 * 60 && !m_notification5MinShown) {
            showNotification("Осталось меньше 5 минут");
            m_notification5MinShown = true;
        } else if (m_remainingSeconds <= 60 && !m_notification1MinShown) {
            showNotification("Осталось меньше 1 минуты");
            m_notification1MinShown = true;
        } else if (m_remainingSeconds <= 30 && !m_notification30SecShown) {
            showNotification("Осталось меньше 30 секунд");
            m_notification30SecShown = true;
        }

        // Включаем мигание за 30 секунд до окончания
        if (m_remainingSeconds <= 30 && !m_blinkTimer->isActive()) {
            m_blinkTimer->start(500); // Мигание каждые 500 мс
        }
    } else {
        stopTimer();
        m_timerLabel->setText("Таймер: --:--");
    }
}

void TimerWidget::updateBlinkEffect()
{
    if (m_remainingSeconds <= 30) {
        m_blinkState = !m_blinkState;
        if (m_blinkState) {
            m_timerLabel->setStyleSheet(
                "color: red; "
                "font-size: 12px; "
                "background: transparent; "
                "padding: 0px; "
                "margin: 0px;"
            );
        } else {
            m_timerLabel->setStyleSheet(
                "color: white; "
                "font-size: 12px; "
                "background: transparent; "
                "padding: 0px; "
                "margin: 0px;"
            );
        }
    }
}

void TimerWidget::showNotification(const QString& message)
{
    TimerNotification* notification = new TimerNotification(message);
    notification->showNotification();
}

void TimerWidget::startTimer()
{
    int minutes = validateAndGetTime(); // Получаем минуты
    if (minutes > 0) {
        m_remainingSeconds = minutes * 60; // Конвертируем минуты в секунды
        m_timerActive = true;
        m_countdownTimer->start(1000);
        m_blinkTimer->stop(); // Останавливаем мигание при старте
        m_timerLabel->setStyleSheet(
            "color: white; "
            "font-size: 12px; "
            "background: transparent; "
            "padding: 0px; "
            "margin: 0px;"
        );
        m_stopButton->setEnabled(true);

        // Сбрасываем флаги уведомлений
        m_notification5MinShown = false;
        m_notification1MinShown = false;
        m_notification30SecShown = false;

        m_timerMenu->hide();
        updateTimerDisplay();
    }
}

void TimerWidget::stopTimer()
{
    m_timerActive = false;
    m_countdownTimer->stop();
    m_blinkTimer->stop();
    m_remainingSeconds = 0;
    m_stopButton->setEnabled(false);
    m_timerLabel->setStyleSheet(
        "color: white; "
        "font-size: 12px; "
        "background: transparent; "
        "padding: 0px; "
        "margin: 0px;"
    );
    m_timerLabel->setText("Таймер: --:--");

    // Сбрасываем флаги уведомлений
    m_notification5MinShown = false;
    m_notification1MinShown = false;
    m_notification30SecShown = false;
}

void TimerWidget::increaseTime()
{
    int currentMinutes = validateAndGetTime(); // Получаем текущие минуты
    m_timeInput->setText(QString::number(currentMinutes + 5)); // Добавляем 5 минут
}

void TimerWidget::decreaseTime()
{
    int currentMinutes = validateAndGetTime(); // Получаем текущие минуты
    int newValue = qMax(0, currentMinutes - 5); // Вычитаем 5 минут
    m_timeInput->setText(QString::number(newValue));
}

void TimerWidget::handleTextInput()
{
    int minutes = validateAndGetTime(); // Просто валидируем ввод
    m_timeInput->setText(QString::number(minutes));
}

int TimerWidget::validateAndGetTime() const
{
    bool ok;
    int minutes = m_timeInput->text().toInt(&ok);
    return ok ? qMax(0, minutes) : 0; // Возвращаем минуты
}

QWidget* TimerExtension::createWidget(QWidget* parent)
{
    return new TimerWidget(parent);
}

// Явное включение MOC для решения проблем с линковкой
#include "TimerExtension.moc"