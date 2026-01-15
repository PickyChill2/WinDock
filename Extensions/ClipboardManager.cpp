#include "ClipboardManager.h"
#include "ClipboardManagerWidget.h"

QWidget* ClipboardManagerExtension::createWidget(QWidget* parent)
{
    return new ClipboardManagerWidget(parent);
}