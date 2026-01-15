#ifndef CLIPBOARDMANAGERWIDGET_H
#define CLIPBOARDMANAGERWIDGET_H

#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ClipboardHistoryOverlay;

class ClipboardManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClipboardManagerWidget(QWidget* parent = nullptr);
    ~ClipboardManagerWidget();

    ClipboardHistoryOverlay* m_overlay;

#ifdef Q_OS_WIN
protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void registerHotkey();
    void unregisterHotkey();

    int m_hotkeyId;
#endif
};

#endif // CLIPBOARDMANAGERWIDGET_H