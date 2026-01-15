cmake_minimum_required(VERSION 3.20)
project(Dock)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "W:/QT/6.9.3/mingw_64")

# Включаем автоматический MOC, UIC и RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network Multimedia MultimediaWidgets Concurrent OpenGLWidgets)

qt_standard_project_setup()


# Функция для чтения расширений из файла
function(load_extensions_from_file file_path)
    if(EXISTS ${file_path})
        file(READ ${file_path} EXTENSIONS_CONTENT)
        string(REPLACE "\n" ";" EXTENSIONS_LIST "${EXTENSIONS_CONTENT}")

        foreach(EXTENSION_LINE ${EXTENSIONS_LIST})
            # Пропускаем комментарии и пустые строки
            if(NOT EXTENSION_LINE MATCHES "^[ \t]*#" AND NOT EXTENSION_LINE STREQUAL "")
                # Разделяем имя класса и пути к файлам
                string(REGEX MATCH "([^=]+)=(.+)" _ ${EXTENSION_LINE})
                set(EXTENSION_NAME ${CMAKE_MATCH_1})
                set(EXTENSION_FILES ${CMAKE_MATCH_2})

                # Разделяем пути к файлам (теперь может быть несколько файлов)
                string(REPLACE "," ";" FILE_LIST "${EXTENSION_FILES}")

                foreach(EXTENSION_FILE ${FILE_LIST})
                    list(APPEND EXTENSIONS_SOURCES ${EXTENSION_FILE})
                    message(STATUS "Found extension file: ${EXTENSION_FILE} for ${EXTENSION_NAME}")
                endforeach()
            endif()
        endforeach()

        # Сохраняем список в родительской области видимости
        set(EXTENSIONS_SOURCES ${EXTENSIONS_SOURCES} PARENT_SCOPE)
    else()
        message(WARNING "Extensions file ${file_path} not found")
    endif()
endfunction()

# Загружаем расширения из файла
load_extensions_from_file("${CMAKE_CURRENT_SOURCE_DIR}/extensions.txt")

# Основные исходные файлы
set(MAIN_SOURCES
        main.cpp
        Dock.cpp
        Dock.h
        BaseConstants.h
        DockConstants.h
        DockContextConstants.h
        DockAnimationManager.cpp
        DockAnimationManager.h
        WindowPreviewDialog.cpp
        WindowPreviewDialog.h
        TaskbarBlocker.cpp
        TaskbarBlocker.h
        AppManager.cpp
        AppManager.h
        TopPanel.cpp
        TopPanel.h
        PowerMenu.cpp
        PowerMenu.h
        CalendarWidget.cpp
        CalendarWidget.h
        TopPanelConstants.h
        TopPanelManager.cpp
        TopPanelManager.h
        SystemTrayWidget.cpp
        SystemTrayWidget.h
        TrayConstants.h
        VolumeWidget.cpp
        VolumeWidget.h
        LanguageWidget.cpp
        LanguageWidget.h
        ExtensionManager.cpp
        ExtensionManager.h
        ExtensionsMenu.cpp
        ExtensionsMenu.h
        TimerWidget.h
        # Добавляем файлы компонентов WinDockBar
        #_________________________________
        Extensions/AppVolumeWidget.cpp
        Extensions/AppVolumeWidget.h
        Extensions/VolumeMixerWidget.cpp
        Extensions/VolumeMixerWidget.h
        #_________________________________
        # Все остальное
        SettingsMenu.cpp
        SettingsMenu.h
        TimerNotification.cpp
        TimerNotification.h
        StartMenu.cpp
        StartMenu.h
        StartMenuAppManager.cpp
        StartMenuAppManager.h
        DockMenuAppManager.cpp
        DockMenuAppManager.h
        HiddenAppsDialog.cpp
        HiddenAppsDialog.h
        DockContextMenu.cpp
        DockContextMenu.h
        WinKeyListener.cpp
        WinKeyListener.h
        SettingsSignalBridge.cpp
        SettingsSignalBridge.h
        ExtensionLayoutManager.cpp
        ExtensionLayoutManager.h
        ManualProcessDialog.cpp
        ManualProcessDialog.h
        WindowButtonManager.cpp
        WindowButtonManager.h
        WindowArrangementPanel.cpp
        WindowArrangementPanel.h
        WindowMoveHandler.cpp
        WindowMoveHandler.h
        VolumeOverlay.cpp
        VolumeOverlay.h
        VolumeKeyHook.cpp
        VolumeKeyHook.h
        WindowZOrderManager.cpp
        WindowZOrderManager.h
        AudioDeviceManager.cpp
        AudioDeviceManager.h
        ScreenshotManager.cpp
        ScreenshotManager.h
        EmojiList.cpp
        EmojiList.h
        EmojiCategories/EmojiSmileys.h
        EmojiCategories/EmojiHearts.h
        EmojiCategories/EmojiAnimals.h
        EmojiCategories/EmojiFood.h
        EmojiCategories/EmojiSports.h
        EmojiCategories/EmojiFish.h
        EmojiCategories/EmojiTransport.h
        EmojiCategories/EmojiObjects.h
        TrayProcesses.cpp
        TrayProcesses.h
        BackgroundSettingsDialog.cpp
        BackgroundSettingsDialog.h
        IconCache.cpp
        IconCache.h
        PowerMenuConstants.h
        WindowArrangementManager.cpp
        WindowArrangementManager.h
        PinnedAppsDialog.cpp
        PinnedAppsDialog.h
        #_________________________________
        #Для видео и фонового изображения
        Wallpaper/MultiDesktopManager.cpp
        Wallpaper/MultiDesktopManager.h
        Wallpaper/MediaRenderer.cpp
        Wallpaper/MediaRenderer.h
        Wallpaper/ImageRenderer.cpp
        Wallpaper/ImageRenderer.h
        Wallpaper/VideoRenderer.cpp
        Wallpaper/VideoRenderer.h
        Wallpaper/ScreenSettings.h
        Wallpaper/CommonDefines.h
        Wallpaper/DesktopBackground.cpp
        Wallpaper/DesktopBackground.h
        #_______________________________
        Search/SearchWindow.cpp
        Search/SearchWindow.h
        Search/ApplicationSearcher.cpp
        Search/ApplicationSearcher.h
        Search/SearchResult.h
        AddTrayAppDialog.cpp
        AddTrayAppDialog.h
)

