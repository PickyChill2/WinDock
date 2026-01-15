#include "..//ExtensionManager.h"
#include "WeatherExtension.h"
#include "WinDockBar.h"

// Инициализация всех расширений
INITIALIZE_EXTENSIONS
    REGISTER_EXTENSION_CLASS(WeatherExtension);
REGISTER_EXTENSION_CLASS(WinDockBar);
// Добавьте здесь регистрацию других расширений
FINALIZE_EXTENSIONS