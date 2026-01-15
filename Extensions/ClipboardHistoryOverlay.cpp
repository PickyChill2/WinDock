#include "ClipboardHistoryOverlay.h"
#include "..//EmojiList.h"
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QPainter>
#include <QDebug>
#include <QFileInfo>
#include <QScrollBar>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QMimeData>
#include <QTabWidget>
#include <QIcon>
#include <QPixmap>
#include <QLineEdit>
#include <QFontDatabase>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ClipboardHistoryOverlay::ClipboardHistoryOverlay(QWidget* parent)
    : QWidget(parent), m_visible(false), m_ignoreNextClipboardChange(false), 
      m_positionInitialized(false), m_closeButtonHovered(false)
#ifdef Q_OS_WIN
    , m_previousWindow(nullptr)
#endif
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::StrongFocus);

    // Устанавливаем начальный размер для вкладки History
    setFixedSize(TAB_WIDTH, HISTORY_INITIAL_HEIGHT);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "   border: 2px solid rgba(80, 80, 80, 200);"
        "   border-radius: 10px;"
        "   background-color: rgba(45, 45, 45, 250);"
        "}"
        "QTabBar::tab {"
        "   background-color: rgba(60, 60, 60, 180);"
        "   color: white;"
        "   padding: 8px 16px;"
        "   margin: 2px;"
        "   border-radius: 5px;"
        "}"
        "QTabBar::tab:selected {"
        "   background-color: rgba(0, 120, 215, 200);"
        "}"
        "QTabBar::tab:hover {"
        "   background-color: rgba(80, 80, 80, 200);"
        "}"
    );

    // History list widget
    m_historyList = new QListWidget(this);
    m_historyList->setStyleSheet(
        "QListWidget {"
        "   background-color: rgba(45, 45, 45, 250);"
        "   border: 2px solid rgba(80, 80, 80, 200);"
        "   border-radius: 10px;"
        "   padding: 5px;"
        "   color: white;"
        "   font-size: 14px;"
        "   outline: none;"
        "}"
        "QListWidget::item {"
        "   background-color: rgba(60, 60, 60, 180);"
        "   border-radius: 5px;"
        "   margin: 2px;"
        "   padding: 10px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: rgba(0, 120, 215, 200);"
        "}"
        "QListWidget::item:hover {"
        "   background-color: rgba(80, 80, 80, 200);"
        "}"
        "QScrollBar:vertical {"
        "   background: rgba(50, 50, 50, 150);"
        "   width: 12px;"
        "   margin: 0px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(100, 100, 100, 200);"
        "   border-radius: 6px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: rgba(120, 120, 120, 200);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   border: none;"
        "   background: none;"
        "}"
    );

    // Убираем горизонтальный скроллбар
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyList->setSelectionMode(QListWidget::SingleSelection);
    m_historyList->setIconSize(QSize(64, 64));

    connect(m_historyList, &QListWidget::itemClicked, this, &ClipboardHistoryOverlay::onItemClicked);

    // Emoji search edit
    m_emojiSearchEdit = new QLineEdit(this);
    m_emojiSearchEdit->setPlaceholderText("Поиск эмодзи...");
    m_emojiSearchEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: rgba(60, 60, 60, 180);"
        "   border: 2px solid rgba(80, 80, 80, 200);"
        "   border-radius: 10px;"
        "   padding: 8px;"
        "   color: white;"
        "   font-size: 14px;"
        "   margin-bottom: 5px;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid rgba(0, 120, 215, 200);"
        "}"
    );

    // Emoji list widget
    m_emojiList = new QListWidget(this);
    m_emojiList->setStyleSheet(
        "QListWidget {"
        "   background-color: rgba(45, 45, 45, 250);"
        "   border: 2px solid rgba(80, 80, 80, 200);"
        "   border-radius: 10px;"
        "   padding: 5px;"
        "   color: white;"
        "   font-size: 20px;"
        "   outline: none;"
        "}"
        "QListWidget::item {"
        "   background-color: rgba(60, 60, 60, 180);"
        "   border-radius: 8px;"
        "   margin: 2px;"
        "   padding: 0px;"
        "   text-align: center;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: rgba(0, 120, 215, 200);"
        "   border: 2px solid rgba(100, 180, 255, 255);"
        "}"
        "QListWidget::item:hover {"
        "   background-color: rgba(80, 80, 80, 200);"
        "}"
        "QScrollBar:vertical {"
        "   background: rgba(50, 50, 50, 150);"
        "   width: 12px;"
        "   margin: 0px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(100, 100, 100, 200);"
        "   border-radius: 6px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: rgba(120, 120, 120, 200);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   border: none;"
        "   background: none;"
        "}"
    );

    m_emojiList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_emojiList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_emojiList->setSelectionMode(QListWidget::SingleSelection);
    m_emojiList->setViewMode(QListWidget::IconMode);
    m_emojiList->setGridSize(QSize(50, 50));
    m_emojiList->setMovement(QListWidget::Static);

    connect(m_emojiList, &QListWidget::itemClicked, this, &ClipboardHistoryOverlay::onEmojiItemClicked);

    // Создаем layout для вкладки эмодзи
    QWidget* emojiTabWidget = new QWidget();
    QVBoxLayout* emojiLayout = new QVBoxLayout(emojiTabWidget);
    emojiLayout->setContentsMargins(0, 0, 0, 0);
    emojiLayout->setSpacing(5);
    emojiLayout->addWidget(m_emojiSearchEdit);
    emojiLayout->addWidget(m_emojiList);

    // Add tabs
    m_tabWidget->addTab(m_historyList, "History");
    m_tabWidget->addTab(emojiTabWidget, "Emoji");

    // Подключаем сигнал изменения вкладки
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ClipboardHistoryOverlay::onTabChanged);
    // Подключаем сигнал поиска
    connect(m_emojiSearchEdit, &QLineEdit::textChanged, this, &ClipboardHistoryOverlay::filterEmojis);

    mainLayout->addWidget(m_tabWidget);

    // Animation setup
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(200);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);

    // Подключаемся к сигналу изменения буфера обмена
    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardHistoryOverlay::onClipboardDataChanged);

    // Инициализируем историю текущим содержимым буфера
    QString initialText = m_clipboard->text();
    if (!initialText.isEmpty()) {
        m_clipboardHistory.append(initialText);
    }

    // Заполняем список эмодзи
    refreshEmojiList();

    hide();
}

