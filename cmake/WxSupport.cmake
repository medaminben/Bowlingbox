# wxWidgets 3.2 built statically from source (FetchContent). Host needs GTK3
# development headers (libgtk-3-dev) and OpenGL headers (libgl1-mesa-dev).
include(FetchContent)
find_package(OpenGL REQUIRED)

set(GAMEBOX_WX_VERSION "3.2.8" CACHE STRING "wxWidgets release")

FetchContent_Declare(
    wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v${GAMEBOX_WX_VERSION}/wxWidgets-${GAMEBOX_WX_VERSION}.tar.bz2
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
foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
    set(${lib} builtin CACHE STRING "" FORCE)
endforeach()
find_package(ZLIB REQUIRED)
set(wxUSE_ZLIB sys CACHE STRING "" FORCE)

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
