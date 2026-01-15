#include "CalendarWidget.h"

#include <QTimer>
#include <QStyleOption>
#include <QTableView>
#include <QApplication>
#include <QAbstractItemModel>

#include "ExtensionManager.h"

// Реализация делегата для отрисовки зеленой точки
void CalendarWidget::CalendarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    // Сначала рисуем стандартный элемент
    QStyledItemDelegate::paint(painter, option, index);

    // Получаем модель из табличного представления
    QTableView *tableView = qobject_cast<QTableView*>(parent());
    if (!tableView) return;

    QAbstractItemModel *model = tableView->model();
    if (!model) return;

    // Получаем день месяца из модели
    int day = model->data(index, Qt::DisplayRole).toInt();

    if (day > 0) {
        // Получаем текущий отображаемый месяц и год из календаря
        int month = m_calendar->monthShown();
        int year = m_calendar->yearShown();

        // Создаем дату для текущей ячейки
        QDate date(year, month, day);

        // Получаем текущую дату
        QDate currentDate = QDate::currentDate();

        // Проверяем, является ли дата текущей
        if (date.isValid() && date == currentDate) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Зеленая точка
            painter->setBrush(QColor(76, 175, 80));  // Зеленый цвет как в Material Design
            painter->setPen(Qt::NoPen);

            // Позиция точки в нижнем правом углу ячейки
            QRect rect = option.rect;
            int dotSize = CALENDAR_CURRENT_DOT_SIZE;
            int margin = CALENDAR_CURRENT_DOT_MARGIN;
            QPoint center = rect.bottomRight() - QPoint(margin + dotSize/2, margin + dotSize/2);

            painter->drawEllipse(center, dotSize/2, dotSize/2);
            painter->restore();
        }
    }
}

