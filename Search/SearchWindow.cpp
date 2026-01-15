#include "SearchWindow.h"
#include "ApplicationSearcher.h"
#include "SearchResult.h"

#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <QApplication>
#include <QClipboard>
#include <QPropertyAnimation>
#include <QStandardPaths>
#include <QThread>
#include <QtMath>
#include <QInputDialog>
#include <QtConcurrent>
#include <QCheckBox>
#include <QMenu>

bool SearchWindow::isSearchEnabled()
{
    QSettings settings("MyCompany", "DockApp");
    return settings.value("General/SearchEnabled", true).toBool();
}

SearchWindow::SearchWindow(QWidget *parent)
    : QWidget(parent), m_isClosing(false), m_showResults(true),
      m_isSettingsVisible(false), m_appSearcher(nullptr),
      m_wereResultsVisible(false), m_useExplorer(true),
      m_contextMenu(nullptr), m_openLocationAction(nullptr)
{
    // Проверяем, включен ли поиск
    if (!isSearchEnabled()) {
        m_isClosing = true;
        close();
        deleteLater();
        return;
    }

    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    setStyleSheet("background: transparent; border: none;");

    // Устанавливаем фильтр событий для всего приложения
    qApp->installEventFilter(this);

    setupUI();
    setupSettingsUI();

    // Создаем контекстное меню
    m_contextMenu = new QMenu(this);
    m_openLocationAction = m_contextMenu->addAction("Открыть расположение файла");
    connect(m_openLocationAction, &QAction::triggered, this, &SearchWindow::onOpenFileLocation);

    // Загружаем пользовательские пути и настройку проводника
    loadCustomSearchPaths();
    loadUseExplorerSetting();

    // Если путей нет, добавляем стандартные
    if (m_customSearchPaths.isEmpty()) {
        // Добавляем стандартные пути
        QStringList defaultPaths;
        defaultPaths << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        defaultPaths << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        defaultPaths << QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

        for (const QString &path : defaultPaths) {
            if (!path.isEmpty() && !isPathInList(path)) {
                m_customSearchPaths.append(path);
            }
        }

        saveCustomSearchPaths();
    }

    // Создаем объект для поиска
    m_appSearcher = new ApplicationSearcher(this);

    // Запускаем кэширование в фоновом потоке, чтобы не блокировать UI
    QtConcurrent::run([this]() {
        refreshSearchPaths();
    });

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(200);
    connect(m_searchTimer, &QTimer::timeout, this, &SearchWindow::updateResults);
}

SearchWindow::~SearchWindow()
{
    m_isClosing = true;
    qDebug() << "SearchWindow destructor called";
}

void SearchWindow::setupUI()
{
    m_mainWidget = new QWidget(this);
    m_mainWidget->setObjectName("searchWidget");
    m_mainWidget->setStyleSheet(
        "#searchWidget {"
        "   background: rgba(45, 45, 45, 230);"
        "   border: 2px solid rgba(120, 120, 120, 255);"
        "   border-radius: 15px;"
        "}"
    );

    m_mainLayout = new QVBoxLayout(m_mainWidget);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);

    // Верхняя панель с поиском
    m_searchLayout = new QHBoxLayout();
    m_searchLayout->setSpacing(10);

    // Поле поиска
    m_searchEdit = new QLineEdit(m_mainWidget);
    m_searchEdit->setPlaceholderText("Введите запрос для поиска, математическое выражение или имя файла...");
    m_searchEdit->setFixedHeight(40);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "   background: rgba(60, 60, 60, 180);"
        "   border: 1px solid rgba(120, 120, 120, 255);"
        "   border-radius: 10px;"
        "   color: white;"
        "   padding-left: 15px;"
        "   padding-right: 15px;"
        "   font-size: 14px;"
        "   selection-background-color: rgba(100, 100, 255, 150);"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid rgba(150, 150, 255, 255);"
        "}"
    );

    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchWindow::onSearchTextChanged);

    // Кнопка поиска в интернете
    m_webSearchButton = new QPushButton("🌐", m_mainWidget);
    m_webSearchButton->setFixedSize(40, 40);
    m_webSearchButton->setToolTip("Поиск в интернете (Enter)");
    m_webSearchButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(70, 70, 150, 180);"
        "   border: 1px solid rgba(120, 120, 180, 255);"
        "   border-radius: 10px;"
        "   color: white;"
        "   font-size: 18px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(90, 90, 180, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(60, 60, 140, 200);"
        "}"
    );

    connect(m_webSearchButton, &QPushButton::clicked, [this]() {
        QString query = m_searchEdit->text().trimmed();
        if (!query.isEmpty()) {
            QString url = "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(query);
            QDesktopServices::openUrl(QUrl(url));
        }
        hideAndClose();
    });

    // Кнопка поиска на ПК
    m_pcSearchButton = new QPushButton("🔍", m_mainWidget);
    m_pcSearchButton->setFixedSize(40, 40);
    m_pcSearchButton->setToolTip("Поиск на компьютере (Ctrl+Enter)");
    m_pcSearchButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(70, 150, 70, 180);"
        "   border: 1px solid rgba(120, 180, 120, 255);"
        "   border-radius: 10px;"
        "   color: white;"
        "   font-size: 18px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(90, 180, 90, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(60, 140, 60, 200);"
        "}"
    );

    connect(m_pcSearchButton, &QPushButton::clicked, [this]() {
        QString query = m_searchEdit->text().trimmed();
        if (!query.isEmpty()) {
            if (m_resultsList->count() > 0) {
                QListWidgetItem *item = m_resultsList->item(0);
                if (item) {
                    onResultItemClicked(item);
                }
            }
        }
    });

    // Кнопка настроек
    m_settingsButton = new QPushButton("⚙", m_mainWidget);
    m_settingsButton->setFixedSize(40, 40);
    m_settingsButton->setToolTip("Настройки путей поиска");
    m_settingsButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(100, 100, 100, 180);"
        "   border: 1px solid rgba(150, 150, 150, 255);"
        "   border-radius: 10px;"
        "   color: white;"
        "   font-size: 18px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(120, 120, 120, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(80, 80, 80, 200);"
        "}"
    );

    connect(m_settingsButton, &QPushButton::clicked, this, &SearchWindow::onSettingsClicked);

    m_searchLayout->addWidget(m_searchEdit);
    m_searchLayout->addWidget(m_webSearchButton);
    m_searchLayout->addWidget(m_pcSearchButton);
    m_searchLayout->addWidget(m_settingsButton);

    // Список результатов
    m_resultsList = new QListWidget(m_mainWidget);
    m_resultsList->setStyleSheet(
        "QListWidget {"
        "   background: rgba(50, 50, 50, 180);"
        "   border: 1px solid rgba(100, 100, 100, 180);"
        "   border-radius: 8px;"
        "   color: white;"
        "   font-size: 13px;"
        "   outline: none;"
        "   padding: 5px;"
        "}"
        "QListWidget::item {"
        "   padding: 0px;"
        "   border-bottom: 1px solid rgba(80, 80, 80, 100);"
        "   background: transparent;"
        "}"
        "QListWidget::item:hover {"
        "   background: rgba(70, 70, 70, 150);"
        "   border-radius: 5px;"
        "}"
        "QListWidget::item:selected {"
        "   background: rgba(80, 80, 150, 200);"
        "   border-radius: 5px;"
        "}"
        "QScrollBar:vertical {"
        "   background: rgba(64, 64, 64, 150);"
        "   width: 6px;"
        "   border-radius: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(96, 96, 96, 200);"
        "   border-radius: 0px;"
        "   min-height: 20px;"
        "}"
    );
    m_resultsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsList->setFrameShape(QFrame::NoFrame);
    m_resultsList->setVisible(false);

    // Устанавливаем политику контекстного меню
    m_resultsList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_resultsList, &QListWidget::itemClicked, this, &SearchWindow::onResultItemClicked);
    connect(m_resultsList, &QListWidget::customContextMenuRequested,
            this, &SearchWindow::showContextMenuForItem);

    // Метка статуса
    m_statusLabel = new QLabel(m_mainWidget);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(180, 180, 180, 200);"
        "   font-size: 11px;"
        "   background: transparent;"
        "   padding: 5px;"
        "}"
    );
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setVisible(false);

    m_mainLayout->addLayout(m_searchLayout);
    m_mainLayout->addSpacing(10);
    m_mainLayout->addWidget(m_resultsList);
    m_mainLayout->addWidget(m_statusLabel);

    setLayout(new QVBoxLayout(this));
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->addWidget(m_mainWidget);

    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(40);
    m_shadowEffect->setColor(QColor(0, 0, 0, 100));
    m_shadowEffect->setOffset(0, 8);
    m_mainWidget->setGraphicsEffect(m_shadowEffect);

    setFixedWidth(600);
    setFixedHeight(80);
}

