cmake_minimum_required(VERSION 3.22)

project(mac_pulseunlock)
set(CMAKE_CXX_STANDARD 23)
enable_language(OBJCXX)

include(MacAppIcon)

add_compile_definitions(APPLE)

add_executable(PulseUnlockAgent MACOSX_BUNDLE
        src/main.mm
        src/AgentController.h
        src/AgentController.mm
        src/auth/AuthProcess.h
        src/auth/AuthProcess.cpp
        src/input/ActivityMonitor.h
        src/input/ActivityMonitor.mm
        src/input/PasswordInjector.h
        src/input/PasswordInjector.mm
        src/loginwindow/AccessibilityPermission.h
        src/loginwindow/AccessibilityPermission.mm
        src/loginwindow/LockStateMonitor.h
        src/loginwindow/LockStateMonitor.mm
        src/loginwindow/LoginWindowInspector.h
        src/loginwindow/LoginWindowInspector.mm
        src/ui/SkyLightSpace.h
        src/ui/SkyLightSpace.mm
        src/ui/StatusOverlay.h
        src/ui/StatusOverlay.mm
)
target_include_directories(PulseUnlockAgent PRIVATE src)
target_compile_options(PulseUnlockAgent PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(PulseUnlockAgent PRIVATE
        pcbu_common
        "-framework AppKit"
        "-framework ApplicationServices"
        "-framework Carbon"
        "-framework IOKit"
        "-framework QuartzCore"
        "-framework Security"
        "-framework Symbols"
)
set_target_properties(PulseUnlockAgent PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME PulseUnlockAgent
        MACOSX_BUNDLE_GUI_IDENTIFIER com.meisapps.PulseUnlock.Agent
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist
)

add_mac_app_icon(PulseUnlockAgent)
if(DEFINED ENV{MAC_SIGN_IDENTITY})
    set(MAC_SIGN_IDENTITY_DEFAULT "$ENV{MAC_SIGN_IDENTITY}")
else()
    set(MAC_SIGN_IDENTITY_DEFAULT "-")
endif()
set(MAC_SIGN_IDENTITY "${MAC_SIGN_IDENTITY_DEFAULT}" CACHE STRING "Code signing identity for the lock screen agent")
add_custom_command(TARGET PulseUnlockAgent POST_BUILD
        COMMAND codesign --force --sign "${MAC_SIGN_IDENTITY}" $<TARGET_BUNDLE_DIR:PulseUnlockAgent>
        COMMENT "Signing lock screen agent..."
)