CalendarWidget::CalendarWidget(QWidget *parent) : QWidget(parent)
    , m_innerWidget(nullptr)
    , m_calendar(nullptr)
    , m_extensionsLayout(nullptr)
    , m_separator(nullptr)
    , m_innerLayout(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_innerWidget = new InnerWidget(this);
    layout->addWidget(m_innerWidget);

    m_innerLayout = new QVBoxLayout(m_innerWidget);
    m_innerLayout->setContentsMargins(CALENDAR_PADDING, CALENDAR_PADDING, CALENDAR_PADDING, CALENDAR_PADDING);
    m_innerLayout->setSpacing(CALENDAR_INNER_PADDING);

    m_calendar = new QCalendarWidget(m_innerWidget);
    m_calendar->setGridVisible(true);
    m_calendar->setNavigationBarVisible(true);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    QString calendarStyle = QString(
        "QCalendarWidget {"
        "    background-color: transparent;"
        "    color: %1;"
        "    font-family: '%2';"
        "    font-size: %3px;"
        "    font-weight: bold;"
        "    border: none;"
        "    min-width: %4px;"
        "}"
        "QCalendarWidget QAbstractItemView {"
        "    background-color: transparent;"
        "    selection-background-color: %5;"
        "    selection-color: %1;"
        "    border: none;"
        "    outline: none;"
        "}"
        "QCalendarWidget QToolButton {"
        "    color: %1;"
        "    background-color: transparent;"
        "    border: 1px solid %6;"
        "    border-radius: %7px;"
        "    padding: %8px %9px;"
        "    icon-size: %10px;"
        "    font-weight: bold;"
        "    font-family: '%2';"
        "    font-size: %3px;"
        "    min-width: %11px;"
        "    min-height: %12px;"
        "    margin-bottom: %26px;"  // Отступ снизу для разделителя
        "}"
        "QCalendarWidget QToolButton:hover {"
        "    background-color: %13;"
        "}"
        "QCalendarWidget QWidget#qt_calendar_navigationbar {"
        "    background-color: %14;"
        "    border-top-left-radius: %15px;"
        "    border-top-right-radius: %15px;"
        "    padding: %16px %17px %18px %17px;"
        "    spacing: %19px;"
        "    border: none;"
        "    border-bottom: %20px solid %6;"  // Разделитель под навигационной панелью
        "}"
        "QCalendarWidget QMenu {"
        "    background-color: %14;"
        "    color: %1;"
        "    border: 2px solid %6;"
        "    border-radius: %21px;"
        "    font-size: %22px;"
        "    font-family: '%2';"
        "    font-weight: bold;"
        "}"
        "QCalendarWidget QMenu::item {"
        "    padding: %23px 16px;"
        "    border-radius: %7px;"
        "    font-size: %22px;"
        "    font-family: '%2';"
        "    font-weight: bold;"
        "    margin: 2px;"
        "    min-width: %24px;"
        "}"
        "QCalendarWidget QMenu::item:selected {"
        "    background-color: %5;"
        "}"
        "QCalendarWidget QSpinBox {"
        "    color: %1;"
        "    background-color: transparent;"
        "    border: 1px solid %6;"
        "    border-radius: %7px;"
        "    padding: %8px %9px;"
        "    font-weight: bold;"
        "    font-family: '%2';"
        "    font-size: %3px;"
        "    min-width: %25px;"
        "    margin-bottom: %26px;"  // Отступ снизу для разделителя
        "}"
        "QCalendarWidget QSpinBox::up-button, QCalendarWidget QSpinBox::down-button {"
        "    background-color: transparent;"
        "    border: none;"
        "    width: %27px;"
        "}"
        "QCalendarWidget QWidget#qt_calendar_year button,"
        "QCalendarWidget QWidget#qt_calendar_month button {"
        "    color: %1;"
        "    background-color: transparent;"
        "    border: 1px solid %6;"
        "    border-radius: %7px;"
        "    padding: %8px %9px;"
        "    font-weight: bold;"
        "    font-family: '%2';"
        "    font-size: %3px;"
        "    min-width: %24px;"
        "}"
        "QCalendarWidget QTableView {"
        "    gridline-color: %6;"
        "    border: %28px solid %6;"  // Добавляем границу вокруг таблицы
        "    border-top: none;"
        "    border-bottom-left-radius: %29px;"
        "    border-bottom-right-radius: %29px;"
        "    margin-top: %30px;"  // Отступ сверху от разделителя
        "}"
        "QCalendarWidget QTableView::item {"
        "    padding: %31px;"
        "    border: none;"
        "}"
        "QCalendarWidget QTableView::item:selected {"
        "    background-color: %5;"
        "    border-radius: %7px;"
        "}"
        "QCalendarWidget QHeaderView::section {"
        "    background-color: %14;"
        "    color: %1;"
        "    border: none;"
        "    border-bottom: %28px solid %6;"  // Граница снизу заголовков
        "    padding: %32px %31px;"
        "    font-weight: bold;"
        "    font-family: '%2';"
        "    font-size: %3px;"
        "}"
    ).arg(PowerMenuSettings::MENU_TEXT_COLOR)
     .arg(PowerMenuSettings::FONT_FAMILY)
     .arg(PowerMenuSettings::MENU_FONT_SIZE)
     .arg(CALENDAR_MIN_WIDTH)
     .arg(PowerMenuSettings::MENU_SELECTED_BACKGROUND_COLOR)
     .arg(PowerMenuSettings::MENU_BORDER_COLOR)
     .arg(PowerMenuSettings::MENU_ITEM_BORDER_RADIUS)
     .arg(CALENDAR_BUTTON_PADDING)
     .arg(CALENDAR_BUTTON_PADDING + 2)
     .arg(ARROW_ICON_SIZE)
     .arg(CALENDAR_BUTTON_MIN_WIDTH)
     .arg(CALENDAR_BUTTON_HEIGHT)
     .arg(PowerMenuSettings::BUTTON_HOVER_BACKGROUND)
     .arg(PowerMenuSettings::MENU_BACKGROUND_COLOR)
     .arg(PowerMenuSettings::MENU_BORDER_RADIUS)
     .arg(CALENDAR_NAV_PADDING)
     .arg(CALENDAR_NAV_PADDING)
     .arg(CALENDAR_NAV_PADDING)
     .arg(CALENDAR_NAV_SPACING)
     .arg(CALENDAR_NAV_SEPARATOR_HEIGHT)
     .arg(PowerMenuSettings::ADVANCED_MENU_BORDER_RADIUS)
     .arg(PowerMenuSettings::ADVANCED_MENU_FONT_SIZE)
     .arg(PowerMenuSettings::ADVANCED_MENU_ITEM_PADDING)
     .arg(CALENDAR_MENU_MIN_WIDTH)
     .arg(CALENDAR_SPINBOX_MIN_WIDTH)
     .arg(CALENDAR_NAV_BOTTOM_MARGIN)
     .arg(ARROW_ICON_SIZE - 2)
     .arg(CALENDAR_TABLE_BORDER)
     .arg(PowerMenuSettings::MENU_ITEM_BORDER_RADIUS)
     .arg(CALENDAR_TABLE_TOP_MARGIN)
     .arg(CALENDAR_CELL_PADDING)
     .arg(CALENDAR_CELL_PADDING + 2);

    m_calendar->setStyleSheet(calendarStyle);
    m_innerLayout->addWidget(m_calendar);

    // Устанавливаем пользовательский делегат для отрисовки зеленой точки
    QTableView *tableView = m_calendar->findChild<QTableView*>();
    if (tableView) {
        // Создаем делегат с tableView в качестве родителя
        CalendarDelegate *delegate = new CalendarDelegate(m_calendar, tableView);
        tableView->setItemDelegate(delegate);

        // Принудительно обновляем вид при изменении месяца/года
        connect(m_calendar, &QCalendarWidget::currentPageChanged,
                [tableView]() { tableView->viewport()->update(); });
    }

    // Загружаем расширения
    loadExtensions();

    // Обновляем размер после загрузки расширений
    updateSize();
}

