#ifndef BOWLINGGL_BOWLING_FRAME_H
#define BOWLINGGL_BOWLING_FRAME_H
#include <Bowlingbox/Render/Environment.h>
#include <wx/wx.h>

class BowlingCanvas;

class BowlingFrame : public wxFrame {
public:
    explicit BowlingFrame(Bowlingbox::Render::ThemeId initialTheme = Bowlingbox::Render::ThemeId::NeonHall,
                          bool muted = false);

private:
    void OnNewGame(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnControls(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnTheme(wxCommandEvent&);
    void OnSound(wxCommandEvent&);  // every Sound menu item

    BowlingCanvas* _canvas = nullptr;
    bool _soundLoaded = false;  // the first sync only fills the menu
};
#endif  // BOWLINGGL_BOWLING_FRAME_H
