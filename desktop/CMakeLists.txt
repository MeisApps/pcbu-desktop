cmake_minimum_required(VERSION 3.22)
project(pcbu_desktop)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED QT_BASE_DIR)
    message(FATAL_ERROR "QT_BASE_DIR is not set.")
endif()
if(WIN32)
    set(QT_HOST_PATH "${QT_BASE_DIR}/msvc2022_64")
    if(TARGET_ARCH STREQUAL "x64")
        set(CMAKE_PREFIX_PATH "${QT_BASE_DIR}/msvc2022_64")
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(CMAKE_PREFIX_PATH "${QT_BASE_DIR}/msvc2022_arm64")
    endif()
elseif(APPLE)
    set(CMAKE_PREFIX_PATH "${QT_BASE_DIR}/macos")
endif()

find_package(Qt6 COMPONENTS Core Gui Quick QuickControls2 REQUIRED)

if(WIN32)
    add_compile_definitions(WINDOWS _WIN32_WINNT=0x0A00)
    set(PLATFORM_SRC
            src/installer/WinFirewallHelper.cpp
            src/installer/WinFirewallHelper.h
            src/installer/ServiceInstaller.Win.cpp
            src/winres.rc)

    if(TARGET_ARCH STREQUAL "x64")
        set(PLATFORM_RES res/resources_win_x64.qrc)
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(PLATFORM_RES res/resources_win_arm64.qrc)
    else()
        message(FATAL_ERROR "Undefined resources for ${TARGET_ARCH} on WINDOWS.")
    endif()
elseif(APPLE)
    add_compile_definitions(APPLE)
    set(PLATFORM_SRC
            src/installer/PAMHelper.cpp
            src/installer/PAMHelper.h
            src/installer/ServiceInstaller.Mac.cpp
            res/icons/icons.icns
    )
    set(MACOSX_BUNDLE_ICON_FILE icons.icns)
    set_source_files_properties(res/icons/icons.icns PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")

    if(TARGET_ARCH STREQUAL "arm64")
        set(PLATFORM_RES res/resources_mac_arm64.qrc)
    else()
        message(FATAL_ERROR "Undefined resources for ${TARGET_ARCH} on APPLE.")
    endif()
elseif(UNIX)
    add_compile_definitions(LINUX)
    set(PLATFORM_SRC
            src/installer/PAMHelper.cpp
            src/installer/PAMHelper.h
            src/installer/ServiceInstaller.Linux.cpp
    )

    if(TARGET_ARCH STREQUAL "x64")
        set(PLATFORM_RES res/resources_linux_x64.qrc)
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(PLATFORM_RES res/resources_linux_arm64.qrc)
    else()
        message(FATAL_ERROR "Undefined resources for ${TARGET_ARCH} on LINUX.")
    endif()
endif()

include_directories(src src/ui src/ui/helpers src/ui/models ../deps/headers)
set(QML_FILES
        qml/MainWindow.qml
        qml/AboutWindow.qml
        qml/LogsWindow.qml
        qml/base/Form.qml
        qml/base/StepForm.qml
        qml/base/StepButtons.qml

        qml/forms/MainForm.qml
        qml/forms/SettingsForm.qml
        qml/forms/LoadingForm.qml

        qml/pairing/PairingPasswordForm.qml
        qml/pairing/PairingMethodForm.qml
        qml/pairing/PairingBTSelectForm.qml
        qml/pairing/PairingBTPairForm.qml
        qml/pairing/PairingQRForm.qml
)
set(UI_SRC_FILES
        src/ui/MainWindow.h
        src/ui/MainWindow.cpp
        src/ui/PairingForm.h
        src/ui/PairingForm.cpp
        src/ui/SettingsForm.cpp
        src/ui/SettingsForm.h
        src/ui/LogsWindow.cpp
        src/ui/LogsWindow.h
        src/ui/UpdaterWindow.cpp
        src/ui/UpdaterWindow.h

        src/ui/models/DevicesTableModel.cpp
        src/ui/models/DevicesTableModel.h
        src/ui/models/UserListModel.cpp
        src/ui/models/UserListModel.h
        src/ui/models/NetworkListModel.cpp
        src/ui/models/NetworkListModel.h
        src/ui/helpers/I18nWrapper.cpp
        src/ui/helpers/I18nWrapper.h
)
qt_add_executable(pcbu_desktop
        ../deps/headers/qrcodegen.cpp
        qml/resources.qrc
        res/resources.qrc
        ${PLATFORM_RES}

        src/main.cpp
        src/installer/ServiceInstaller.h
        src/utils/I18n.cpp
        src/utils/I18n.h
        src/utils/QRUtils.cpp
        src/utils/QRUtils.h
        src/utils/ResourceHelper.cpp
        src/utils/ResourceHelper.h
        ${PLATFORM_SRC}
)
qt_add_qml_module(pcbu_desktop
        URI PCBioUnlock
        VERSION 1.0
        QML_FILES
        ${QML_FILES}
        SOURCES
        ${UI_SRC_FILES}
        RESOURCE_PREFIX "/qt/qml/"
)
target_link_libraries(pcbu_desktop PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Quick
        Qt::QuickControls2
        pcbu_common
)

set_target_properties(pcbu_desktop PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
)
install(TARGETS pcbu_desktop
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