ClipboardHistoryOverlay::~ClipboardHistoryOverlay()
{
    if (m_fadeAnimation) {
        m_fadeAnimation->stop();
        delete m_fadeAnimation;
    }
    if (m_opacityEffect) {
        delete m_opacityEffect;
    }

    // Убираем фильтр событий при уничтожении
    qApp->removeEventFilter(this);
}

#ifdef Q_OS_WIN
void ClipboardHistoryOverlay::pasteToActiveWindow()
{
    // Сохраняем указатель на this, так как окно может быть закрыто
    ClipboardHistoryOverlay* self = this;
    
    // Закрываем окно
    if (m_visible) {
        toggleVisibility();
    }
    
    // Ждем закрытия окна
    QTimer::singleShot(200, [self]() {
        // Восстанавливаем активное окно
        if (self->m_previousWindow && IsWindow(self->m_previousWindow)) {
            // Активируем предыдущее окно
            SetForegroundWindow(self->m_previousWindow);
            SetActiveWindow(self->m_previousWindow);
            SetFocus(self->m_previousWindow);
            
            // Даем окну время на активацию
            Sleep(100);
            
            // Эмулируем нажатие Ctrl+V
            keybd_event(VK_CONTROL, 0, 0, 0); // Ctrl down
            keybd_event('V', 0, 0, 0);        // V down
            keybd_event('V', 0, KEYEVENTF_KEYUP, 0); // V up
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0); // Ctrl up
        } else {
            // Если предыдущее окно не найдено, используем текущее активное окно
            HWND currentWindow = GetForegroundWindow();
            if (currentWindow) {
                // Даем окну время на активацию
                Sleep(100);
                
                // Эмулируем нажатие Ctrl+V
                keybd_event(VK_CONTROL, 0, 0, 0);
                keybd_event('V', 0, 0, 0);
                keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
                keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            }
        }
    });
}
#endif