void SearchWindow::setupSettingsUI()
{
    // Создаем виджет настроек (изначально скрыт)
    m_settingsWidget = new QWidget(this);
    m_settingsWidget->setObjectName("settingsWidget");
    m_settingsWidget->setStyleSheet(
        "#settingsWidget {"
        "   background: rgba(50, 50, 50, 245);"
        "   border: 2px solid rgba(120, 120, 120, 255);"
        "   border-radius: 10px;"
        "   padding: 0px;"
        "}"
    );
    m_settingsWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_settingsWidget->setFixedSize(450, 350); // Увеличиваем высоту для чекбокса
    m_settingsWidget->setVisible(false);

    // Создаем эффект тени для виджета настроек
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(m_settingsWidget);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 150));
    shadowEffect->setOffset(0, 5);
    m_settingsWidget->setGraphicsEffect(shadowEffect);

    // Основной layout для настроек
    QVBoxLayout *settingsMainLayout = new QVBoxLayout(m_settingsWidget);
    settingsMainLayout->setSpacing(8);
    settingsMainLayout->setContentsMargins(12, 12, 12, 12);

    // Заголовок
    QLabel *titleLabel = new QLabel("Пути поиска:", m_settingsWidget);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "   background: transparent;"
        "}"
    );
    settingsMainLayout->addWidget(titleLabel);

    // Чекбокс для использования проводника
    m_useExplorerCheckBox = new QCheckBox("Использовать Проводник?", m_settingsWidget);
    m_useExplorerCheckBox->setChecked(m_useExplorer);
    m_useExplorerCheckBox->setStyleSheet(
        "QCheckBox {"
        "   color: white;"
        "   font-size: 11px;"
        "   background: transparent;"
        "   spacing: 6px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 16px;"
        "   height: 16px;"
        "   border: 1px solid rgba(150, 150, 150, 200);"
        "   border-radius: 3px;"
        "   background: rgba(60, 60, 60, 180);"
        "}"
        "QCheckBox::indicator:checked {"
        "   background: rgba(70, 150, 70, 200);"
        "}"
        "QCheckBox::indicator:hover {"
        "   border: 1px solid rgba(200, 200, 200, 255);"
        "}"
    );

    connect(m_useExplorerCheckBox, &QCheckBox::stateChanged, [this](int state) {
        m_useExplorer = (state == Qt::Checked);
        saveUseExplorerSetting();
    });

    settingsMainLayout->addWidget(m_useExplorerCheckBox);
    settingsMainLayout->addSpacing(5);

    // Список путей
    m_pathsList = new QListWidget(m_settingsWidget);
    m_pathsList->setStyleSheet(
        "QListWidget {"
        "   background: rgba(40, 40, 40, 200);"
        "   border: 1px solid rgba(100, 100, 100, 180);"
        "   border-radius: 5px;"
        "   color: white;"
        "   font-size: 11px;"
        "   outline: none;"
        "   padding: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 8px;"
        "   border-bottom: 1px solid rgba(80, 80, 80, 100);"
        "   background: transparent;"
        "}"
        "QListWidget::item:hover {"
        "   background: rgba(70, 70, 70, 150);"
        "   border-radius: 3px;"
        "}"
        "QListWidget::item:selected {"
        "   background: rgba(80, 80, 150, 200);"
        "   border-radius: 3px;"
        "}"
    );
    m_pathsList->setMinimumHeight(150);
    m_pathsList->setMaximumHeight(200);
    settingsMainLayout->addWidget(m_pathsList);

    // Кнопки
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(6);

    m_addPathButton = new QPushButton("Добавить", m_settingsWidget);
    m_addPathButton->setFixedHeight(30);
    m_addPathButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(70, 150, 70, 180);"
        "   border: 1px solid rgba(120, 180, 120, 255);"
        "   border-radius: 5px;"
        "   color: white;"
        "   font-size: 11px;"
        "   padding: 0px 8px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(90, 180, 90, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(60, 140, 60, 200);"
        "}"
    );

    m_removePathButton = new QPushButton("Удалить", m_settingsWidget);
    m_removePathButton->setFixedHeight(30);
    m_removePathButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(150, 70, 70, 180);"
        "   border: 1px solid rgba(180, 120, 120, 255);"
        "   border-radius: 5px;"
        "   color: white;"
        "   font-size: 11px;"
        "   padding: 0px 8px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(180, 90, 90, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(140, 60, 60, 200);"
        "}"
    );

    m_removeAllPathsButton = new QPushButton("Очистить", m_settingsWidget);
    m_removeAllPathsButton->setFixedHeight(30);
    m_removeAllPathsButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(150, 100, 70, 180);"
        "   border: 1px solid rgba(180, 140, 120, 255);"
        "   border-radius: 5px;"
        "   color: white;"
        "   font-size: 11px;"
        "   padding: 0px 8px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(180, 130, 90, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(140, 90, 60, 200);"
        "}"
    );

    m_closeSettingsButton = new QPushButton("Закрыть", m_settingsWidget);
    m_closeSettingsButton->setFixedHeight(30);
    m_closeSettingsButton->setStyleSheet(
        "QPushButton {"
        "   background: rgba(100, 100, 100, 180);"
        "   border: 1px solid rgba(150, 150, 150, 255);"
        "   border-radius: 5px;"
        "   color: white;"
        "   font-size: 11px;"
        "   padding: 0px 8px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(120, 120, 120, 180);"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(80, 80, 80, 200);"
        "}"
    );

    buttonsLayout->addWidget(m_addPathButton);
    buttonsLayout->addWidget(m_removePathButton);
    buttonsLayout->addWidget(m_removeAllPathsButton);
    buttonsLayout->addWidget(m_closeSettingsButton);

    settingsMainLayout->addLayout(buttonsLayout);

    // Подключаем сигналы
    connect(m_addPathButton, &QPushButton::clicked, this, &SearchWindow::onAddPathClicked);
    connect(m_removePathButton, &QPushButton::clicked, this, &SearchWindow::onRemovePathClicked);
    connect(m_removeAllPathsButton, &QPushButton::clicked, this, &SearchWindow::onRemoveAllPathsClicked);
    connect(m_closeSettingsButton, &QPushButton::clicked, this, &SearchWindow::onCloseSettingsClicked);
}

