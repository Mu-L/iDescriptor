cmake_minimum_required(VERSION 3.16)
project(cpp_bridge CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(IDESCRIPTOR_APPIMAGE_BUILD "Build iDescriptor for AppImage" OFF)

# ---------------------------------------------------------------------------
# Package manager hints
# ---------------------------------------------------------------------------
if(WIN32)
    set(PKG_CONFIG_EXECUTABLE "C:/msys64/mingw64/bin/pkg-config.exe" CACHE FILEPATH "" FORCE)
    set(ENV{PKG_CONFIG_PATH} "C:/msys64/mingw64/lib/pkgconfig;$ENV{PKG_CONFIG_PATH}")
elseif(APPLE)
    set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
else()
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig")
    # no need for x11 deps on Linux/Unix
    # this var is respected by uxplay
    set(NO_X11_DEPS ON)
endif()

find_package(PkgConfig REQUIRED)

# ---------------------------------------------------------------------------
# Qt6
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# FFmpeg
# ---------------------------------------------------------------------------
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC  REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL   REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE  REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(HEIF     REQUIRED IMPORTED_TARGET libheif)

# ---------------------------------------------------------------------------
# GStreamer
# ---------------------------------------------------------------------------
pkg_check_modules(GST       REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GST_APP   REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GST_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)
pkg_check_modules(GST_AUDIO REQUIRED IMPORTED_TARGET gstreamer-audio-1.0)

# ---------------------------------------------------------------------------
# glib / gobject / openssl / plist
# ---------------------------------------------------------------------------
pkg_check_modules(GLIB    REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GOBJECT REQUIRED IMPORTED_TARGET gobject-2.0)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules(PLIST   REQUIRED IMPORTED_TARGET libplist-2.0)

# ---------------------------------------------------------------------------
# Platform-specific service discovery
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# uxplay
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/uxplay uxplay_build)
if(IDESCRIPTOR_APPIMAGE_BUILD)
    target_compile_definitions(uxplay PRIVATE IDESCRIPTOR_APPIMAGE_BUILD=1)
endif()

# ---------------------------------------------------------------------------
# Sources — per-platform
# ---------------------------------------------------------------------------
set(BRIDGE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systemappearance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systemappearance.h
)

#if(APPLE)
#    list(APPEND BRIDGE_SOURCES
#        ${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/macos.mm
#        ${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/macos.h
#    )
#endif()

# ---------------------------------------------------------------------------
# Static library
# ---------------------------------------------------------------------------
add_library(cpp_bridge STATIC ${BRIDGE_SOURCES})
set_target_properties(cpp_bridge PROPERTIES AUTOMOC ON)

target_link_libraries(cpp_bridge PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::HEIF
    PkgConfig::GST
    PkgConfig::GST_APP
    PkgConfig::GST_VIDEO
    PkgConfig::GST_AUDIO
    PkgConfig::GLIB
    PkgConfig::GOBJECT
    PkgConfig::OPENSSL
    PkgConfig::PLIST
    uxplay
    renderers
    airplay
    llhttp
    playfair
)

if(UNIX AND NOT APPLE)
    target_link_libraries(cpp_bridge PUBLIC Qt6::DBus)
endif()