void ClipboardHistoryOverlay::showEvent(QShowEvent* event)
{
    Q_UNUSED(event)
    // Устанавливаем фильтр событий для отслеживания кликов вне окна
    qApp->installEventFilter(this);

    // ОБНОВЛЯЕМ ТОЛЬКО ИСТОРИЮ, так как это первая вкладка
    refreshList();
}

void ClipboardHistoryOverlay::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event)
    // Убираем фильтр событий при скрытии окна
    qApp->removeEventFilter(this);
}

void ClipboardHistoryOverlay::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event)

    // Получаем виджет, который теперь имеет фокус
    QWidget* focusedWidget = QApplication::focusWidget();

    // Если фокус перешел на один из наших дочерних виджетов, не закрываем окно
    if (focusedWidget &&
        (focusedWidget == this ||
         focusedWidget == m_tabWidget ||
         focusedWidget == m_historyList ||
         focusedWidget == m_emojiList ||
         focusedWidget == m_emojiSearchEdit ||
         isAncestorOf(focusedWidget))) {
        return;
         }

    // Закрываем окно только при потере фокуса на внешние элементы
    if (m_visible) {
        toggleVisibility();
    }
}

bool ClipboardHistoryOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        // Проверяем, был ли клик вне этого окна
        if (!geometry().contains(mouseEvent->globalPosition().toPoint())) {
            toggleVisibility();
            return true;
        }
    }

    // Обрабатываем движения мыши для кнопки закрытия
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
        bool wasHovered = m_closeButtonHovered;
        m_closeButtonHovered = isInCloseButton(localPos);

        if (wasHovered != m_closeButtonHovered) {
            update(); // Перерисовываем если состояние изменилось
        }
    }

    // Обрабатываем клики по кнопке закрытия
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());

        if (isInCloseButton(localPos)) {
            toggleVisibility();
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ClipboardHistoryOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Рисуем полупрозрачный фон с закругленными углами
    QColor backgroundColor(30, 30, 30, 240);
    painter.setBrush(backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);

    // Рисуем кнопку закрытия
    updateCloseButtonRect();

    // Фон кнопки закрытия
    QColor closeButtonBg = m_closeButtonHovered ?
                          QColor(255, 80, 80, 200) :
                          QColor(100, 100, 100, 150);
    painter.setBrush(closeButtonBg);
    painter.drawEllipse(m_closeButtonRect);

    // Крестик
    painter.setPen(QPen(Qt::white, 2));
    int offset = 5;
    painter.drawLine(m_closeButtonRect.center() + QPoint(-offset, -offset),
                    m_closeButtonRect.center() + QPoint(offset, offset));
    painter.drawLine(m_closeButtonRect.center() + QPoint(-offset, offset),
                    m_closeButtonRect.center() + QPoint(offset, -offset));
}

void ClipboardHistoryOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        toggleVisibility();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        // Передаем навигационные клавиши текущему списку
        QWidget* currentWidget = m_tabWidget->currentWidget();
        if (currentWidget) {
            QApplication::sendEvent(currentWidget, event);
        }
    } else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        // Если фокус в поле поиска - не обрабатываем Enter
        if (m_emojiSearchEdit->hasFocus()) {
            event->ignore();
            return;
        }

        // Enter для выбора элемента
        QWidget* currentWidget = m_tabWidget->currentWidget();
        if (currentWidget == m_historyList) {
            QListWidgetItem* currentItem = m_historyList->currentItem();
            if (currentItem) {
                onItemClicked(currentItem);
            }
        } else if (m_tabWidget->currentIndex() == 1) { // Вкладка Emoji
            QListWidgetItem* currentItem = m_emojiList->currentItem();
            if (currentItem) {
                onEmojiItemClicked(currentItem);
            }
        }
    } else if (event->key() == Qt::Key_Tab) {
        // Переключение между вкладками
        int currentIndex = m_tabWidget->currentIndex();
        int nextIndex = (currentIndex + 1) % m_tabWidget->count();
        m_tabWidget->setCurrentIndex(nextIndex);
    }
    QWidget::keyPressEvent(event);
}

