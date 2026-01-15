#ifndef TIMEREXTENSION_H
#define TIMEREXTENSION_H

#include "..//ExtensionManager.h"
#include "..//TimerWidget.h"

class TimerExtension : public ExtensionInterface
{
public:
    TimerExtension() = default;
    ~TimerExtension() = default;

    QString name() const override { return "Timer"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;

    // Указываем, что это расширение должно отображаться только в верхней панели
    ExtensionDisplayLocation displayLocation() const override {
        return ExtensionDisplayLocation::TopPanel;
    }
};

REGISTER_EXTENSION_CONDITIONAL(TimerExtension)

#endif // TIMEREXTENSION_H