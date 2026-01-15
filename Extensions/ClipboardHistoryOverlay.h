#ifndef CLIPBOARDHISTORYOVERLAY_H
#define CLIPBOARDHISTORYOVERLAY_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVector>
#include <QCursor>
#include <QClipboard>
#include <QTabWidget>
#include <QLineEdit>
#include <QImage>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ClipboardHistoryOverlay : public QWidget
{
    Q_OBJECT

public:
    // Константы размеров
    static inline const int TAB_WIDTH = 450;
    static inline const int EMOJI_TAB_HEIGHT = 600;
    static inline const int HISTORY_INITIAL_HEIGHT = 600;
    static inline const int HISTORY_MAX_HEIGHT = 800;

    explicit ClipboardHistoryOverlay(QWidget* parent = nullptr);
    ~ClipboardHistoryOverlay();

    void toggleVisibility();
    void updateClipboardHistory();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onClipboardDataChanged();
    void onEmojiItemClicked(QListWidgetItem* item);
    void onTabChanged(int index);
    void filterEmojis(const QString& searchText);

private:
    void updatePosition();
    void adjustPositionToScreen();
    void refreshList();
    void refreshEmojiList();
    void addToHistory(const QString& text);
    void addImageToHistory(const QImage& image);
    bool isImageFile(const QString& filePath);
    void updateCloseButtonRect();
    bool isInCloseButton(const QPoint& pos) const;
    
#ifdef Q_OS_WIN
    void pasteToActiveWindow();
    HWND m_previousWindow;
#endif

    QTabWidget* m_tabWidget;
    QListWidget* m_historyList;
    QListWidget* m_emojiList;
    QLineEdit* m_emojiSearchEdit;
    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    bool m_visible;
    QVector<QString> m_clipboardHistory;
    QVector<QImage> m_imageHistory;
    QClipboard* m_clipboard;
    bool m_ignoreNextClipboardChange;
    QPoint m_initialPosition; // Сохраняем начальную позицию
    bool m_positionInitialized; // Флаг инициализации позиции
    bool m_closeButtonHovered;
    QRect m_closeButtonRect;
};

#endif // CLIPBOARDHISTORYOVERLAY_H