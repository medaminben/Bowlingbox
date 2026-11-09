# wxWidgets 3.2 with OpenGL through dev_dependency(). Every path provides the
# targets wx::base, wx::core and wx::gl. Lookup order:
#   1. an installed CMake package (wxWidgetsConfig.cmake, e.g. a wx built with CMake)
#   2. the system wx-config (e.g. Debian/Ubuntu libwxgtk3.2-dev, through FindwxWidgets)
#   3. release 3.2.8 fetched and built statically from source. On Linux the
#      host then needs GTK3 development headers (libgtk-3-dev) and OpenGL headers
#      (libgl1-mesa-dev); on Windows (MinGW) nothing beyond the compiler.
# Set BOWLINGBOX_USE_SYSTEM_WX=OFF to skip 1 and 2 and always build from source.
#
# Only BowlingGL and BowlingDisplay need wxWidgets. With BUILD_WX_UI=AUTO (the
# default) it is OPTIONAL: without it (or without OpenGL)
# BOWLINGBOX_WX_AVAILABLE stays FALSE and the wx applications are left out with
# a warning; the libraries and BowlingConsole still build. With BUILD_WX_UI=ON
# it is REQUIRED and its absence stops the configure.
option(BOWLINGBOX_USE_SYSTEM_WX "Use an installed wxWidgets if one is found" ON)

if(BUILD_WX_UI STREQUAL "AUTO")
    set(_bowlingbox_wx_mode OPTIONAL)
    set(_bowlingbox_wx_level WARNING)
else()
    set(_bowlingbox_wx_mode REQUIRED)
    set(_bowlingbox_wx_level FATAL_ERROR)
endif()

find_package(OpenGL QUIET)
if(NOT OpenGL_FOUND)
    message(${_bowlingbox_wx_level} "OpenGL not found: cannot build the wxWidgets applications. "
            "Install with redev/bootstrap/bootstrap.sh (Debian/Ubuntu: apt install libgl1-mesa-dev)")
    return()
endif()

# Only read when wxWidgets is fetched.
set(_bowlingbox_wx_cache
    wxBUILD_SHARED=OFF wxBUILD_SAMPLES=OFF wxBUILD_TESTS=OFF wxBUILD_DEMOS=OFF
    wxBUILD_INSTALL=OFF wxBUILD_MONOLITHIC=OFF
    wxUSE_OPENGL=ON wxUSE_GLCANVAS_EGL=OFF wxUSE_WXHTML_HELP=OFF)
foreach(feature wxUSE_WEBVIEW wxUSE_MEDIACTRL wxUSE_STC wxUSE_RIBBON wxUSE_RICHTEXT
                wxUSE_PROPGRID wxUSE_AUI wxUSE_XRC wxUSE_HTML wxUSE_SECRETSTORE
                wxUSE_LIBSDL wxUSE_LIBTIFF wxUSE_LIBLZMA wxUSE_HELP)
    list(APPEND _bowlingbox_wx_cache ${feature}=OFF)
endforeach()
if(WIN32)
    # BowlingGL uses no wx JPEG, XML or regex classes. Leaving them out keeps
    # libjpeg, Expat and PCRE2 (whose licences need binary notices) out of the
    # Windows installer. wx turns wxUSE_XML off with Expat. wxMSW cannot drop
    # PNG (its Tango art provider and wxUSE_SVG require it); libpng needs no
    # binary notice. Windows has no system zlib, so wx builds its own.
    list(APPEND _bowlingbox_wx_cache
        wxUSE_LIBJPEG=OFF wxUSE_EXPAT=OFF wxUSE_REGEX=OFF
        wxUSE_LIBPNG=builtin wxUSE_ZLIB=builtin)
else()
    list(APPEND _bowlingbox_wx_cache
        wxUSE_LIBPNG=builtin wxUSE_LIBJPEG=builtin wxUSE_EXPAT=builtin wxUSE_REGEX=builtin
        wxUSE_ZLIB=sys)
endif()

# Installed: the CMake package before wx-config, as before buildkit.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
if(NOT BOWLINGBOX_USE_SYSTEM_WX)
    set(CMAKE_DISABLE_FIND_PACKAGE_wxWidgets TRUE)
endif()

dev_dependency(wxWidgets VERSION 3.2 ${_bowlingbox_wx_mode}
    COMPONENTS gl core base
    FETCH_URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.8/wxWidgets-3.2.8.tar.bz2
    FETCH_CACHE ${_bowlingbox_wx_cache}
    INSTALL_HINT "Debian/Ubuntu: apt install libwxgtk3.2-dev; MSYS2: pacman -S mingw-w64-ucrt-x86_64-wxwidgets3.2-msw")

unset(CMAKE_FIND_PACKAGE_PREFER_CONFIG)
unset(CMAKE_DISABLE_FIND_PACKAGE_wxWidgets)
unset(_bowlingbox_wx_cache)

if(NOT wxWidgets_AVAILABLE)
    return()
endif()

if(TARGET wxcore)
    # Fetched. Static wxGTK pulls X11 helpers (e.g. utilsx11.cpp) into
    # libwx_gtk3u_core. CMake often omits -lX11 on app link lines, which yields
    # "DSO missing from command line".
    if(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
        foreach(_wx_target IN ITEMS wxcore wxgl)
            if(TARGET ${_wx_target})
                target_link_libraries(${_wx_target} INTERFACE X11::X11)
            endif()
        endforeach()
    endif()
elseif(NOT TARGET wx::core AND TARGET wxWidgets::wxWidgets)
    # wx-config reports the requested libraries as one set; expose it under
    # the per-library names the Bowlingbox targets link against.
    foreach(_wx_lib IN ITEMS base core gl)
        add_library(wx::${_wx_lib} INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wx::${_wx_lib} INTERFACE wxWidgets::wxWidgets)
    endforeach()
    target_link_libraries(wx::gl INTERFACE OpenGL::GL)
endif()

if(TARGET wx::base AND TARGET wx::core AND TARGET wx::gl)
    set(BOWLINGBOX_WX_AVAILABLE TRUE)
else()
    message(${_bowlingbox_wx_level} "wxWidgets found, but without the targets wx::base, "
            "wx::core and wx::gl: cannot build the wxWidgets applications")
endif()
