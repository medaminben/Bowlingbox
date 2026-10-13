#include "BowlingFrame.h"
#include <wx/wx.h>

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

wxIMPLEMENT_APP(BowlingApp);
