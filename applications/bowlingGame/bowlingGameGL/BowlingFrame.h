#ifndef BOWLINGGL_BOWLING_FRAME_H
#define BOWLINGGL_BOWLING_FRAME_H
#include <wx/wx.h>

class BowlingCanvas;

class BowlingFrame : public wxFrame {
public:
    BowlingFrame();

private:
    void OnNewGame(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnControls(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);

    BowlingCanvas* _canvas = nullptr;
};
#endif  // BOWLINGGL_BOWLING_FRAME_H
