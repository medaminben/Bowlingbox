# wxWidgets 3.2 with OpenGL. Every path provides the targets wx::base, wx::core
# and wx::gl. Lookup order:
#   1. an installed CMake package (wxWidgetsConfig.cmake, e.g. a wx built with CMake)
#   2. the system wx-config (e.g. Debian/Ubuntu libwxgtk3.2-dev)
#   3. FetchContent: download and build it statically from source. On Linux the
#      host then needs GTK3 development headers (libgtk-3-dev) and OpenGL headers
#      (libgl1-mesa-dev); on Windows (MinGW) nothing beyond the compiler.
# Set BOWLINGBOX_USE_SYSTEM_WX=OFF to skip 1 and 2 and always build from source.
include(FetchContent)
find_package(OpenGL REQUIRED)

option(BOWLINGBOX_USE_SYSTEM_WX "Use an installed wxWidgets if one is found" ON)
set(BOWLINGBOX_WX_VERSION "3.2.8" CACHE STRING "wxWidgets release (source build)")
set(BOWLINGBOX_WX_MIN_VERSION "3.2" CACHE STRING "Oldest installed wxWidgets that is accepted")

set(BOWLINGBOX_WX_SOURCE "")

if(BOWLINGBOX_USE_SYSTEM_WX)
    find_package(wxWidgets ${BOWLINGBOX_WX_MIN_VERSION} CONFIG QUIET COMPONENTS gl core base)
    if(wxWidgets_FOUND AND TARGET wx::gl AND TARGET wx::core AND TARGET wx::base)
        set(BOWLINGBOX_WX_SOURCE "installed CMake package ${wxWidgets_DIR}")
    else()
        find_package(wxWidgets ${BOWLINGBOX_WX_MIN_VERSION} MODULE QUIET COMPONENTS gl core base)
        if(wxWidgets_FOUND AND TARGET wxWidgets::wxWidgets)
            # wx-config reports the requested libraries as one set; expose it
            # under the per-library names the Bowlingbox targets link against.
            foreach(_wx_lib IN ITEMS base core gl)
                add_library(wx::${_wx_lib} INTERFACE IMPORTED GLOBAL)
                target_link_libraries(wx::${_wx_lib} INTERFACE wxWidgets::wxWidgets)
            endforeach()
            target_link_libraries(wx::gl INTERFACE OpenGL::GL)
            set(BOWLINGBOX_WX_SOURCE "system wx-config ${wxWidgets_CONFIG_EXECUTABLE}")
        endif()
    endif()
endif()

if(BOWLINGBOX_WX_SOURCE)
    if(NOT wxWidgets_VERSION_STRING)
        set(wxWidgets_VERSION_STRING ${wxWidgets_VERSION})
    endif()
    message(STATUS "wxWidgets ${wxWidgets_VERSION_STRING}: using ${BOWLINGBOX_WX_SOURCE}")
    return()
endif()

message(STATUS "wxWidgets: no installed version >= ${BOWLINGBOX_WX_MIN_VERSION} found, "
               "building ${BOWLINGBOX_WX_VERSION} from source")

FetchContent_Declare(
    wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v${BOWLINGBOX_WX_VERSION}/wxWidgets-${BOWLINGBOX_WX_VERSION}.tar.bz2
    SYSTEM
)

set(wxBUILD_SHARED      OFF CACHE BOOL   "" FORCE)
set(wxBUILD_SAMPLES     OFF CACHE STRING "" FORCE)
set(wxBUILD_TESTS       OFF CACHE STRING "" FORCE)
set(wxBUILD_DEMOS       OFF CACHE BOOL   "" FORCE)
set(wxBUILD_INSTALL     OFF CACHE BOOL   "" FORCE)
set(wxBUILD_MONOLITHIC  OFF CACHE BOOL   "" FORCE)
set(wxUSE_OPENGL        ON  CACHE BOOL   "" FORCE)
set(wxUSE_GLCANVAS_EGL  OFF CACHE BOOL   "" FORCE)
set(wxUSE_WXHTML_HELP   OFF CACHE BOOL   "" FORCE)
foreach(feature wxUSE_WEBVIEW wxUSE_MEDIACTRL wxUSE_STC wxUSE_RIBBON wxUSE_RICHTEXT
                wxUSE_PROPGRID wxUSE_AUI wxUSE_XRC wxUSE_HTML wxUSE_SECRETSTORE
                wxUSE_LIBSDL wxUSE_LIBTIFF wxUSE_LIBLZMA wxUSE_HELP)
    set(${feature} OFF CACHE BOOL "" FORCE)
endforeach()
if(WIN32)
    # BowlingGL uses no wx image, XML or regex classes. Leaving them out keeps
    # libjpeg, Expat and PCRE2 (whose licences need binary notices) out of the
    # Windows installer. wx turns wxUSE_XML off with Expat. Windows has no
    # system zlib, so wx builds its own.
    foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
        set(${lib} OFF CACHE STRING "" FORCE)
    endforeach()
    set(wxUSE_ZLIB builtin CACHE STRING "" FORCE)
else()
    foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
        set(${lib} builtin CACHE STRING "" FORCE)
    endforeach()
    find_package(ZLIB REQUIRED)
    set(wxUSE_ZLIB sys CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(wxWidgets)

# Static wxGTK pulls X11 helpers (e.g. utilsx11.cpp) into libwx_gtk3u_core. CMake
# often omits -lX11 on app link lines, which yields "DSO missing from command line".
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    foreach(_wx_target IN ITEMS wxcore wxgl)
        if(TARGET ${_wx_target})
            target_link_libraries(${_wx_target} INTERFACE X11::X11)
        endif()
    endforeach()
endif()