void ClipboardHistoryOverlay::mouseMoveEvent(QMouseEvent* event)
{
    // Обработка hover эффекта для кнопки закрытия
    bool wasHovered = m_closeButtonHovered;
    m_closeButtonHovered = isInCloseButton(event->pos());

    if (wasHovered != m_closeButtonHovered) {
        update(); // Перерисовываем если состояние изменилось
    }

    QWidget::mouseMoveEvent(event);
}

void ClipboardHistoryOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isInCloseButton(event->pos())) {
        toggleVisibility();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ClipboardHistoryOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCloseButtonRect();
}


void ClipboardHistoryOverlay::updatePosition()
{
    // Получаем позицию курсора
    QPoint cursorPos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenGeometry = screen->geometry();

    // Вычисляем позицию окна - под курсором, смещенное немного вверх и влево
    // УЧИТЫВАЕМ ТЕКУЩУЮ ВЫСОТУ ОКНА, которая соответствует активной вкладке
    int x = cursorPos.x() - width() / 2;
    int y = cursorPos.y() - height() - 10;

    // Проверяем, чтобы окно не выходило за границы экрана
    if (x < screenGeometry.left()) {
        x = screenGeometry.left();
    } else if (x + width() > screenGeometry.right()) {
        x = screenGeometry.right() - width();
    }

    if (y < screenGeometry.top()) {
        y = cursorPos.y() + 20; // Показываем под курсором, если сверху не помещается
    } else if (y + height() > screenGeometry.bottom()) {
        y = screenGeometry.bottom() - height();
    }

    move(x, y);
    m_initialPosition = pos(); // Сохраняем начальную позицию
    m_positionInitialized = true;
}

void ClipboardHistoryOverlay::adjustPositionToScreen()
{
    // Получаем текущий экран
    QScreen *screen = QGuiApplication::screenAt(m_initialPosition);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenGeometry = screen->geometry();

    int x = m_initialPosition.x();
    int y = m_initialPosition.y();

    // Проверяем, чтобы окно не выходило за границы экрана после изменения размера
    // УЧИТЫВАЕМ ТЕКУЩУЮ ВЫСОТУ ОКНА
    if (x < screenGeometry.left()) {
        x = screenGeometry.left();
    } else if (x + width() > screenGeometry.right()) {
        x = screenGeometry.right() - width();
    }

    if (y < screenGeometry.top()) {
        y = screenGeometry.top();
    } else if (y + height() > screenGeometry.bottom()) {
        y = screenGeometry.bottom() - height();
    }

    move(x, y);
}

void ClipboardHistoryOverlay::toggleVisibility()
{
    if (m_visible) {
        m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
        m_fadeAnimation->start();
        QTimer::singleShot(200, this, &QWidget::hide);
        m_visible = false;
        m_positionInitialized = false; // Сбрасываем флаг при скрытии
    } else {
        // Сохраняем предыдущее активное окно перед показом
#ifdef Q_OS_WIN
        m_previousWindow = GetForegroundWindow();
#endif
        
        // ВСЕГДА устанавливаем первую вкладку (History) при открытии
        m_tabWidget->setCurrentIndex(0);

        // Обновляем список истории перед показом
        refreshList();

        // Обновляем позицию только если это первое открытие ИЛИ если позиция не инициализирована
        if (!m_positionInitialized) {
            updatePosition(); // Обновляем позицию перед показом
        }

        show();
        raise();
        activateWindow();

        // Устанавливаем фокус на окно, но не сразу (даем время на отображение)
        QTimer::singleShot(50, this, [this]() {
            setFocus(Qt::PopupFocusReason);

            // Устанавливаем фокус на историю (первая вкладка)
            m_historyList->setFocus();
            if (m_historyList->count() > 0) {
                m_historyList->setCurrentRow(0);
            }
        });

        m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
        m_fadeAnimation->start();

#ifdef Q_OS_WIN
        // Bring to top
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
        m_visible = true;
    }
}