void SearchWindow::saveUseExplorerSetting()
{
    QSettings settings("MyCompany", "DockApp");
    settings.setValue("SearchPaths/UseExplorer", m_useExplorer);
}

void SearchWindow::loadUseExplorerSetting()
{
    QSettings settings("MyCompany", "DockApp");
    m_useExplorer = settings.value("SearchPaths/UseExplorer", true).toBool();
}

void SearchWindow::showSettingsPanel()
{
    if (m_isSettingsVisible) {
        return;
    }

    qDebug() << "Showing settings panel";

    m_isSettingsVisible = true;
    refreshSettingsList();

    // Обновляем состояние чекбокса
    if (m_useExplorerCheckBox) {
        m_useExplorerCheckBox->setChecked(m_useExplorer);
    }

    // Сохраняем оригинальные размеры и состояние
    m_originalWindowSize = size();
    m_originalSearchEditText = m_searchEdit->text();
    m_wereResultsVisible = m_resultsList->isVisible();

    // Полностью скрываем все элементы поиска
    m_searchLayout->parentWidget()->setVisible(false);
    m_resultsList->setVisible(false);
    m_statusLabel->setVisible(false);

    // Устанавливаем новый размер окна для настроек
    int settingsWidth = 500; // Немного увеличиваем ширину
    int settingsHeight = 420; // Увеличиваем высоту для полного отображения
    setFixedSize(settingsWidth, settingsHeight);

    // Центрируем окно на экране
    QScreen *screen = QGuiApplication::screenAt(pos());
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = screenGeometry.center().x() - settingsWidth / 2;
        int y = screenGeometry.center().y() - settingsHeight / 2;
        move(x, y);
    }

    // Настраиваем виджет настроек
    m_settingsWidget->setFixedSize(settingsWidth - 40, settingsHeight - 40); // С отступами
    m_settingsWidget->move(20, 20); // Отступ от краев
    m_settingsWidget->setVisible(true);
    m_settingsWidget->raise();

    // Анимация появления
    m_settingsWidget->setWindowOpacity(0.0);
    QPropertyAnimation *animation = new QPropertyAnimation(m_settingsWidget, "windowOpacity");
    animation->setDuration(150);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::InOutQuad);
    animation->start(QPropertyAnimation::DeleteWhenStopped);

    // Обновляем тень для нового размера
    if (m_shadowEffect) {
        m_shadowEffect->setBlurRadius(40);
        m_shadowEffect->setOffset(0, 8);
    }

    qDebug() << "Settings panel shown. Window size:" << size() << "Settings widget:" << m_settingsWidget->geometry();
}

void SearchWindow::hideSettingsPanel()
{
    if (!m_isSettingsVisible) {
        return;
    }

    qDebug() << "Hiding settings panel";

    m_isSettingsVisible = false;

    // Анимация исчезновения
    QPropertyAnimation *animation = new QPropertyAnimation(m_settingsWidget, "windowOpacity");
    animation->setDuration(150);
    animation->setStartValue(m_settingsWidget->windowOpacity());
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, [this]() {
        m_settingsWidget->hide();

        // Восстанавливаем отображение элементов поиска
        m_searchLayout->parentWidget()->setVisible(true);

        // Восстанавливаем оригинальный размер окна
        if (m_originalWindowSize.isValid()) {
            setFixedSize(m_originalWindowSize);
        } else {
            setFixedSize(600, 80);
        }

        // Центрируем окно на экране
        QScreen *screen = QGuiApplication::screenAt(pos());
        if (screen) {
            QRect screenGeometry = screen->geometry();
            QSize windowSize = size();
            int x = screenGeometry.center().x() - windowSize.width() / 2;
            int y = screenGeometry.center().y() - windowSize.height() / 2;
            move(x, y);
        }

        // Восстанавливаем текст поиска и обновляем результаты
        if (!m_originalSearchEditText.isEmpty()) {
            m_searchEdit->setText(m_originalSearchEditText);
            updateResults();
        } else {
            // Если текста не было, просто очищаем результаты
            clearResults();
            m_resultsList->setVisible(false);
            m_statusLabel->setVisible(false);
        }

        // Фокусируемся на поле поиска
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });

    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void SearchWindow::onSettingsClicked()
{
    qDebug() << "SearchWindow::onSettingsClicked() called";

    if (m_isSettingsVisible) {
        hideSettingsPanel();
    } else {
        showSettingsPanel();
    }
}

void SearchWindow::refreshSettingsList()
{
    m_pathsList->clear();

    for (const QString &path : m_customSearchPaths) {
        QListWidgetItem *item = new QListWidgetItem(path);
        item->setToolTip(path);
        m_pathsList->addItem(item);
    }

    bool hasItems = m_pathsList->count() > 0;
    m_removePathButton->setEnabled(hasItems);
    m_removeAllPathsButton->setEnabled(hasItems);
}

bool SearchWindow::isPathInList(const QString &path)
{
    QString normalizedPath = QDir::cleanPath(path);
    for (const QString &existingPath : m_customSearchPaths) {
        if (QDir::cleanPath(existingPath) == normalizedPath) {
            return true;
        }
    }
    return false;
}

