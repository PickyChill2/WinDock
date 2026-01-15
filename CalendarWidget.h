#ifndef CALENDARWIDGET_H
#define CALENDARWIDGET_H

#include <QWidget>
#include <QCalendarWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QFrame>
#include <QStyledItemDelegate>

#include "PowerMenuConstants.h"

class ExtensionManager;

class CalendarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarWidget(QWidget *parent = nullptr);
    void updateSize();

private:
    class InnerWidget : public QWidget {
    public:
        InnerWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    protected:
        void paintEvent(QPaintEvent *event) override;
    };

    class CalendarDelegate : public QStyledItemDelegate {
    public:
        explicit CalendarDelegate(QCalendarWidget *calendar, QObject *parent = nullptr)
            : QStyledItemDelegate(parent), m_calendar(calendar) {}

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                  const QModelIndex &index) const override;

    private:
        QCalendarWidget *m_calendar;
    };

    InnerWidget *m_innerWidget;
    QCalendarWidget *m_calendar;
    QVBoxLayout *m_extensionsLayout;
    QFrame *m_separator;
    QVBoxLayout *m_innerLayout;

    // Константы для размеров и отступов
    static constexpr int CALENDAR_PADDING = 8;              // Отступы вокруг календаря
    static constexpr int CALENDAR_MIN_WIDTH = 300;          // Минимальная ширина календаря
    static constexpr int CALENDAR_INNER_PADDING = 6;        // Внутренние отступы элементов
    static constexpr int CALENDAR_NAV_PADDING = 6;          // Отступы навигационной панели
    static constexpr int CALENDAR_NAV_SPACING = 8;          // Расстояние между элементами навигации
    static constexpr int CALENDAR_BUTTON_HEIGHT = 15;       // Высота кнопок
    static constexpr int CALENDAR_BUTTON_PADDING = 5;       // Отступы кнопок
    static constexpr int CALENDAR_BUTTON_MIN_WIDTH = 56;    // Минимальная ширина кнопок
    static constexpr int CALENDAR_SPINBOX_MIN_WIDTH = 66;   // Минимальная ширина спинбокса
    static constexpr int CALENDAR_MENU_MIN_WIDTH = 76;      // Минимальная ширина меню
    static constexpr int CALENDAR_CELL_PADDING = 4;         // Отступы ячеек
    static constexpr int CALENDAR_NAV_SEPARATOR_HEIGHT = 1; // Высота разделителя под навигацией
    static constexpr int CALENDAR_NAV_BOTTOM_MARGIN = 4;    // Отступ от разделителя для кнопок
    static constexpr int CALENDAR_TABLE_TOP_MARGIN = 4;     // Отступ от разделителя для календаря
    static constexpr int CALENDAR_TABLE_BORDER = 1;         // Толщина границы таблицы
    static constexpr int CALENDAR_CURRENT_DOT_SIZE = 4;     // Размер точки текущей даты
    static constexpr int CALENDAR_CURRENT_DOT_MARGIN = 2;   // Отступ точки от края
    static constexpr int ARROW_ICON_SIZE = 18;              // Размер иконок стрелок

    void loadExtensions();
    int calculateTotalExtensionsHeight() const;
};

#endif // CALENDARWIDGET_H