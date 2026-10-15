#ifndef BOWLINGGL_BOWLING_CANVAS_H
#define BOWLINGGL_BOWLING_CANVAS_H
#include "Command.h"
#include "FixedStep.h"
#include "FollowCamera.h"
#include <Bowlingbox/Alley/Alley.h>
#include <Bowlingbox/Render/Environment.h>
#include <Bowlingbox/Render/Render.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <chrono>
#include <functional>
#include <memory>

/// GL canvas: owns the Lane and the GL resources, runs the timer (FixedStep turns
/// its ticks into 1/60 s physics steps) and forwards wx events. Rules, input
/// mapping, camera and HUD live elsewhere.
class BowlingCanvas : public wxGLCanvas {
public:
    BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs);
    ~BowlingCanvas() override;

    void newGame();
    /// Stores the theme; applies it now if the renderer exists, else when it is created.
    void setTheme(Bowlingbox::Render::ThemeId id);
    Bowlingbox::Render::ThemeId theme() const { return _theme; }
    std::function<void(Bowlingbox::Render::ThemeId)> onThemeChanged;  // menu sync
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
    std::unique_ptr<Bowlingbox::Render::Context> _ctx;
    std::unique_ptr<Bowlingbox::Render::SceneRenderer> _scene;
    std::unique_ptr<Bowlingbox::Render::Overlay> _overlay;
    Bowlingbox::Render::Camera _camera;
    FollowCamera _follow;
    Bowlingbox::Alley::Lane _lane;
    wxTimer _timer;
    wxPoint _lastMouse;
    FixedStep _clock;
    std::chrono::steady_clock::time_point _lastTick = std::chrono::steady_clock::now();
    wxString _status;  // last text sent to onStatus
    bool _failed = false;
    Bowlingbox::Render::ThemeId _theme = Bowlingbox::Render::ThemeId::NeonHall;
    bool _themeKeyHeld = false;  // auto-repeat would otherwise flash through themes
};
#endif  // BOWLINGGL_BOWLING_CANVAS_H