void ClipboardHistoryOverlay::onClipboardDataChanged()
{
    // Игнорируем изменения буфера, которые мы сами вызвали
    if (m_ignoreNextClipboardChange) {
        m_ignoreNextClipboardChange = false;
        return;
    }

    // Обновляем историю при каждом изменении буфера обмена
    updateClipboardHistory();
}

void ClipboardHistoryOverlay::updateClipboardHistory()
{
    // Проверяем наличие изображения в буфере обмена
    const QMimeData* mimeData = m_clipboard->mimeData();
    if (mimeData && mimeData->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            addImageToHistory(image);
            return;
        }
    }

    // Проверяем наличие URL (файлов изображений)
    if (mimeData && mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                // Проверяем, является ли файл изображением
                if (isImageFile(filePath)) {
                    QImage image(filePath);
                    if (!image.isNull()) {
                        addImageToHistory(image);
                        return; // Добавляем только первое изображение
                    }
                }
            }
        }
    }

    // Добавляем текстовое содержимое буфера обмена
    QString currentText = m_clipboard->text();
    if (!currentText.isEmpty()) {
        addToHistory(currentText);
    }
}

bool ClipboardHistoryOverlay::isImageFile(const QString& filePath)
{
    QString extension = QFileInfo(filePath).suffix().toLower();
    static const QStringList imageExtensions = {
        "png", "jpg", "jpeg", "bmp", "gif", "tiff", "tif",
        "webp", "ico", "svg", "psd", "raw", "heic", "heif"
    };
    return imageExtensions.contains(extension);
}

void ClipboardHistoryOverlay::addToHistory(const QString& text)
{
    // Проверяем, нет ли уже этого текста в истории
    int existingIndex = -1;
    for (int i = 0; i < m_clipboardHistory.size(); ++i) {
        if (m_clipboardHistory[i] == text) {
            existingIndex = i;
            break;
        }
    }

    // Если текст уже есть, перемещаем его в начало
    if (existingIndex != -1) {
        m_clipboardHistory.move(existingIndex, 0);
    } else {
        // Добавляем новый текст в начало
        m_clipboardHistory.prepend(text);

        // Оставляем только последние 15 элементов
        if (m_clipboardHistory.size() > 15) {
            m_clipboardHistory.removeLast();
        }
    }

    // Обновляем отображение, если окно видимо
    if (m_visible) {
        refreshList();
    }
}

void ClipboardHistoryOverlay::addImageToHistory(const QImage& image)
{
    // Добавляем изображение в историю
    m_imageHistory.prepend(image);

    // Оставляем только последние 15 элементов
    if (m_imageHistory.size() > 15) {
        m_imageHistory.removeLast();
    }

    // Обновляем отображение, если окно видимо
    if (m_visible) {
        refreshList();
    }
}