void SearchWindow::onAddPathClicked()
{
    qDebug() << "SearchWindow::onAddPathClicked() called";

    // Используем диалог для ввода текста вместо выбора папки
    bool ok;
    QString path = QInputDialog::getText(this, "Добавить путь для поиска",
                                         "Введите путь к папке:",
                                         QLineEdit::Normal,
                                         QDir::homePath(),
                                         &ok);

    if (ok && !path.isEmpty()) {
        // Очищаем путь от кавычек, если они есть
        path = path.trimmed();
        if (path.startsWith('"') && path.endsWith('"')) {
            path = path.mid(1, path.length() - 2);
        }

        // Преобразуем путь в абсолютный, если он относительный
        QDir dir(path);
        if (dir.isRelative()) {
            dir = QDir::current().absoluteFilePath(path);
            path = dir.absolutePath();
        } else {
            path = dir.absolutePath();
        }

        qDebug() << "User entered path:" << path;

        if (!isPathInList(path)) {
            m_customSearchPaths.append(path);
            refreshSettingsList();
            saveCustomSearchPaths();
            refreshSearchPaths();
            qDebug() << "Path added to list:" << path;

            // Показываем сообщение об успешном добавлении
            QMessageBox::information(this, "Успех",
                QString("Путь добавлен в список поиска:\n%1").arg(path));
        } else {
            QMessageBox::warning(this, "Внимание", "Этот путь уже добавлен в поиск");
        }
    }
}

void SearchWindow::onRemovePathClicked()
{
    QListWidgetItem *selectedItem = m_pathsList->currentItem();
    if (!selectedItem) {
        QMessageBox::information(this, "Информация", "Выберите путь для удаления.");
        return;
    }

    QString path = selectedItem->text();

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
        QString("Удалить путь из списка?\n%1").arg(path),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_customSearchPaths.removeAll(path);
        refreshSettingsList();
        saveCustomSearchPaths();
        refreshSearchPaths();
        qDebug() << "Path removed:" << path;
    }
}

void SearchWindow::onRemoveAllPathsClicked()
{
    if (m_pathsList->count() == 0) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
        "Удалить все пути из списка?\nПоиск будет выполняться только в стандартных папках.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_customSearchPaths.clear();
        refreshSettingsList();
        saveCustomSearchPaths();
        refreshSearchPaths();
        qDebug() << "All paths removed";
    }
}

void SearchWindow::onCloseSettingsClicked()
{
    hideSettingsPanel();
}

void SearchWindow::showAtScreen(QScreen *screen)
{
    // Проверяем, включен ли поиск
    if (!isSearchEnabled()) {
        close();
        return;
    }

    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    calculatePosition(screen);
    show();
    raise();
    activateWindow();
    m_searchEdit->setFocus();

    // Плавное появление
    setWindowOpacity(0.0);
    QTimer::singleShot(10, [this]() {
        QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
        animation->setDuration(150);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::InOutQuad);
        animation->start(QPropertyAnimation::DeleteWhenStopped);
    });
}

void SearchWindow::activateSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void SearchWindow::calculatePosition(QScreen *screen)
{
    if (!screen) return;

    QRect screenGeometry = screen->geometry();
    QSize windowSize = size();

    int x = screenGeometry.center().x() - windowSize.width() / 2;
    int y = screenGeometry.center().y() - windowSize.height() / 2;

    move(x, y);
}

void SearchWindow::onSearchTextChanged(const QString &text)
{
    m_searchTimer->stop();

    if (text.trimmed().isEmpty()) {
        clearResults();
        m_resultsList->setVisible(false);
        m_statusLabel->setVisible(false);
        setFixedHeight(80);
        return;
    }

    m_searchTimer->start();
}

void SearchWindow::updateResults()
{
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) {
        return;
    }

    clearResults();

    // Обработка специальных команд терминала
    QString lowerQuery = query.toLower();
    if (lowerQuery == "\\cmd" || lowerQuery == "cmd") {
        SearchResult cmdResult;
        cmdResult.name = "Открыть командную строку";
        cmdResult.type = "terminal";
        cmdResult.description = "Запустить командную строку Windows (cmd.exe)";
        cmdResult.data = "cmd"; // Флаг для обычного терминала

        addSearchResult(cmdResult);
    } else if (lowerQuery == "\\acmd" || lowerQuery == "acmd") {
        SearchResult adminCmdResult;
        adminCmdResult.name = "Открыть командную строку от имени администратора";
        adminCmdResult.type = "terminal";
        adminCmdResult.description = "Запустить командную строку с правами администратора";
        adminCmdResult.data = "admin"; // Флаг для администраторского терминала

        addSearchResult(adminCmdResult);
    } else if (lowerQuery == "\\powershell" || lowerQuery == "powershell" ||
               lowerQuery == "\\ps" || lowerQuery == "ps") {
        SearchResult psResult;
        psResult.name = "Открыть PowerShell";
        psResult.type = "terminal";
        psResult.description = "Запустить PowerShell";
        psResult.data = "powershell";

        addSearchResult(psResult);
               } else if (lowerQuery == "\\apowershell" || lowerQuery == "apowershell" ||
                          lowerQuery == "\\aps" || lowerQuery == "aps") {
        SearchResult adminPsResult;
        adminPsResult.name = "Открыть PowerShell от имени администратора";
        adminPsResult.type = "terminal";
        adminPsResult.description = "Запустить PowerShell с правами администратора";
        adminPsResult.data = "admin_powershell";

        addSearchResult(adminPsResult);
    }

    // Остальной код остается без изменений
    else if (isMathExpression(query)) {
        try {
            QString result = calculateMathExpression(query);

            SearchResult calcResult;
            calcResult.name = "= " + result;
            calcResult.type = "calc";
            calcResult.description = "Результат вычисления";
            calcResult.data = result;

            addSearchResult(calcResult);

            SearchResult copyResult;
            copyResult.name = "📋 Копировать результат";
            copyResult.type = "copy";
            copyResult.description = "Скопировать результат в буфер обмена";
            copyResult.data = result;

            addSearchResult(copyResult);
        } catch (const std::exception &e) {
            qDebug() << "Calculation error:" << e.what();
        }
    }

    if (m_appSearcher) {
        QList<SearchResult> fileResults = m_appSearcher->searchAllFiles(query, 50);
        for (const SearchResult &file : fileResults) {
            addSearchResult(file);
        }
    }

    if (!query.isEmpty() && !lowerQuery.startsWith("\\")) {
        SearchResult webResult;
        webResult.name = "🌐 Поиск в интернете: \"" + query + "\"";
        webResult.type = "web";
        webResult.description = "Искать в DuckDuckGo";
        webResult.data = query;

        addSearchResult(webResult);
    }

    int count = m_resultsList->count();
    if (count > 0) {
        m_resultsList->setVisible(true);
        m_statusLabel->setText(QString("Найдено результатов: %1").arg(count));
        m_statusLabel->setVisible(true);
        m_resultsList->setCurrentRow(0);

        for (int i = 0; i < count; i++) {
            QListWidgetItem *item = m_resultsList->item(i);
            QWidget *widget = m_resultsList->itemWidget(item);
            if (widget) {
                widget->adjustSize();
                item->setSizeHint(widget->sizeHint());
            }
        }
    } else {
        m_resultsList->setVisible(false);
        m_statusLabel->setText("Ничего не найдено");
        m_statusLabel->setVisible(true);
    }

    int baseHeight = 80;
    int spacing = 10;
    int statusHeight = 30;
    int maxVisibleItems = 8;

    int additionalHeight = 0;
    if (m_resultsList->isVisible()) {
        int visibleItems = qMin(count, maxVisibleItems);
        int totalItemsHeight = 0;
        for (int i = 0; i < visibleItems; i++) {
            QListWidgetItem *item = m_resultsList->item(i);
            if (item) {
                totalItemsHeight += item->sizeHint().height();
            }
        }
        // Увеличиваем высоту для учета третьей строки
        additionalHeight += spacing + totalItemsHeight + (visibleItems * 2);
    }
    if (m_statusLabel->isVisible()) {
        additionalHeight += statusHeight;
    }

    int newHeight = baseHeight + additionalHeight;
    setFixedHeight(qMin(newHeight, 600));
}

