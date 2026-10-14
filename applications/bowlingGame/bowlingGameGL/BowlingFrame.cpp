#include "BowlingFrame.h"
#include "BowlingCanvas.h"
#include <wx/glcanvas.h>

BowlingFrame::BowlingFrame()
    : wxFrame(nullptr, wxID_ANY, "Bowling GL", wxDefaultPosition, wxSize(1280, 800)) {
    auto* game = new wxMenu;
    game->Append(wxID_NEW, "&New Game\tCtrl+N");
    game->AppendSeparator();
    game->Append(wxID_EXIT, "&Quit\tCtrl+Q");
    auto* help = new wxMenu;
    help->Append(wxID_HELP, "&Controls");
    help->Append(wxID_ABOUT, "&About");
    auto* bar = new wxMenuBar;
    bar->Append(game, "&Game");
    bar->Append(help, "&Help");
    SetMenuBar(bar);
    CreateStatusBar();

    Bind(wxEVT_MENU, &BowlingFrame::OnNewGame, this, wxID_NEW);
    Bind(wxEVT_MENU, &BowlingFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &BowlingFrame::OnControls, this, wxID_HELP);
    Bind(wxEVT_MENU, &BowlingFrame::OnAbout, this, wxID_ABOUT);

    wxGLAttributes attrs;
    attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();
    if (!wxGLCanvas::IsDisplaySupported(attrs)) {
        attrs.Reset();
        attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(16).EndList();
    }
    _canvas = new BowlingCanvas(this, attrs);
    _canvas->onStatus = [this](const wxString& text) { SetStatusText(text); };
    _canvas->onQuit = [this] { Close(true); };

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(_canvas, 1, wxEXPAND);
    SetSizer(sizer);
    _canvas->SetFocus();
}

void BowlingFrame::OnNewGame(wxCommandEvent&) {
    _canvas->newGame();
}

void BowlingFrame::OnQuit(wxCommandEvent&) {
    Close(true);
}

void BowlingFrame::OnControls(wxCommandEvent&) {
    wxMessageBox("Left / Right or mouse move: position the ball\n"
                 "A / D or right-drag: aim angle\n"
                 "Hold Space or left button: charge, release to throw\n"
                 "N: new game    Esc / Ctrl+Q: quit",
                 "Controls", wxOK | wxICON_INFORMATION, this);
}

void BowlingFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox("Bowling GL\nwxWidgets + OpenGL front-end for the Bowlingbox score calculator.\n"
                 "Physics: Bullet3.",
                 "About", wxOK | wxICON_INFORMATION, this);
}
