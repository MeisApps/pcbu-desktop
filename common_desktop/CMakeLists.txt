cmake_minimum_required(VERSION 3.22)
project(pcbu_common_desktop)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_definitions(WINDOWS _WIN32_WINNT=0x0A00)
    set(PLATFORM_SRC
            src/installer/helpers/WinFirewallHelper.cpp
            src/installer/helpers/WinFirewallHelper.h
            src/installer/service/ServiceInstaller.Win.cpp
            src/updater/Updater.Win.cpp
    )
elseif(APPLE)
    add_compile_definitions(APPLE)
    set(PLATFORM_SRC
            src/installer/helpers/EnvHelper.cpp
            src/installer/helpers/EnvHelper.h
            src/installer/helpers/PAMHelper.cpp
            src/installer/helpers/PAMHelper.h
            src/installer/service/ServiceInstaller.Mac.cpp
            src/installer/service/ServiceInstaller.Unix.cpp
            src/updater/Updater.Mac.cpp
            src/updater/Updater.Unix.cpp
    )
elseif(UNIX)
    add_compile_definitions(LINUX)
    set(PLATFORM_SRC
            src/installer/helpers/EnvHelper.cpp
            src/installer/helpers/EnvHelper.h
            src/installer/helpers/PAMHelper.cpp
            src/installer/helpers/PAMHelper.h
            src/installer/service/ServiceInstaller.Linux.cpp
            src/installer/service/ServiceInstaller.Unix.cpp
            src/updater/Updater.Linux.cpp
            src/updater/Updater.Unix.cpp
    )
endif()

add_library(pcbu_common_desktop STATIC
        ../deps/qrcodegen.cpp
        src/installer/AppInstaller.cpp
        src/installer/AppInstaller.h
        src/installer/StartupChecks.cpp
        src/installer/StartupChecks.h
        src/installer/service/ServiceInstaller.h
        src/pairing/PairingSession.cpp
        src/pairing/PairingSession.h
        src/ssh/SshServerHelper.cpp
        src/ssh/SshServerHelper.h
        src/updater/Updater.cpp
        src/updater/Updater.h
        src/utils/QRUtils.cpp
        src/utils/QRUtils.h
        src/utils/ResourceHelper.cpp
        src/utils/ResourceHelper.h
        ${PLATFORM_SRC}
)
target_include_directories(pcbu_common_desktop PUBLIC
        src
        ../deps
)
target_link_libraries(pcbu_common_desktop PUBLIC
        pcbu_common
)

set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)
if(WIN32)
    embed_resources(pcbu_common_desktop
            ":/res/licenses.txt" "${RES_DIR}/licenses.txt"
            ":/res/natives/win_pulseunlock.dll" "${PCBU_NATIVES_DIR}/win_pulseunlock.dll"
    )
elseif(APPLE)
    embed_resources(pcbu_common_desktop
            ":/res/licenses.txt" "${RES_DIR}/licenses.txt"
            ":/res/natives/pam_pulseunlock.dylib" "${PCBU_NATIVES_DIR}/pam_pulseunlock.dylib"
            ":/res/natives/pcbu_auth" "${PCBU_NATIVES_DIR}/pcbu_auth"
            ":/res/natives/pcbu_ssh_askpass" "${PCBU_NATIVES_DIR}/pcbu_ssh_askpass"
            ":/res/mac/com.meisapps.PulseUnlock.agent.plist" "${RES_DIR}/mac/com.meisapps.PulseUnlock.agent.plist"
    )
elseif(UNIX)
    embed_resources(pcbu_common_desktop
            ":/res/licenses.txt" "${RES_DIR}/licenses.txt"
            ":/res/natives/pam_pulseunlock.so" "${PCBU_NATIVES_DIR}/pam_pulseunlock.so"
            ":/res/natives/pcbu_auth" "${PCBU_NATIVES_DIR}/pcbu_auth"
            ":/res/natives/pcbu_ssh_askpass" "${PCBU_NATIVES_DIR}/pcbu_ssh_askpass"
            ":/res/selinux/pcbu_auth_policy.pp" "${RES_DIR}/selinux/pcbu_auth_policy.pp"
    )
endif()