# Объединяем основные файлы и файлы расширений
set(ALL_SOURCES ${MAIN_SOURCES} ${EXTENSIONS_SOURCES})

qt_add_executable(Dock ${ALL_SOURCES})

# Добавляем определение для VLC
target_compile_definitions(Dock PRIVATE HAVE_LIBVLC)

# Включаем путь к заголовкам VLC
target_include_directories(Dock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vlc/include)

# Основные библиотеки Qt
target_link_libraries(Dock PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::Network
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Concurrent
        Qt6::OpenGLWidgets
)

# Для Windows добавляем необходимые системные библиотеки
if(WIN32)
    target_link_libraries(Dock PRIVATE
            shell32
            psapi
            user32
            comctl32
            powrprof
            ole32
            oleaut32
            uuid
            mmdevapi
            winmm
            propsys
            avrt
            d3d11
            dxgi
            dxguid
            d3d12
            wininet
            dwmapi
            gdi32
            version.lib
    )

    # Добавляем определения для Windows
    target_compile_definitions(Dock PRIVATE
            UNICODE
            _UNICODE
    )
endif()

# Автоматическое развертывание Qt
if(WIN32 AND QT6_FOUND)
    # Находим windeployqt
    get_target_property(QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR ${QMAKE_EXE} DIRECTORY)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${QT_BIN_DIR})

    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET Dock POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                "$<TARGET_FILE:Dock>"
                --verbose 0
                --no-compiler-runtime
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-translations
                COMMENT "Deploying Qt libraries..."
        )

        # Копируем DLL файлы VLC в выходную директорию
        add_custom_command(TARGET Dock POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/vlc/libvlc.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/libvlc.dll"
                COMMENT "Copying VLC libraries..."
        )

        add_custom_command(TARGET Dock POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/vlc/libvlccore.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/libvlccore.dll"
        )

        # Также копируем в папку vlc рядом с исполняемым файлом
        add_custom_command(TARGET Dock POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/vlc"
        )

        add_custom_command(TARGET Dock POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/vlc/libvlc.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/vlc/libvlc.dll"
        )

        add_custom_command(TARGET Dock POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/vlc/libvlccore.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/vlc/libvlccore.dll"
        )
    else()
        message(WARNING "windeployqt not found, application might not run properly")
    endif()
endif()