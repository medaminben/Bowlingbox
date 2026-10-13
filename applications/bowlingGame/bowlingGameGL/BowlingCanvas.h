#ifndef BOWLINGGL_BOWLING_CANVAS_H
#define BOWLINGGL_BOWLING_CANVAS_H
#include "Command.h"
#include "FollowCamera.h"
#include <Gamebox/Alley/Alley.h>
#include <Gamebox/Render/Render.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <functional>
#include <memory>

/// GL canvas: owns the Lane and the GL resources, runs the 60 Hz loop and
/// forwards wx events. Rules, input mapping, camera and HUD live elsewhere.
class BowlingCanvas : public wxGLCanvas {
public:
    BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs);
    ~BowlingCanvas() override;

    void newGame();
    std::function<void(const wxString&)> onStatus;  // status bar text
    std::function<void()> onQuit;                   // close the frame

private:
    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnTimer(wxTimerEvent&);
    void OnKeyDown(wxKeyEvent&);
    void OnKeyUp(wxKeyEvent&);
    void OnMouse(wxMouseEvent&);

    bool ensureRenderer();
    void apply(Command command);
    void renderFrame();
    void resizeContext();

    std::unique_ptr<wxGLContext> _gl;
    std::unique_ptr<Gamebox::Render::Context> _ctx;
    std::unique_ptr<Gamebox::Render::SceneRenderer> _scene;
    std::unique_ptr<Gamebox::Render::Overlay> _overlay;
    Gamebox::Render::Camera _camera;
    FollowCamera _follow;
    Gamebox::Alley::Lane _lane;
    wxTimer _timer;
    wxPoint _lastMouse;
    bool _failed = false;
};
#endif  // BOWLINGGL_BOWLING_CANVAS_H
