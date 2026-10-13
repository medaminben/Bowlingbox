#include "BowlingCanvas.h"
#include "SceneAdapter.h"
#include <algorithm>
#include <string>

using namespace Sandbox;
using Alley::Phase;

namespace {
constexpr bool kFollowCamera = true;
constexpr float kDt = 1.f / 60.f;
constexpr float kCameraHomeZ = -3.5f;
constexpr float kCameraHeight = 1.8f;
constexpr float kCameraLookAhead = 9.f;

const char* hintFor(Phase p) {
    switch (p) {
        case Phase::Aiming:   return "<- -> move   A D angle   hold Space to charge";
        case Phase::Charging: return "release to throw";
        case Phase::GameOver: return "N: new game   Esc: quit";
        default:              return "";
    }
}

/// Gamebox marks the most recent roll slot with -1 for a spare and -2 for a strike.
int lastRollValue(const std::vector<int>& rolls) {
    for (auto it = rolls.rbegin(); it != rolls.rend(); ++it)
        if (*it != -3 && *it != -4)
            return *it;
    return -3;
}

std::string resultFor(const Alley::Snapshot& s, const Alley::Lane& lane) {
    if (s.phase == Phase::GameOver)
        return "Game over - score " + std::to_string(lane.totalScore());
    if (s.phase != Phase::FrameDone || s.lastPinsDown < 0)
        return "";
    const int last = lastRollValue(lane.rolls());
    if (last == -2 || s.lastPinsDown == 10)
        return "STRIKE!";
    if (last == -1)
        return "SPARE!";
    if (s.lastPinsDown == 0)
        return "Gutter";
    return std::to_string(s.lastPinsDown) + " pins";
}
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
    const double scale = GetContentScaleFactor();
    const wxSize sz = GetClientSize();
    _ctx->resize(static_cast<int>(sz.x * scale), static_cast<int>(sz.y * scale));
    return true;
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
    if (_ctx) {
        const double scale = GetContentScaleFactor();
        const wxSize sz = GetClientSize();
        _ctx->resize(static_cast<int>(sz.x * scale), static_cast<int>(sz.y * scale));
    }
    Refresh(false);
}

void BowlingCanvas::OnTimer(wxTimerEvent&) {
    _lane.step(kDt);
    const auto s = _lane.snapshot();
    updateCamera(s, kDt);
    if (onStatus)
        onStatus(statusFor(s));
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
    Result<> r(std::monostate {});
    switch (command) {
        case Command::MoveLeft:    r = _lane.moveAim(-InputMapper::kKeyOffsetStep); break;
        case Command::MoveRight:   r = _lane.moveAim(InputMapper::kKeyOffsetStep); break;
        case Command::AngleLeft:   r = _lane.rotateAim(-InputMapper::kKeyAngleStep); break;
        case Command::AngleRight:  r = _lane.rotateAim(InputMapper::kKeyAngleStep); break;
        case Command::StartCharge: r = _lane.startCharge(); break;
        case Command::Release:     r = _lane.release(); break;
        case Command::NewGame:     r = _lane.newGame(); break;
        case Command::Quit:
            if (onQuit) onQuit();
            return;
        case Command::None:
            return;
    }
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
}

void BowlingCanvas::updateCamera(const Alley::Snapshot& s, float dt) {
    float target = kCameraHomeZ;
    if (kFollowCamera && (s.phase == Phase::Rolling || s.phase == Phase::Settling || s.phase == Phase::FrameDone))
        target = std::clamp(s.ball.position[2] - 3.5f, kCameraHomeZ, Alley::kLane.length - 5.5f);
    _cameraZ += (target - _cameraZ) * std::min(1.f, 6.f * dt);
    _camera.lookAt({0.f, kCameraHeight, _cameraZ}, {0.f, 0.f, _cameraZ + kCameraLookAhead});
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
    drawHud(s, w, h, topPx, bottomPx);
    _ctx->endFrame();
}

void BowlingCanvas::drawHud(const Alley::Snapshot& s, int w, int h, int topPx, int bottomPx) {
    const float margin = w * 0.02f;
    float cell = (w - 2 * margin) / 25.f;  // 21 cells + room for the total
    cell = std::min(cell, (topPx - 2 * margin) / 2.f);
    const Render::ScoreboardLayout layout {margin, margin, cell};
    _overlay->drawScoreboard(_lane.rolls(), _lane.frameScores(), _lane.totalScore(), layout);

    // Power bar (left of the bottom band).
    const float barY = h - bottomPx * 0.7f, barH = bottomPx * 0.4f, barW = w * 0.3f;
    _overlay->strokeRect({margin, barY, barW, barH}, 2.f, {0.f, 0.898f, 1.f, 1.f});
    const glm::vec4 powerFill = glm::mix(glm::vec4(1.f, 0.541f, 0.f, 1.f), glm::vec4(1.f, 0.780f, 0.f, 1.f), s.power);
    _overlay->drawRect({margin + 2.f, barY + 2.f}, {(barW - 4.f) * s.power, barH - 4.f}, powerFill);

    // Hint (centre) and result (right).
    const float textScale = std::max(1.f, bottomPx / 40.f);
    const std::string hint = hintFor(s.phase);
    const glm::vec2 hintSize = _overlay->measureText(hint, textScale);
    _overlay->drawText(hint, {(w - hintSize.x) / 2, barY + (barH - hintSize.y) / 2}, textScale, {0.85f, 0.85f, 0.85f});
    const std::string result = resultFor(s, _lane);
    const glm::vec2 resSize = _overlay->measureText(result, textScale * 1.5f);
    _overlay->drawText(result, {w - margin - resSize.x, barY + (barH - resSize.y) / 2}, textScale * 1.5f,
                       {1.f, 0.780f, 0.2f});
}

wxString BowlingCanvas::statusFor(const Alley::Snapshot& s) const {
    const char* phase = "";
    switch (s.phase) {
        case Phase::Aiming:    phase = "Aiming"; break;
        case Phase::Charging:  phase = "Charging"; break;
        case Phase::Rolling:   phase = "Rolling"; break;
        case Phase::Settling:  phase = "Settling"; break;
        case Phase::FrameDone: phase = "Frame done"; break;
        case Phase::GameOver:  phase = "Game over"; break;
    }
    return wxString::Format("%s   |   score %d   |   pins standing %d", phase, _lane.totalScore(),
                            static_cast<int>(std::count(s.pinStanding.begin(), s.pinStanding.end(), true)));
}