void SearchWindow::addSearchResult(const SearchResult &result)
{
    QListWidgetItem *item = new QListWidgetItem(m_resultsList);
    item->setData(Qt::UserRole, result.type);
    item->setData(Qt::UserRole + 1, result.path);
    item->setData(Qt::UserRole + 2, result.data);

    QWidget *itemWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    QLabel *iconLabel = new QLabel();
    iconLabel->setFixedSize(28, 28);

    QString iconText = "📄";
    if (result.type == "app") {
        iconText = "📱";
    } else if (result.type == "calc") {
        iconText = "🧮";
    } else if (result.type == "copy") {
        iconText = "📋";
    } else if (result.type == "web") {
        iconText = "🌐";
    } else if (result.type == "folder") {
        iconText = "📁";
    } else if (result.type == "image") {
        iconText = "🖼";
    } else if (result.type == "video") {
        iconText = "🎬";
    } else if (result.type == "audio") {
        iconText = "🎵";
    } else if (result.type == "document") {
        iconText = "📝";
    } else if (result.type == "archive") {
        iconText = "📦";
    } else if (result.type == "config") {
        iconText = "⚙";
    } else if (result.type == "terminal") {
        iconText = "💻"; // Иконка для терминала
    }

    iconLabel->setText(iconText);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    infoLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *nameLabel = new QLabel(result.name);
    nameLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 14px;"
        "   background: transparent;"
        "   padding: 0;"
        "   margin: 0;"
        "}"
    );
    nameLabel->setWordWrap(true);
    nameLabel->setMaximumWidth(500);

    QLabel *descLabel = new QLabel(result.description);
    descLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(180, 180, 180, 200);"
        "   font-size: 11px;"
        "   background: transparent;"
        "   padding: 0;"
        "   margin: 0;"
        "}"
    );

    // ДОБАВИТЬ ЭТУ ЧАСТЬ - третья строка с путем
    QLabel *pathLabel = nullptr;
    if (!result.path.isEmpty() && result.type != "calc" && result.type != "copy" &&
        result.type != "web" && result.type != "terminal") {
        QString displayPath = result.path;
        // Сокращаем слишком длинные пути
        if (displayPath.length() > 80) {
            displayPath = "..." + displayPath.right(77);
        }

        pathLabel = new QLabel(displayPath);
        pathLabel->setStyleSheet(
            "QLabel {"
            "   color: rgba(150, 150, 150, 180);"
            "   font-size: 10px;"
            "   background: transparent;"
            "   padding: 0;"
            "   margin: 0;"
            "   font-style: italic;"
            "}"
        );
        pathLabel->setWordWrap(true);
        pathLabel->setMaximumWidth(500);
        pathLabel->setToolTip(result.path); // Полный путь в тултипе
    }

    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(descLabel);
    if (pathLabel) {
        infoLayout->addWidget(pathLabel);
    }
    infoLayout->addStretch();

    layout->addWidget(iconLabel);
    layout->addLayout(infoLayout);
    layout->addStretch();

    itemWidget->setLayout(layout);
    itemWidget->adjustSize();

    // Увеличиваем высоту элемента, если есть путь
    int baseHeight = itemWidget->sizeHint().height() + 4;
    int finalHeight = pathLabel ? qMax(baseHeight, 70) : qMax(baseHeight, 60);
    item->setSizeHint(QSize(m_resultsList->width() - 20, finalHeight));

    m_resultsList->addItem(item);
    m_resultsList->setItemWidget(item, itemWidget);
}

void SearchWindow::clearResults()
{
    m_resultsList->clear();
}

void SearchWindow::onResultItemClicked(QListWidgetItem *item)
{
    // Проверяем, не был ли это правый клик (обрабатывается через контекстное меню)
    if (QApplication::mouseButtons() & Qt::RightButton) {
        return;
    }

    if (!item) return;

    QString type = item->data(Qt::UserRole).toString();
    QString path = item->data(Qt::UserRole + 1).toString();
    QVariant data = item->data(Qt::UserRole + 2);

    if (type == "app") {
        if (!path.isEmpty()) {
            if (path.endsWith(".lnk", Qt::CaseInsensitive)) {
                HRESULT hres = CoInitialize(NULL);
                if (SUCCEEDED(hres)) {
                    IShellLink* pShellLink = NULL;
                    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&pShellLink);

                    if (SUCCEEDED(hres)) {
                        IPersistFile* pPersistFile = NULL;
                        hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);

                        if (SUCCEEDED(hres)) {
                            hres = pPersistFile->Load(path.toStdWString().c_str(), STGM_READ);

                            if (SUCCEEDED(hres)) {
                                wchar_t exePath[MAX_PATH];
                                hres = pShellLink->GetPath(exePath, MAX_PATH, NULL, SLGP_UNCPRIORITY);

                                if (SUCCEEDED(hres)) {
                                    path = QString::fromWCharArray(exePath);
                                }
                            }
                            pPersistFile->Release();
                        }
                        pShellLink->Release();
                    }
                    CoUninitialize();
                }
            }

            QProcess::startDetached(path);
            qDebug() << "Launched:" << path;
        }
    }
    // Обработка терминала