void CalendarWidget::updateSize()
{
    // Рассчитываем общую высоту расширений
    int extensionsHeight = calculateTotalExtensionsHeight();

    // Базовый размер календаря + высота расширений + отступы
    QSize baseCalendarSize = m_calendar->sizeHint();

    int calculatedWidth = baseCalendarSize.width() + (CALENDAR_PADDING * 2);
    int minWidth = CALENDAR_MIN_WIDTH + (CALENDAR_PADDING * 2);

    int totalWidth = calculatedWidth > minWidth ? calculatedWidth : minWidth;
    int totalHeight = baseCalendarSize.height() + extensionsHeight + (CALENDAR_PADDING * 2);

    setMinimumSize(totalWidth, totalHeight);
    resize(minimumSize());
}

int CalendarWidget::calculateTotalExtensionsHeight() const
{
    int totalHeight = 0;

    if (m_extensionsLayout) {
        for (int i = 0; i < m_extensionsLayout->count(); ++i) {
            QWidget *widget = m_extensionsLayout->itemAt(i)->widget();
            if (widget && widget->isVisible()) {
                totalHeight += widget->sizeHint().height() + m_extensionsLayout->spacing();
            }
        }

        // Добавляем высоту разделителя, если есть расширения
        if (totalHeight > 0 && m_separator && m_separator->isVisible()) {
            totalHeight += m_separator->height() + m_innerLayout->spacing();
        }
    }

    return totalHeight;
}

void CalendarWidget::loadExtensions()
{
    ExtensionManager& em = ExtensionManager::instance();

    // Получаем только расширения для календаря
    QList<QString> extensionNames = em.extensionsForLocation(ExtensionDisplayLocation::Calendar);

    // Создаем разделитель только если есть расширения
    if (!extensionNames.isEmpty()) {
        m_separator = new QFrame(m_innerWidget);
        m_separator->setFrameShape(QFrame::HLine);
        m_separator->setFrameShadow(QFrame::Sunken);
        m_separator->setStyleSheet(QString("color: %1; background-color: %1; border-radius: 1px;")
                                  .arg(PowerMenuSettings::MENU_BORDER_COLOR));
        m_separator->setFixedHeight(1);
        m_innerLayout->addWidget(m_separator);
    }

    // Создаем layout для расширений
    m_extensionsLayout = new QVBoxLayout();
    m_extensionsLayout->setContentsMargins(0, 0, 0, 0);
    m_extensionsLayout->setSpacing(5);
    m_innerLayout->addLayout(m_extensionsLayout);

    for (const QString& name : extensionNames) {
        QWidget* extensionWidget = em.createExtensionWidget(name, m_innerWidget);
        if (extensionWidget) {
            // Устанавливаем стиль для расширений
            extensionWidget->setStyleSheet(QString(
                "background-color: transparent;"
                "color: %1;"
                "font-family: '%2';"
                "font-weight: bold;"
                "font-size: %3px;"
            ).arg(PowerMenuSettings::MENU_TEXT_COLOR)
             .arg(PowerMenuSettings::FONT_FAMILY)
             .arg(PowerMenuSettings::MENU_FONT_SIZE));

            m_extensionsLayout->addWidget(extensionWidget);
        }
    }

    // Обновляем размер после добавления всех расширений
    QTimer::singleShot(100, this, &CalendarWidget::updateSize);
}

void CalendarWidget::InnerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Рисуем фон с закругленными углами
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(PowerMenuSettings::MENU_BACKGROUND_COLOR));
    painter.drawRoundedRect(rect(),
                           PowerMenuSettings::MENU_BORDER_RADIUS,
                           PowerMenuSettings::MENU_BORDER_RADIUS);

    // Рисуем границу
    painter.setPen(QPen(QColor(PowerMenuSettings::MENU_BORDER_COLOR), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect(),
                           PowerMenuSettings::MENU_BORDER_RADIUS,
                           PowerMenuSettings::MENU_BORDER_RADIUS);
}