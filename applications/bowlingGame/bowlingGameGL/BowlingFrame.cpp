#include "BowlingFrame.h"
#include "BowlingCanvas.h"
#include "HudText.h"
#include <wx/glcanvas.h>
#include <cstddef>
#include <string>

namespace {
constexpr int kThemeMenuBase = wxID_HIGHEST + 1;  // one id per entry of Render::kThemes

int themeMenuId(Bowlingbox::Render::ThemeId id) {
    const auto& ids = Bowlingbox::Render::kThemes;
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == id)
            return kThemeMenuBase + static_cast<int>(i);
    return kThemeMenuBase;
}
}  // namespace

BowlingFrame::BowlingFrame(Bowlingbox::Render::ThemeId initialTheme)
    : wxFrame(nullptr, wxID_ANY, "Bowling GL", wxDefaultPosition, wxSize(1280, 800)) {
    auto* game = new wxMenu;
    game->Append(wxID_NEW, "&New Game\tCtrl+N");
    game->AppendSeparator();
    game->Append(wxID_EXIT, "&Quit\tCtrl+Q");
    auto* themes = new wxMenu;
    for (std::size_t i = 0; i < Bowlingbox::Render::kThemes.size(); ++i) {
        const auto label = Bowlingbox::Render::theme(Bowlingbox::Render::kThemes[i]).label;
        themes->AppendRadioItem(kThemeMenuBase + static_cast<int>(i), wxString::FromUTF8(std::string(label)));
    }
    auto* view = new wxMenu;
    view->AppendSubMenu(themes, "&Theme");
    auto* help = new wxMenu;
    help->Append(wxID_HELP, "&Controls");
    help->Append(wxID_ABOUT, "&About");
    auto* bar = new wxMenuBar;
    bar->Append(game, "&Game");
    bar->Append(view, "&View");
    bar->Append(help, "&Help");
    SetMenuBar(bar);
    CreateStatusBar();

    Bind(wxEVT_MENU, &BowlingFrame::OnNewGame, this, wxID_NEW);
    Bind(wxEVT_MENU, &BowlingFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &BowlingFrame::OnControls, this, wxID_HELP);
    Bind(wxEVT_MENU, &BowlingFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &BowlingFrame::OnTheme, this, kThemeMenuBase,
         kThemeMenuBase + static_cast<int>(Bowlingbox::Render::kThemes.size()) - 1);

    wxGLAttributes attrs;
    attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();
    if (!wxGLCanvas::IsDisplaySupported(attrs)) {
        attrs.Reset();
        attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(16).EndList();
    }
    _canvas = new BowlingCanvas(this, attrs);
    _canvas->onStatus = [this](const wxString& text) { SetStatusText(text); };
    _canvas->onQuit = [this] { Close(true); };
    _canvas->onThemeChanged = [this](Bowlingbox::Render::ThemeId id) {
        GetMenuBar()->Check(themeMenuId(id), true);
    };
    _canvas->setTheme(initialTheme);

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

void BowlingFrame::OnTheme(wxCommandEvent& e) {
    const auto index = static_cast<std::size_t>(e.GetId() - kThemeMenuBase);
    _canvas->setTheme(Bowlingbox::Render::kThemes[index]);
}

void BowlingFrame::OnControls(wxCommandEvent&) {
    wxMessageBox(wxString::FromUTF8(HudText::controlsHelp()), "Controls", wxOK | wxICON_INFORMATION, this);
}

void BowlingFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox("Bowling GL\nwxWidgets + OpenGL front-end for the Bowlingbox score calculator.\n"
                 "Physics: Bullet3.",
                 "About", wxOK | wxICON_INFORMATION, this);
}