else if (type == "terminal") {
    QString terminalType = data.toString();
    bool success = false;

#ifdef Q_OS_WIN
    // Получаем пути к системным директориям
    wchar_t system32Path[MAX_PATH];
    wchar_t windowsPath[MAX_PATH];

    GetSystemDirectoryW(system32Path, MAX_PATH);  // C:\Windows\System32
    GetWindowsDirectoryW(windowsPath, MAX_PATH);  // C:\Windows

    // Пути к исполняемым файлам
    QString cmdExePath = QString::fromWCharArray(system32Path) + "\\cmd.exe";
    QString powerShellExePath = QString::fromWCharArray(system32Path) + "\\WindowsPowerShell\\v1.0\\powershell.exe";

    // Проверяем существование
    if (!QFile::exists(cmdExePath)) {
        cmdExePath = "cmd.exe";
    }

    if (!QFile::exists(powerShellExePath)) {
        powerShellExePath = "powershell.exe";
    }

    if (terminalType == "cmd") {
        // Запуск обычного cmd с рабочим каталогом System32
        HINSTANCE hInst = ShellExecuteW(
            NULL,
            L"open",
            cmdExePath.toStdWString().c_str(),
            L"/k",
            system32Path,  // Рабочий каталог = C:\Windows\System32
            SW_SHOWNORMAL
        );

        success = (reinterpret_cast<INT_PTR>(hInst) > 32);
        qDebug() << "Launched cmd.exe with working directory:" << QString::fromWCharArray(system32Path);
    }
    else if (terminalType == "admin") {
        // Запуск cmd от имени администратора с рабочим каталогом System32
        SHELLEXECUTEINFOW shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = NULL;
        shExInfo.lpVerb = L"runas";
        shExInfo.lpFile = cmdExePath.toStdWString().c_str();
        shExInfo.lpParameters = L"/k";
        shExInfo.lpDirectory = system32Path;  // Рабочий каталог = C:\Windows\System32
        shExInfo.nShow = SW_SHOWNORMAL;
        shExInfo.hInstApp = NULL;

        success = ShellExecuteExW(&shExInfo);
        qDebug() << "Launched cmd.exe as administrator with working directory:" << QString::fromWCharArray(system32Path);
    }
    else if (terminalType == "powershell") {
        // Запуск PowerShell с рабочим каталогом System32
        QString powerShellDir = QString::fromWCharArray(system32Path) + "\\WindowsPowerShell\\v1.0\\";

        HINSTANCE hInst = ShellExecuteW(
            NULL,
            L"open",
            powerShellExePath.toStdWString().c_str(),
            L"-NoExit",
            system32Path,  // Рабочий каталог = C:\Windows\System32
            SW_SHOWNORMAL
        );

        success = (reinterpret_cast<INT_PTR>(hInst) > 32);
        qDebug() << "Launched powershell.exe with working directory:" << QString::fromWCharArray(system32Path);
    }
    else if (terminalType == "admin_powershell") {
        // Запуск PowerShell от имени администратора с рабочим каталогом System32
        SHELLEXECUTEINFOW shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = NULL;
        shExInfo.lpVerb = L"runas";
        shExInfo.lpFile = powerShellExePath.toStdWString().c_str();
        shExInfo.lpParameters = L"-NoExit -Command \"Write-Host 'Running PowerShell as administrator...'\"";
        shExInfo.lpDirectory = system32Path;  // Рабочий каталог = C:\Windows\System32
        shExInfo.nShow = SW_SHOWNORMAL;
        shExInfo.hInstApp = NULL;

        success = ShellExecuteExW(&shExInfo);
        qDebug() << "Launched powershell.exe as administrator with working directory:" << QString::fromWCharArray(system32Path);
    }
#endif

    if (success) {
        m_statusLabel->setText("Терминал запущен");
    } else {
        m_statusLabel->setText("Не удалось запустить терминал");
    }

    QTimer::singleShot(1000, this, [this]() {
        if (!m_searchEdit->text().isEmpty()) {
            updateResults();
        }
    });
    return;
}
    else if (type == "calc" || type == "copy") {
        QString result = data.toString();
        QApplication::clipboard()->setText(result);
        qDebug() << "Copied to clipboard:" << result;

        m_statusLabel->setText("Результат скопирован в буфер обмена");
        QTimer::singleShot(1000, this, [this]() {
            if (!m_searchEdit->text().isEmpty()) {
                updateResults();
            }
        });
        return;
    } else if (type == "web") {
        QString query = data.toString();
        if (!query.isEmpty()) {
            QString url = "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(query);
            QDesktopServices::openUrl(QUrl(url));
        }
    } else if (type == "folder" || !path.isEmpty()) {
        // Проверяем, является ли путь папкой
        QFileInfo fileInfo(path);
        bool isDirectory = fileInfo.isDir();

        if (m_useExplorer) {
            // Используем стандартный способ через проводник
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        } else {
            // Если выбрано использование QFiles
            if (isDirectory) {
                // Для папок используем префикс QFile:
                QString windowsPath = QDir::toNativeSeparators(path);
                QString qfilePath = "QFile:" + windowsPath;

                qDebug() << "Executing QFiles command for folder:" << qfilePath;

#ifdef Q_OS_WIN
                // Используем ShellExecute для открытия папки через QFiles
                HINSTANCE hInst = ::ShellExecuteA(
                    0,
                    "open",
                    "cmd.exe",
                    QString("/c start \"\" \"%1\"").arg(qfilePath).toUtf8().constData(),
                    0,
                    SW_HIDE
                );

                if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
                    qDebug() << "ShellExecute failed with code:" << reinterpret_cast<INT_PTR>(hInst);

                    // Пробуем альтернативный способ
                    QStringList args;
                    args << "/c" << "start" << "\"\"" << qfilePath;
                    bool success = QProcess::startDetached("cmd", args);

                    if (!success) {
                        // Если не получилось, открываем как обычную папку
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                        qDebug() << "Falling back to standard folder opening:" << path;
                    }
                }
#endif
            } else {
                // Для файлов (не папок) открываем напрямую в ассоциированной программе
                qDebug() << "Opening file directly:" << path;
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        }
    }

    // Не закрываем окно сразу - даем время для запуска
    QTimer::singleShot(200, this, &SearchWindow::hideAndClose);
}

void SearchWindow::saveCustomSearchPaths()
{
    // Удаляем пустые пути перед сохранением
    m_customSearchPaths.removeAll("");
    m_customSearchPaths.removeAll(QString());

    QSettings settings("MyCompany", "DockApp");
    settings.setValue("SearchPaths/CustomPaths", m_customSearchPaths);
}

void SearchWindow::loadCustomSearchPaths()
{
    QSettings settings("MyCompany", "DockApp");
    m_customSearchPaths = settings.value("SearchPaths/CustomPaths").toStringList();
}

void SearchWindow::refreshSearchPaths()
{
    if (m_appSearcher) {
        m_appSearcher->setCustomSearchPaths(m_customSearchPaths);
        m_appSearcher->cacheAllFiles();
    }
}

