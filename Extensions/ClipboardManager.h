#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#include "../ExtensionManager.h"
#include <QWidget>

class ClipboardManagerExtension : public ExtensionInterface
{
public:
    ClipboardManagerExtension() = default;
    ~ClipboardManagerExtension() = default;

    QString name() const override { return "ClipboardManager"; }
    QString version() const override { return "1.0"; }
    ExtensionDisplayLocation displayLocation() const override { return ExtensionDisplayLocation::None; }
    QWidget* createWidget(QWidget* parent = nullptr) override;
};

REGISTER_EXTENSION(ClipboardManagerExtension)

#endif // CLIPBOARDMANAGER_H