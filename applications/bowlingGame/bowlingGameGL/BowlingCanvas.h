#ifndef BOWLINGGL_BOWLING_CANVAS_H
#define BOWLINGGL_BOWLING_CANVAS_H
#include "InputMapper.h"
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Render/Render.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <functional>
#include <memory>

/// GL canvas: owns the Lane, drives it at 60 Hz, draws it, maps input.
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
    void updateCamera(const Sandbox::Alley::Snapshot& s, float dt);
    void drawHud(const Sandbox::Alley::Snapshot& s, int w, int h, int topPx, int bottomPx);
    wxString statusFor(const Sandbox::Alley::Snapshot& s) const;

    std::unique_ptr<wxGLContext> _gl;
    std::unique_ptr<Sandbox::Render::Context> _ctx;
    std::unique_ptr<Sandbox::Render::SceneRenderer> _scene;
    std::unique_ptr<Sandbox::Render::Overlay> _overlay;
    Sandbox::Render::Camera _camera;
    Sandbox::Alley::Lane _lane;
    wxTimer _timer;
    float _cameraZ = -3.5f;
    wxPoint _lastMouse;
    bool _failed = false;
};
#endif  // BOWLINGGL_BOWLING_CANVAS_H
