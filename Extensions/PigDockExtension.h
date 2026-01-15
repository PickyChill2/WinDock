#ifndef PIGDOCKEXTENSION_H
#define PIGDOCKEXTENSION_H

#include "../ExtensionManager.h"
#include "../Dock.h"
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QDebug>
#include <QStorageInfo>
#include <QProcess>
#include <QMessageBox>
#include <QTimer>
#include <QMetaObject>

class PigDockExtension : public QObject, public ExtensionInterface
{
    Q_OBJECT

public:
    QString name() const override;
    QString version() const override;
    ExtensionDisplayLocation displayLocation() const override;
    QWidget* createWidget(QWidget* parent = nullptr) override;
    void populateDockContextMenu(QMenu* menu, DockContextMenuType menuType, DockItem* item = nullptr) override;

private slots:
    void onTrashButtonClicked();
    void onSettingsAction();
    void onGlobalPigAction();
    void onItemPigAction();
    void checkTrashStatus();
    void performTrashEmpty();

private:
    void showTrashContextMenu(QWidget* parent, const QPoint& pos);
    void openTrash();
    void emptyTrash();
    bool isTrashEmpty();
    void safeShowEmptyMessage();
    void safeShowConfirmMessage();
    void refreshTrashState();

    DockItem* m_currentItem = nullptr;
    QPushButton* m_trashButton = nullptr;
    QTimer* m_trashCheckTimer = nullptr;
    bool m_isTrashEmpty = true;
    QTimer* m_refreshTimer = nullptr;
};

#endif // PIGDOCKEXTENSION_H