void ClipboardHistoryOverlay::refreshList()
{
    m_historyList->clear();

    // Добавляем изображения в список
    for (const QImage& image : m_imageHistory) {
        QPixmap pixmap = QPixmap::fromImage(image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QListWidgetItem* item = new QListWidgetItem(QIcon(pixmap), "Image");
        item->setData(Qt::UserRole, QVariant::fromValue(image));
        item->setToolTip("Image");
        m_historyList->addItem(item);
    }

    // Добавляем текстовые элементы в список
    for (const QString& item : m_clipboardHistory) {
        QString displayText = item;

        // Проверяем, является ли текст путем к файлу изображения
        if (item.startsWith("file:///") && isImageFile(item)) {
            QString filePath = QUrl(item).toLocalFile();
            QImage image(filePath);
            if (!image.isNull()) {
                QPixmap pixmap = QPixmap::fromImage(image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                QListWidgetItem* listItem = new QListWidgetItem(QIcon(pixmap), "Image");
                listItem->setData(Qt::UserRole, QVariant::fromValue(image));
                listItem->setToolTip(filePath);
                m_historyList->addItem(listItem);
                continue; // Пропускаем добавление как текста
            }
        }

        if (displayText.length() > 80) {
            displayText = displayText.left(80) + "...";
        }

        QListWidgetItem* listItem = new QListWidgetItem(displayText);
        listItem->setData(Qt::UserRole, item); // Сохраняем полный текст
        listItem->setToolTip(item);
        m_historyList->addItem(listItem);
    }

    // Автоматически подбираем высоту окна под содержимое (только для вкладки History)
    if (m_historyList->count() > 0) {
        int contentHeight = 0;
        for (int i = 0; i < qMin(8, m_historyList->count()); ++i) {
            contentHeight += m_historyList->sizeHintForRow(i);
        }
        contentHeight += 80; // Отступы, рамки и заголовок вкладки

        // Ограничиваем максимальную высоту
        int newHeight = qMin(HISTORY_MAX_HEIGHT, contentHeight);
        setFixedHeight(newHeight);

        // Корректируем позицию после изменения размера
        if (m_visible && m_positionInitialized) {
            adjustPositionToScreen();
        }
    } else {
        // Если нет элементов, устанавливаем минимальную высоту
        setFixedHeight(HISTORY_INITIAL_HEIGHT);

        // Корректируем позицию после изменения размера
        if (m_visible && m_positionInitialized) {
            adjustPositionToScreen();
        }
    }
}

void ClipboardHistoryOverlay::refreshEmojiList()
{
    m_emojiList->clear();

    // Устанавливаем шрифт, который поддерживает эмодзи
    QFont emojiFont = m_emojiList->font();
    emojiFont.setPointSize(16);

    // Пробуем разные шрифты, поддерживающие эмодзи
    QStringList emojiFonts = {
        "Segoe UI Emoji",
        "Segoe UI Symbol",
        "Apple Color Emoji",
        "Noto Color Emoji",
        "Android Emoji",
        "Twemoji Mozilla",
        "EmojiOne",
        "Arial"
    };

    // Проверяем доступные шрифты
    QFontDatabase fontDatabase;
    for (const QString& fontName : emojiFonts) {
        if (fontDatabase.hasFamily(fontName)) {
            emojiFont.setFamily(fontName);
            qDebug() << "Using emoji font:" << fontName;
            break;
        }
    }

    // Если не нашли специальный шрифт, используем системный
    if (emojiFont.family() == m_emojiList->font().family()) {
        qDebug() << "No emoji font found, using system font";
    }

    m_emojiList->setFont(emojiFont);

    // Добавляем популярные эмодзи
    const QList<EmojiData>& emojis = getEmojiList();

    for (const EmojiData& emojiData : emojis) {
        QListWidgetItem* item = new QListWidgetItem(emojiData.emoji);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        // Устанавливаем размер элемента
        item->setSizeHint(QSize(60, 60));

        m_emojiList->addItem(item);
    }

    // Устанавливаем режим отображения и размеры элементов
    m_emojiList->setViewMode(QListWidget::IconMode);
    m_emojiList->setGridSize(QSize(65, 65)); // Увеличиваем размер сетки
    m_emojiList->setIconSize(QSize(45, 45)); // Увеличиваем размер иконки
    m_emojiList->setSpacing(8); // Увеличиваем расстояние между элементами

    // Отключаем обрезку текста
    m_emojiList->setWordWrap(true);
    m_emojiList->setTextElideMode(Qt::ElideNone);

    // Устанавливаем политику размера
    m_emojiList->setResizeMode(QListWidget::Adjust);

    qDebug() << "Emoji list refreshed, count:" << m_emojiList->count();
    if (m_emojiList->count() > 0) {
        QListWidgetItem* firstItem = m_emojiList->item(0);
        if (firstItem) {
            qDebug() << "First emoji text:" << firstItem->text();
            qDebug() << "First emoji font:" << m_emojiList->font().toString();
        }
    }
}

void ClipboardHistoryOverlay::filterEmojis(const QString& searchText)
{
    m_emojiList->clear();

    QString searchLower = searchText.toLower();
    const QList<EmojiData>& allEmojis = getEmojiList();

    for (const EmojiData& emojiData : allEmojis) {
        bool matches = false;

        // Поиск по ключевым словам
        for (const QString& keyword : emojiData.keywords) {
            if (keyword.contains(searchLower)) {
                matches = true;
                break;
            }
        }

        // Также ищем по самому эмодзи (если пользователь ввел эмодзи)
        if (!matches && emojiData.emoji.contains(searchText)) {
            matches = true;
        }

        if (searchText.isEmpty() || matches) {
            QListWidgetItem* item = new QListWidgetItem(emojiData.emoji);
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_emojiList->addItem(item);
        }
    }
}

void ClipboardHistoryOverlay::onTabChanged(int index)
{
    if (index == 0) { // Вкладка History
        // Для истории используем фиксированную ширину и адаптивную высоту
        setFixedWidth(TAB_WIDTH);
        refreshList(); // Обновляем список, который установит высоту

        // Устанавливаем фокус на историю
        m_historyList->setFocus();
        if (m_historyList->count() > 0) {
            m_historyList->setCurrentRow(0);
        }
    } else if (index == 1) { // Вкладка Emoji
        // Для эмодзи фиксированный размер с той же шириной, но увеличенной высотой для поля поиска
        setFixedSize(TAB_WIDTH, EMOJI_TAB_HEIGHT + 40);

        // Устанавливаем фокус на поле поиска при переключении на вкладку
        m_emojiSearchEdit->setFocus();
        m_emojiSearchEdit->selectAll(); // Выделяем весь текст для удобства

        // Убедимся, что есть выделенный элемент в списке эмодзи
        if (m_emojiList->count() > 0) {
            m_emojiList->setCurrentRow(0);
        }
    }

    // Корректируем позицию после изменения размера, если позиция уже инициализирована
    if (m_visible && m_positionInitialized) {
        adjustPositionToScreen();
    }
}

void ClipboardHistoryOverlay::onItemClicked(QListWidgetItem* item)
{
    if (item) {
        // Проверяем, является ли элемент изображением
        if (item->data(Qt::UserRole).canConvert<QImage>()) {
            QImage image = item->data(Qt::UserRole).value<QImage>();
            m_ignoreNextClipboardChange = true;

            // Создаем QMimeData для передачи изображения
            QMimeData* mimeData = new QMimeData;
            mimeData->setImageData(image);

            // Устанавливаем в буфер обмена
            m_clipboard->setMimeData(mimeData);
        } else {
            QString selectedText = item->data(Qt::UserRole).toString();
            m_ignoreNextClipboardChange = true;
            m_clipboard->setText(selectedText);
        }

        // Вставляем в активное окно (только на Windows)
#ifdef Q_OS_WIN
        pasteToActiveWindow();
#else
        toggleVisibility();
#endif
    }
}

void ClipboardHistoryOverlay::onEmojiItemClicked(QListWidgetItem* item)
{
    if (item) {
        QString emoji = item->text();
        m_ignoreNextClipboardChange = true;
        m_clipboard->setText(emoji);
        
        // Вставляем в активное окно (только на Windows)
#ifdef Q_OS_WIN
        pasteToActiveWindow();
#else
        toggleVisibility();
#endif
    }
}

void ClipboardHistoryOverlay::updateCloseButtonRect()
{
    int closeButtonSize = 20;
    int margin = 10;
    m_closeButtonRect = QRect(width() - closeButtonSize - margin, margin,
                             closeButtonSize, closeButtonSize);
}

bool ClipboardHistoryOverlay::isInCloseButton(const QPoint& pos) const
{
    return m_closeButtonRect.contains(pos);
}