#include "BowlingCanvas.h"
#include "Hud.h"
#include "HudText.h"
#include "InputMapper.h"
#include "LaneCommands.h"
#include "SceneAdapter.h"

using namespace Bowlingbox;
using Alley::Phase;

namespace {
constexpr float kDt = 1.f / 60.f;
}  // namespace

BowlingCanvas::BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs)
    : wxGLCanvas(parent, attrs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
      _timer(this) {
    wxGLContextAttrs ca;
    ca.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    _gl = std::make_unique<wxGLContext>(this, nullptr, &ca);
    if (!_gl->IsOK()) {
        _gl.reset();
        _failed = true;
        CallAfter([this] {
            wxMessageBox("Could not create an OpenGL 3.3 core context.", "BowlingGL", wxICON_ERROR);
            if (onQuit) onQuit();
        });
    }

    Bind(wxEVT_PAINT, &BowlingCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &BowlingCanvas::OnSize, this);
    Bind(wxEVT_TIMER, &BowlingCanvas::OnTimer, this);
    Bind(wxEVT_KEY_DOWN, &BowlingCanvas::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &BowlingCanvas::OnKeyUp, this);
    for (auto evt : {wxEVT_LEFT_DOWN, wxEVT_LEFT_UP, wxEVT_RIGHT_DOWN, wxEVT_RIGHT_UP, wxEVT_MOTION})
        Bind(evt, &BowlingCanvas::OnMouse, this);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { SetFocus(); e.Skip(); });
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});

    _timer.Start(16);
}

BowlingCanvas::~BowlingCanvas() {
    _timer.Stop();
    // GL objects need a current context to be released.
    if (_gl && IsShownOnScreen())
        SetCurrent(*_gl);
    _overlay.reset();
    _scene.reset();
    _ctx.reset();
}

void BowlingCanvas::newGame() {
    apply(Command::NewGame);
}

bool BowlingCanvas::ensureRenderer() {
    if (_ctx)
        return true;
    auto created = Render::Context::create();
    if (created.isError()) {
        _failed = true;
        const wxString msg = wxString::FromUTF8(created.error());
        CallAfter([this, msg] {
            wxMessageBox("OpenGL initialisation failed:\n" + msg, "BowlingGL", wxICON_ERROR);
            if (onQuit) onQuit();
        });
        return false;
    }
    _ctx = created();
    _scene = std::make_unique<Render::SceneRenderer>(*_ctx, SceneAdapter::geometry(Alley::kLane));
    _overlay = std::make_unique<Render::Overlay>(*_ctx);
    resizeContext();
    return true;
}

void BowlingCanvas::resizeContext() {
    const double scale = GetContentScaleFactor();
    const wxSize sz = GetClientSize();
    _ctx->resize(static_cast<int>(sz.x * scale), static_cast<int>(sz.y * scale));
}

void BowlingCanvas::OnPaint(wxPaintEvent&) {
    wxPaintDC dc(this);
    if (_failed || !_gl || !IsShownOnScreen())
        return;
    SetCurrent(*_gl);
    if (!ensureRenderer())
        return;
    renderFrame();
    SwapBuffers();
}

void BowlingCanvas::OnSize(wxSizeEvent& e) {
    e.Skip();
    if (_ctx)
        resizeContext();
    Refresh(false);
}

void BowlingCanvas::OnTimer(wxTimerEvent&) {
    _lane.step(kDt);
    const auto s = _lane.snapshot();
    _follow.update(s, kDt);
    _follow.applyTo(_camera);
    if (onStatus)
        onStatus(wxString::FromUTF8(HudText::status(s, _lane.scoreSheet())));
    Refresh(false);
}

void BowlingCanvas::OnKeyDown(wxKeyEvent& e) {
    const Command c = InputMapper::keyDown(e.GetKeyCode(), e.ControlDown(), _lane.snapshot().phase);
    if (c == Command::None)
        e.Skip();
    else
        apply(c);
}

void BowlingCanvas::OnKeyUp(wxKeyEvent& e) {
    const Command c = InputMapper::keyUp(e.GetKeyCode(), _lane.snapshot().phase);
    if (c == Command::None)
        e.Skip();
    else
        apply(c);
}

void BowlingCanvas::OnMouse(wxMouseEvent& e) {
    const Phase phase = _lane.snapshot().phase;
    if (e.LeftDown() || e.RightDown()) {
        SetFocus();
        _lastMouse = e.GetPosition();
        apply(InputMapper::mouseDown(e.LeftDown(), phase));
    } else if (e.LeftUp() || e.RightUp()) {
        apply(InputMapper::mouseUp(e.LeftUp(), phase));
    } else if (e.Dragging() && e.RightIsDown()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        _lastMouse = e.GetPosition();
        if (dx != 0 && phase == Phase::Aiming)
            _lane.rotateAim(InputMapper::mouseDxToAngle(dx));
    } else if (e.Moving()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        _lastMouse = e.GetPosition();
        if (dx != 0 && phase == Phase::Aiming)
            _lane.moveAim(InputMapper::mouseDxToOffset(dx, GetClientSize().x));
    }
}

void BowlingCanvas::apply(Command command) {
    if (command == Command::Quit) {
        if (onQuit) onQuit();
        return;
    }
    auto r = LaneCommands::execute(command, _lane);
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
}

void BowlingCanvas::renderFrame() {
    const int w = _ctx->width(), h = _ctx->height();
    const int topPx = static_cast<int>(h * 0.18f);
    const int bottomPx = static_cast<int>(h * 0.10f);
    const int midPx = h - topPx - bottomPx;
    const auto s = _lane.snapshot();

    _ctx->beginFrame({0.02f, 0.03f, 0.08f});  // kClearColor — splash backdrop
    _camera.setViewport(w, midPx);
    _scene->draw(_camera, SceneAdapter::toScene(s), bottomPx, midPx);
    _overlay->begin(w, h);
    drawHud(*_overlay, s, _lane.scoreSheet(), {w, h, topPx, bottomPx});
    _ctx->endFrame();
}
