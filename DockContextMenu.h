#ifndef DOCKCONTEXTMENU_H
#define DOCKCONTEXTMENU_H

#include <QMenu>

class DockContextMenu : public QMenu
{
    Q_OBJECT

public:
    explicit DockContextMenu(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
};

#endif // DOCKCONTEXTMENU_H