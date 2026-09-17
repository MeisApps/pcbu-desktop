cmake_minimum_required(VERSION 3.22)

project(pam_pulseunlock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    add_compile_definitions(WINDOWS)
elseif(APPLE)
    add_compile_definitions(APPLE)
elseif(UNIX)
    add_compile_definitions(LINUX)
endif()

add_library(pam_pulseunlock SHARED
        src/pam_auth.cpp
        src/pam_account.cpp
        src/pam_password.cpp
        src/pam_session.cpp
)
target_include_directories(pam_pulseunlock PRIVATE common-cpp/src)
target_link_libraries(pam_pulseunlock PRIVATE pam pcbu_common)
add_custom_command(TARGET pam_pulseunlock
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pam_pulseunlock> ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH})
