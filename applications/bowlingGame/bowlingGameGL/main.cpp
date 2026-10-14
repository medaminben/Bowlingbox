#include "BowlingFrame.h"
#include <wx/wx.h>
#include <cstdlib>
#include <cstring>

class BowlingApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit())
            return false;
        auto* frame = new BowlingFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(BowlingApp);

int main(int argc, char* argv[]) {
#ifdef __linux__
    // wxGLCanvas uses GLX (X11). Native/wl GDK backends segfault in glXQueryVersion on Wayland sessions.
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        const char* gdk = std::getenv("GDK_BACKEND");
        if (gdk == nullptr || std::strcmp(gdk, "wayland") == 0)
            setenv("GDK_BACKEND", "x11", 1);
    }
#endif
    return wxEntry(argc, argv);
}
