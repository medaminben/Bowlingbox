/**
  @file main.cpp
  @details entry point to the app
*/
#include "ScoreSheetFrame.h"
#include <wx/wx.h>

class DisplayApp : public wxApp {
public:
    bool OnInit() override {
        auto* window = new BowlingDisplay::MainWindow();
        window->Show();
        return true;
    }
};

wxIMPLEMENT_APP(DisplayApp);
