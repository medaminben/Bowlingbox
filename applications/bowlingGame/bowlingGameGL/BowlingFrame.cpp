#include "BowlingFrame.h"
#include "BowlingCanvas.h"
#include "HudText.h"
#include <wx/config.h>
#include <wx/glcanvas.h>
#include <algorithm>
#include <cstddef>
#include <string>

namespace {
constexpr int kThemeMenuBase = wxID_HIGHEST + 1;  // one id per entry of Render::kThemes
constexpr int kMuteMenuId = kThemeMenuBase + 100;  // after the theme range
constexpr int kVolumeUpMenuId = kMuteMenuId + 1;
constexpr int kVolumeDownMenuId = kMuteMenuId + 2;
constexpr int kAmbienceMenuId = kMuteMenuId + 3;

int themeMenuId(Bowlingbox::Render::ThemeId id) {
    const auto& ids = Bowlingbox::Render::kThemes;
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == id)
            return kThemeMenuBase + static_cast<int>(i);
    return kThemeMenuBase;
}

// Saved per user (wxConfig: ~/.BowlingGL on Linux, the registry on Windows).
SoundSettings loadSoundSettings() {
    wxConfig config("BowlingGL");
    SoundSettings s;
    s.muted = config.ReadBool("Sound/Muted", s.muted);
    s.volume = std::clamp(static_cast<int>(config.ReadLong("Sound/Volume", s.volume)), 0, 100);
    s.ambience = config.ReadBool("Sound/Ambience", s.ambience);
    return s;
}

void saveSoundSettings(const SoundSettings& s) {
    wxConfig config("BowlingGL");
    config.Write("Sound/Muted", s.muted);
    config.Write("Sound/Volume", static_cast<long>(s.volume));
    config.Write("Sound/Ambience", s.ambience);
}
}  // namespace

BowlingFrame::BowlingFrame(Bowlingbox::Render::ThemeId initialTheme, bool muted)
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
    // The keys are handled by the canvas, so the labels name them without registering accelerators.
    auto* sound = new wxMenu;
    sound->AppendCheckItem(kMuteMenuId, "&Mute    (M)");
    sound->Append(kVolumeUpMenuId, "Volume &up    (+)");
    sound->Append(kVolumeDownMenuId, "Volume &down    (-)");
    sound->AppendSeparator();
    sound->AppendCheckItem(kAmbienceMenuId, "Background &ambience    (B)");
    auto* help = new wxMenu;
    help->Append(wxID_HELP, "&Controls");
    help->Append(wxID_ABOUT, "&About");
    auto* bar = new wxMenuBar;
    bar->Append(game, "&Game");
    bar->Append(view, "&View");
    bar->Append(sound, "&Sound");
    bar->Append(help, "&Help");
    SetMenuBar(bar);
    CreateStatusBar();

    Bind(wxEVT_MENU, &BowlingFrame::OnNewGame, this, wxID_NEW);
    Bind(wxEVT_MENU, &BowlingFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &BowlingFrame::OnControls, this, wxID_HELP);
    Bind(wxEVT_MENU, &BowlingFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &BowlingFrame::OnTheme, this, kThemeMenuBase,
         kThemeMenuBase + static_cast<int>(Bowlingbox::Render::kThemes.size()) - 1);
    Bind(wxEVT_MENU, &BowlingFrame::OnSound, this, kMuteMenuId, kAmbienceMenuId);

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
    _canvas->onSoundChanged = [this](const SoundSettings& s) {
        GetMenuBar()->Check(kMuteMenuId, s.muted);
        GetMenuBar()->Check(kAmbienceMenuId, s.ambience);
        GetMenuBar()->Enable(kVolumeUpMenuId, s.volume < 100);
        GetMenuBar()->Enable(kVolumeDownMenuId, s.volume > 0);
        if (_soundLoaded)
            saveSoundSettings(s);
    };
    SoundSettings settings = loadSoundSettings();
    if (muted)
        settings.muted = true;  // --mute: this session only, not saved until something changes
    _canvas->setSoundSettings(settings);  // before setTheme starts the ambient loop
    _soundLoaded = true;
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

void BowlingFrame::OnSound(wxCommandEvent& e) {
    SoundSettings s = _canvas->soundSettings();
    switch (e.GetId()) {
        case kMuteMenuId:       s.muted = e.IsChecked(); break;
        case kVolumeUpMenuId:   s.stepVolume(1); break;
        case kVolumeDownMenuId: s.stepVolume(-1); break;
        case kAmbienceMenuId:   s.ambience = e.IsChecked(); break;
        default:                return;
    }
    _canvas->setSoundSettings(s);
}

void BowlingFrame::OnControls(wxCommandEvent&) {
    wxMessageBox(wxString::FromUTF8(HudText::controlsHelp()), "Controls", wxOK | wxICON_INFORMATION, this);
}

void BowlingFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox("Bowling GL\nwxWidgets + OpenGL front-end for the Bowlingbox score calculator.\n"
                 "Physics: Bullet3.",
                 "About", wxOK | wxICON_INFORMATION, this);
}