bool SearchWindow::isMathExpression(const QString &text)
{
    QString trimmed = text.trimmed();
    QRegularExpression mathOpsRegex("[+\\-*/^%()]");
    QRegularExpression numbersRegex("[0-9]");
    QRegularExpression lettersRegex("[a-df-zA-DF-Z]");

    bool hasMathOps = trimmed.contains(mathOpsRegex);
    bool hasNumbers = trimmed.contains(numbersRegex);
    bool hasLetters = trimmed.contains(lettersRegex);

    QString lower = trimmed.toLower();
    bool hasFunctions = lower.contains("sin") || lower.contains("cos") ||
                        lower.contains("tan") || lower.contains("sqrt") ||
                        lower.contains("log") || lower.contains("ln") ||
                        lower.contains("exp") || lower.contains("pi") ||
                        lower.contains("e");

    return (hasMathOps && hasNumbers && !hasLetters) || hasFunctions;
}

QString SearchWindow::calculateMathExpression(const QString &expression)
{
    try {
        QString expr = expression.trimmed();

        if (expr.isEmpty()) {
            throw std::runtime_error("Empty expression");
        }

        expr.replace("pi", QString::number(M_PI), Qt::CaseInsensitive);
        expr.replace("e", QString::number(M_E), Qt::CaseInsensitive);
        expr.replace("sin", "s", Qt::CaseInsensitive);
        expr.replace("cos", "c", Qt::CaseInsensitive);
        expr.replace("tan", "t", Qt::CaseInsensitive);
        expr.replace("sqrt", "r", Qt::CaseInsensitive);
        expr.replace("log", "l", Qt::CaseInsensitive);
        expr.replace("ln", "n", Qt::CaseInsensitive);
        expr.replace("exp", "x", Qt::CaseInsensitive);
        expr.remove(" ");

        int balance = 0;
        for (QChar ch : expr) {
            if (ch == '(') balance++;
            else if (ch == ')') balance--;
            if (balance < 0) throw std::runtime_error("Несбалансированные скобки");
        }
        if (balance != 0) throw std::runtime_error("Несбалансированные скобки");

        std::stack<double> values;
        std::stack<char> ops;

        for (int i = 0; i < expr.length(); ++i) {
            QChar ch = expr[i];

            if (ch.isDigit() || ch == '.') {
                QString numStr;
                while (i < expr.length() && (expr[i].isDigit() || expr[i] == '.')) {
                    numStr += expr[i];
                    ++i;
                }
                --i;
                values.push(numStr.toDouble());
            } else if (ch == '(') {
                ops.push(ch.toLatin1());
            } else if (ch == ')') {
                while (!ops.empty() && ops.top() != '(') {
                    if (values.size() < 2) throw std::runtime_error("Invalid expression");
                    double val2 = values.top(); values.pop();
                    double val1 = values.top(); values.pop();
                    char op = ops.top(); ops.pop();
                    values.push(eval(val1, val2, op));
                }
                if (!ops.empty() && ops.top() == '(') {
                    ops.pop();
                } else {
                    throw std::runtime_error("Unbalanced parentheses");
                }
            } else if (isOperator(ch.toLatin1())) {
                while (!ops.empty() && ops.top() != '(' && precedence(ops.top()) >= precedence(ch.toLatin1())) {
                    if (values.size() < 2) throw std::runtime_error("Invalid expression");
                    double val2 = values.top(); values.pop();
                    double val1 = values.top(); values.pop();
                    char op = ops.top(); ops.pop();
                    values.push(eval(val1, val2, op));
                }
                ops.push(ch.toLatin1());
            } else if (ch == 's' || ch == 'c' || ch == 't' || ch == 'r' || ch == 'l' || ch == 'n' || ch == 'x') {
                if (i + 1 >= expr.length() || expr[i + 1] != '(') {
                    throw std::runtime_error("Function requires parentheses");
                }
                ++i;
                QString funcArg;
                int parenCount = 1;
                ++i;
                while (i < expr.length() && parenCount > 0) {
                    if (expr[i] == '(') ++parenCount;
                    else if (expr[i] == ')') --parenCount;
                    if (parenCount > 0) funcArg += expr[i];
                    ++i;
                }
                --i;
                if (parenCount != 0) throw std::runtime_error("Unbalanced parentheses in function");
                if (funcArg.isEmpty()) throw std::runtime_error("Missing argument for function");
                double arg = calculateMathExpression(funcArg).toDouble();
                double result = 0.0;
                switch (ch.toLatin1()) {
                    case 's': result = sin(arg * M_PI / 180.0); break;
                    case 'c': result = cos(arg * M_PI / 180.0); break;
                    case 't': result = tan(arg * M_PI / 180.0); break;
                    case 'r': result = sqrt(arg); break;
                    case 'l': result = log10(arg); break;
                    case 'n': result = log(arg); break;
                    case 'x': result = exp(arg); break;
                }
                values.push(result);
            } else {
                throw std::runtime_error(QString("Unknown character: %1").arg(ch).toStdString());
            }
        }

        while (!ops.empty()) {
            if (ops.top() == '(') throw std::runtime_error("Unbalanced parentheses");
            if (values.size() < 2) throw std::runtime_error("Invalid expression");
            double val2 = values.top(); values.pop();
            double val1 = values.top(); values.pop();
            char op = ops.top(); ops.pop();
            values.push(eval(val1, val2, op));
        }

        if (values.empty()) throw std::runtime_error("Empty expression");
        if (values.size() > 1) throw std::runtime_error("Invalid expression");
        double result = values.top();

        if (qAbs(result - std::round(result)) < 1e-10) {
            return QString::number(static_cast<long long>(std::round(result)));
        } else {
            return QString::number(result, 'g', 10);
        }
    } catch (const std::exception &e) {
        return QString("Ошибка: %1").arg(e.what());
    }
}

double SearchWindow::eval(double a, double b, char op)
{
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0) throw std::runtime_error("Ниче ты клоун 🤡 хонк-хонк");
            return a / b;
        case '^': return pow(a, b);
        case '%': return fmod(a, b);
    }
    throw std::runtime_error("Хрен знает что ты ввел");
}

int SearchWindow::precedence(char op)
{
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/' || op == '%') return 2;
    if (op == '^') return 3;
    return 0;
}

bool SearchWindow::isOperator(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%';
}

void SearchWindow::hideAndClose()
{
    if (m_isClosing) return;

    m_isClosing = true;

    // Скрываем панель настроек, если она открыта
    if (m_isSettingsVisible) {
        hideSettingsPanel();
    }

    QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
    animation->setDuration(150);
    animation->setStartValue(windowOpacity());
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, [this]() {
        hide();
        close();
        deleteLater();
    });

    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void SearchWindow::closeEvent(QCloseEvent *event)
{
    if (m_isClosing) {
        event->accept();
        return;
    }

    m_isClosing = true;

    // Скрываем панель настроек, если она открыта
    if (m_isSettingsVisible) {
        hideSettingsPanel();
    }

    event->accept();
}

void SearchWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::transparent);
}

void SearchWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_isSettingsVisible) {
            hideSettingsPanel();
        } else {
            hideAndClose();
        }
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_resultsList->count() > 0) {
            QListWidgetItem *currentItem = m_resultsList->currentItem();
            if (currentItem) {
                onResultItemClicked(currentItem);
            }
        } else {
            QString query = m_searchEdit->text().trimmed();
            if (!query.isEmpty()) {
                QString url = "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(query);
                QDesktopServices::openUrl(QUrl(url));
                hideAndClose();
            }
        }
        return;
    }

    if (event->key() == Qt::Key_Up) {
        if (m_resultsList->count() > 0) {
            int currentRow = m_resultsList->currentRow();
            if (currentRow > 0) {
                m_resultsList->setCurrentRow(currentRow - 1);
            }
        }
        return;
    }

    if (event->key() == Qt::Key_Down) {
        if (m_resultsList->count() > 0) {
            int currentRow = m_resultsList->currentRow();
            if (currentRow < m_resultsList->count() - 1) {
                m_resultsList->setCurrentRow(currentRow + 1);
            } else if (currentRow == -1) {
                m_resultsList->setCurrentRow(0);
            }
        }
        return;
    }

    if (event->key() == Qt::Key_Tab) {
        if (m_resultsList->count() > 0) {
            int currentRow = m_resultsList->currentRow();
            if (currentRow < m_resultsList->count() - 1) {
                m_resultsList->setCurrentRow(currentRow + 1);
            } else {
                m_resultsList->setCurrentRow(0);
            }
        }
        return;
    }

    QWidget::keyPressEvent(event);
}

void SearchWindow::mousePressEvent(QMouseEvent *event)
{
    // Игнорируем правую кнопку мыши для закрытия окна
    if (event->button() == Qt::RightButton) {
        event->accept();
        return;
    }

    QPoint localPos = event->pos();

    if (m_isSettingsVisible) {
        // При открытых настройках проверяем клик внутри виджета настроек
        bool clickInSettingsWidget = m_settingsWidget->geometry().contains(localPos);

        // Если клик не в виджете настроек, игнорируем (не закрываем окно)
        if (!clickInSettingsWidget) {
            event->accept();
            return;
        }
    } else {
        // При закрытых настройках обычная логика
        bool clickInMainWidget = m_mainWidget->geometry().contains(localPos);

        if (!clickInMainWidget) {
            hideAndClose();
        } else {
            QWidget::mousePressEvent(event);
        }
    }
}

void SearchWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_shadowEffect) {
        m_shadowEffect->setBlurRadius(40);
        m_shadowEffect->setOffset(0, 8);
    }

    // При изменении размера окна, если открыты настройки, обновляем размер виджета настроек
    if (m_isSettingsVisible && m_settingsWidget) {
        m_settingsWidget->setFixedSize(width() - 40, height() - 40);
        m_settingsWidget->move(20, 20);
    }

    if (m_resultsList->count() > 0) {
        for (int i = 0; i < m_resultsList->count(); i++) {
            QListWidgetItem *item = m_resultsList->item(i);
            if (item) {
                QSize size = item->sizeHint();
                size.setWidth(m_resultsList->viewport()->width() - 20);
                item->setSizeHint(size);
            }
        }
    }
}

bool SearchWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_isClosing) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPosition().toPoint();
        QPoint localPos = mapFromGlobal(globalPos);

        if (m_isSettingsVisible) {
            // При открытых настройках
            bool clickInSettingsWidget = m_settingsWidget->geometry().contains(localPos);
            bool clickInMainWindow = this->geometry().contains(mapFromGlobal(globalPos));

            // Если клик внутри окна настроек, обрабатываем как обычно
            if (clickInSettingsWidget) {
                return QWidget::eventFilter(obj, event);
            }

            // Если клик в основном окне, но не в виджете настроек, игнорируем
            if (clickInMainWindow && !clickInSettingsWidget) {
                return true; // Игнорируем клик
            }

            // Если клик вне основного окна, закрываем настройки
            if (!clickInMainWindow) {
                hideSettingsPanel();
                return true;
            }
        } else {
            // При закрытых настройках оригинальная логика
            bool clickInMainWidget = m_mainWidget->geometry().contains(localPos);

            if (clickInMainWidget) {
                QWidget *clickedWidget = QApplication::widgetAt(globalPos);
                if (clickedWidget == m_settingsButton) {
                    return QWidget::eventFilter(obj, event);
                }
                if (m_isSettingsVisible) {
                    hideSettingsPanel();
                    return true;
                }
            }

            if (!clickInMainWidget) {
                hideAndClose();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SearchWindow::showContextMenuForItem(const QPoint &pos)
{
    QListWidgetItem *item = m_resultsList->itemAt(pos);
    if (!item) return;

    QString type = item->data(Qt::UserRole).toString();
    QString path = item->data(Qt::UserRole + 1).toString();

    // Проверяем, является ли элемент файлом (не папкой и не специальным типом)
    QFileInfo fileInfo(path);
    bool isDirectory = fileInfo.isDir();

    // Показываем контекстное меню только для файлов (не папок)
    if (!path.isEmpty() && !isDirectory &&
        type != "calc" && type != "copy" && type != "web" && type != "terminal") {
        m_contextMenuPath = path;
        m_contextMenu->exec(m_resultsList->mapToGlobal(pos));
    }
}

void SearchWindow::onOpenFileLocation()
{
    if (m_contextMenuPath.isEmpty()) return;

    QFileInfo fileInfo(m_contextMenuPath);
    QString folderPath = fileInfo.absolutePath();

    if (folderPath.isEmpty()) return;

    qDebug() << "Opening file location:" << folderPath;

    if (m_useExplorer) {
        // Используем стандартный проводник
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
    } else {
        // Используем QFiles для папки
        QString windowsPath = QDir::toNativeSeparators(folderPath);
        QString qfilePath = "QFile:" + windowsPath;

#ifdef Q_OS_WIN
        HINSTANCE hInst = ::ShellExecuteA(
            0,
            "open",
            "cmd.exe",
            QString("/c start \"\" \"%1\"").arg(qfilePath).toUtf8().constData(),
            0,
            SW_HIDE
        );

        if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
            QStringList args;
            args << "/c" << "start" << "\"\"" << qfilePath;
            bool success = QProcess::startDetached("cmd", args);

            if (!success) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
            }
        }
#else
        QStringList args;
        args << "/c" << "start" << "\"\"" << qfilePath;
        QProcess::startDetached("cmd", args);
#endif
    }

    // Закрываем окно поиска
    QTimer::singleShot(200, this, &SearchWindow::hideAndClose);
}