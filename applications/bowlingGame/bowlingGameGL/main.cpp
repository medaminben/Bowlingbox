#include "BowlingFrame.h"
#include "HudText.h"
#include <Bowlingbox/Render/Environment.h>
#include <wx/cmdline.h>
#include <wx/wx.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

class BowlingApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit())  // parses the command line
            return false;
        auto* frame = new BowlingFrame(_theme);
        frame->Show(true);
        return true;
    }

    void OnInitCmdLine(wxCmdLineParser& parser) override {
        wxApp::OnInitCmdLine(parser);
        parser.AddLongOption("theme", "scenery: neon, classic or outdoor", wxCMD_LINE_VAL_STRING);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override {
        if (!wxApp::OnCmdLineParsed(parser))
            return false;
        wxString value;
        if (parser.Found("theme", &value)) {
            const std::string name(value.ToUTF8().data());
            if (const auto id = Bowlingbox::Render::themeFromName(name))
                _theme = *id;
            else
                std::fprintf(stderr, "%s\n", HudText::unknownThemeMessage(name).c_str());
        }
        return true;
    }

private:
    Bowlingbox::Render::ThemeId _theme = Bowlingbox::Render::ThemeId::NeonHall;
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
