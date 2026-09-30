# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(lupka
    VERSION 0.1.0
    DESCRIPTION "Zoom, draw and snip on the screen, the ZoomIt way"
    HOMEPAGE_URL "https://github.com/komsky/lupka"
    LANGUAGES CXX)

# The product name lives here and nowhere else. Source code only uses the
# APP_* macros from config.h.
set(APP_NAME "Lupka")
set(APP_BIN "lupka")
set(APP_ID "io.github.komsky.Lupka")
set(APP_SUMMARY ${PROJECT_DESCRIPTION})
set(APP_HOMEPAGE ${PROJECT_HOMEPAGE_URL})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(GNUInstallDirs)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-keysyms xcb-xtest)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)
configure_file(data/app.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop @ONLY)
configure_file(data/app.metainfo.xml.in ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.metainfo.xml @ONLY)

add_library(appcore STATIC
    src/actions.cpp
    src/app.cpp
    src/breaktimer.cpp
    src/dbusservice.cpp
    src/demotype/demotype.cpp
    src/demotype/script.cpp
    src/demotype/typist.cpp
    src/imageoutput.cpp
    src/livezoom.cpp
    src/notify.cpp
    src/platform.cpp
    src/recorder.cpp
    src/settings.cpp
    src/x11util.cpp
    src/capture/backends.h
    src/capture/screencapture.cpp
    src/capture/x11capture.cpp
    src/capture/kwincapture.cpp
    src/capture/portalcapture.cpp
    src/capture/grimcapture.cpp
    src/capture/gstutil.cpp
    src/capture/pipewireframe.cpp
    src/capture/screencast.cpp
    src/capture/screencastcapture.cpp
    src/hotkeys/hotkeys.cpp
    src/hotkeys/keynames.cpp
    src/hotkeys/gnomehotkeys.cpp
    src/hotkeys/kdehotkeys.cpp
    src/hotkeys/x11hotkeys.cpp
    src/hotkeys/portalhotkeys.cpp
    src/overlay/annotations.cpp
    src/overlay/overlaywindow.cpp
    src/overlay/session.cpp
    src/overlay/zoommath.cpp
    src/ui/settingsdialog.cpp
    src/ui/tray.cpp
    data/resources.qrc
)
target_include_directories(appcore PUBLIC src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(appcore PUBLIC
    Qt6::Widgets Qt6::DBus Qt6::Concurrent
    PkgConfig::XCB PkgConfig::GIO PkgConfig::GST)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(appcore PRIVATE -Wall -Wextra)
endif()

add_executable(${APP_BIN} src/main.cpp)
target_link_libraries(${APP_BIN} PRIVATE appcore)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS ${APP_BIN} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/icons/app.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME ${APP_ID}.svg)
foreach(size 32 48 64 128 256)
    install(FILES data/icons/app-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME ${APP_ID}.png)
endforeach()
install(FILES packaging/copyright DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${APP_BIN})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${APP_BIN})

# Debian package: cpack -G DEB
set(CPACK_PACKAGE_NAME ${APP_BIN})
set(CPACK_PACKAGE_VENDOR "Komsky")
set(CPACK_PACKAGE_CONTACT "Komsky <komsky@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${APP_SUMMARY})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${PROJECT_HOMEPAGE_URL})
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-wayland, gstreamer1.0-pipewire, gstreamer1.0-plugins-good")
# x264 for MP4 recordings, ximagesrc for recording under X11, AAC for sound.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
    "gstreamer1.0-plugins-ugly, gstreamer1.0-x, gstreamer1.0-libav, xdg-desktop-portal, wl-clipboard")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
"Press a hotkey to zoom into the screen and pan with the mouse, click to
freeze and draw arrows, boxes and text, snip a region to the clipboard,
record the screen, or start a break timer. Works on GNOME and KDE Plasma,
under Wayland and X11.")
include(CPack)
