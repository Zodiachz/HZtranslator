cmake_minimum_required(VERSION 3.22)

project(HypersTranslator
    VERSION 1.0.4
    DESCRIPTION "HYPERS TRANSLATOR - system-wide translation for Windows 10 and 11"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "HYPERS TRANSLATOR is a Windows 10/11 application.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "Build with MSVC (Visual Studio 2022+). Windows OCR is accessed through "
                        "C++/WinRT, whose headers ship with the Windows SDK for MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HYPERS_BUILD_TESTS "Build unit tests" ON)

# Portable layout: HypersTranslator.exe sits next to its Qt DLLs (windeployqt
# deploys into CMAKE_INSTALL_BINDIR), so the zip runs without an installer.
set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Executable/DLL install directory")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent)
qt_standard_project_setup()

set(HYPERS_APP_NAME "HYPERS TRANSLATOR")
configure_file(src/app/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)
configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc @ONLY)

# ---------------------------------------------------------------------------
# Core library: everything except main(), so unit tests can link against it.
# ---------------------------------------------------------------------------
add_library(hypers_core STATIC
    src/app/AppContext.h
    src/app/HypersApp.cpp               src/app/HypersApp.h

    src/core/AppSettings.cpp            src/core/AppSettings.h
    src/core/HistoryStore.cpp           src/core/HistoryStore.h
    src/core/Languages.cpp              src/core/Languages.h
    src/core/Logger.cpp                 src/core/Logger.h
    src/core/SingleInstance.cpp         src/core/SingleInstance.h
    src/core/AsyncWait.cpp              src/core/AsyncWait.h

    src/translation/TranslationTypes.cpp      src/translation/TranslationTypes.h
    src/translation/ITranslationProvider.h
    src/translation/DeepLProvider.cpp         src/translation/DeepLProvider.h
    src/translation/GoogleTranslateProvider.cpp src/translation/GoogleTranslateProvider.h
    src/translation/RetryPolicy.cpp           src/translation/RetryPolicy.h
    src/translation/TextSegmenter.cpp         src/translation/TextSegmenter.h
    src/translation/TranslationService.cpp    src/translation/TranslationService.h

    src/platform/ISecretStore.h
    src/platform/win/AutoStart.cpp            src/platform/win/AutoStart.h
    src/platform/win/Clipboard.cpp            src/platform/win/Clipboard.h
    src/platform/win/GlobalHotkeyManager.cpp  src/platform/win/GlobalHotkeyManager.h
    src/platform/win/InputSimulator.cpp       src/platform/win/InputSimulator.h
    src/platform/win/KeyMapping.cpp           src/platform/win/KeyMapping.h
    src/platform/win/MessageWindow.cpp        src/platform/win/MessageWindow.h
    src/platform/win/WinCredentialStore.cpp   src/platform/win/WinCredentialStore.h
    src/platform/win/WindowsOcrEngine.cpp     src/platform/win/WindowsOcrEngine.h
    src/platform/win/WindowUtils.cpp          src/platform/win/WindowUtils.h

    src/features/FeatureCommon.h
    src/features/OcrTranslator.cpp        src/features/OcrTranslator.h
    src/features/SelectionTranslator.cpp  src/features/SelectionTranslator.h

    src/ui/HistoryPage.cpp        src/ui/HistoryPage.h
    src/ui/HotkeyEdit.cpp         src/ui/HotkeyEdit.h
    src/ui/LanguageComboBox.cpp   src/ui/LanguageComboBox.h
    src/ui/MainWindow.cpp         src/ui/MainWindow.h
    src/ui/RegionSelector.cpp     src/ui/RegionSelector.h
    src/ui/ResultPopup.cpp        src/ui/ResultPopup.h
    src/ui/SettingsPage.cpp       src/ui/SettingsPage.h
    src/ui/Theme.cpp              src/ui/Theme.h
    src/ui/TranslatePage.cpp      src/ui/TranslatePage.h
    src/ui/TrayController.cpp     src/ui/TrayController.h
    src/ui/UiHelpers.h
)

target_include_directories(hypers_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(hypers_core PUBLIC
    UNICODE _UNICODE
    NOMINMAX WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00 WINVER=0x0A00
    QT_NO_KEYWORDS                       # avoids clashes with C++/WinRT and Win32 headers
    QT_USE_QSTRINGBUILDER
    QT_NO_FOREACH)

target_compile_options(hypers_core PUBLIC
    /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc /bigobj /MP
    /wd4127)                            # "conditional expression is constant" from Qt templates

target_link_libraries(hypers_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent
    user32 advapi32 dwmapi ole32 windowsapp)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
qt_add_executable(HypersTranslator WIN32
    src/main.cpp
    resources/app.manifest
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc)

qt_add_resources(HypersTranslator "hypers_resources"
    PREFIX "/"
    BASE resources
    FILES
        resources/styles/dark.qss
        resources/icons/app-16.png
        resources/icons/app-24.png
        resources/icons/app-32.png
        resources/icons/app-48.png
        resources/icons/app-64.png
        resources/icons/app-128.png
        resources/icons/app-256.png
        resources/icons/tray-paused-32.png
        resources/icons/chevron-down.png
        resources/icons/chevron-down@2x.png
        resources/icons/chevron-up.png
        resources/icons/chevron-up@2x.png
        resources/icons/check.png
        resources/icons/check@2x.png)

target_link_libraries(HypersTranslator PRIVATE hypers_core)
set_target_properties(HypersTranslator PROPERTIES OUTPUT_NAME "HypersTranslator")

# ---------------------------------------------------------------------------
# Install + Qt deployment (windeployqt) + ZIP package
# ---------------------------------------------------------------------------
install(TARGETS HypersTranslator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_app_script(
    TARGET HypersTranslator
    OUTPUT_SCRIPT hypers_deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${hypers_deploy_script})

set(CPACK_PACKAGE_NAME "HypersTranslator")
set(CPACK_PACKAGE_VENDOR "Hypers")
set(CPACK_GENERATOR "ZIP")
include(CPack)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(HYPERS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
