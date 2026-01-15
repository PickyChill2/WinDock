// ExplorerKillerExtension.cpp
#include "ExplorerKiller.h"
#include <QApplication>
#include <QFontMetrics>

ExplorerKillerWidget::ExplorerKillerWidget(QWidget* parent)
    : QWidget(parent)
    , m_killButton(nullptr)
    , m_timer(nullptr)
{
    setFixedHeight(WIDGET_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(HORIZONTAL_MARGIN, VERTICAL_MARGIN, HORIZONTAL_MARGIN, VERTICAL_MARGIN);
    layout->setSpacing(10);

    // Создаем кнопку
    m_killButton = new QPushButton(this);

    // Настраиваем стиль кнопки
    QString buttonStyle =
        "QPushButton {"
        "    background-color: rgba(255, 100, 100, 80);"
        "    border: 1px solid rgba(255, 100, 100, 120);"
        "    border-radius: " + QString::number(BORDER_RADIUS) + "px;"
        "    padding: " + QString::number(PADDING_VERTICAL) + "px " + QString::number(PADDING_HORIZONTAL) + "px;"
        "    color: white;"
        "    font-size: " + QString::number(FONT_SIZE) + "px;"
        "    font-family: 'Segoe UI', Arial, sans-serif;"
        "    font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(255, 100, 100, 120);"
        "    border: 1px solid rgba(255, 100, 100, 180);"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(255, 70, 70, 150);"
        "}";

    m_killButton->setStyleSheet(buttonStyle);
    m_killButton->setFixedSize(BUTTON_WIDTH, WIDGET_HEIGHT - 2 * VERTICAL_MARGIN);

    // Создаем и настраиваем таймер
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ExplorerKillerWidget::updateExplorerCount);
    m_timer->start(UPDATE_INTERVAL_MS);

    // Подключаем сигнал
    connect(m_killButton, &QPushButton::clicked, this, &ExplorerKillerWidget::killExplorerProcesses);

    layout->addWidget(m_killButton);
    layout->setAlignment(Qt::AlignCenter);

    // Первоначальное обновление счетчика
    updateExplorerCount();
}

ExplorerKillerWidget::~ExplorerKillerWidget()
{
}

void ExplorerKillerWidget::updateExplorerCount()
{
#ifdef Q_OS_WIN
    int count = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe)) {
            do {
                if (wcscmp(pe.szExeFile, L"explorer.exe") == 0) {
                    count++;
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }

    m_killButton->setText(QString("Отсутвует - %1 комплектов брони").arg(count));
#else
    m_killButton->setText("Kill Explorer Processes - N/A");
#endif
}

void ExplorerKillerWidget::killExplorerProcesses()
{
    m_timer->stop(); // Останавливаем таймер на время выполнения операции

#ifdef Q_OS_WIN
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        QMessageBox::warning(this, "Error", "Failed to create process snapshot");
        m_timer->start(UPDATE_INTERVAL_MS);
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    int killedCount = 0;

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (wcscmp(pe.szExeFile, L"explorer.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 0)) {
                        killedCount++;
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    if (killedCount > 0) {
        QMessageBox::information(this, "Диалог",
            QString("Вот она правда: вы допустили потерю дорогостоящего обмундирования. "
                    "Его стоимость будет вычтена из вашего жалованья, и вы будете служить, пока вам не исполнится пятьсот десять лет, "
                    "потому что вам понадобится именно столько лет, чтобы оплатить комплект Силовой боевой брони модель II, который вы потеряли! "
                    "Доложите об этом в арсенале, получите новый комплект, а потом вернитесь и доложите мне, рядовой! Свободны!").arg(killedCount));
    } else {
        QMessageBox::information(this, "Не шутить тут", "Хватит тратить мое время на шутки рядовой, обратитесь в арсенал и получите новый комплект. Свободны!");
    }
#else
    QMessageBox::information(this, "Info", "This feature is only available on Windows");
#endif

    // Перезапускаем таймер и обновляем счетчик
    m_timer->start(UPDATE_INTERVAL_MS);
    updateExplorerCount();
}

QWidget* ExplorerKillerExtension::createWidget(QWidget* parent)
{
    return new ExplorerKillerWidget(parent);
}