#include "BowlingCanvas.h"
#include "Hud.h"
#include "HudText.h"
#include "InputMapper.h"
#include "LaneCommands.h"
#include "SceneAdapter.h"
#include <cstdio>

using namespace Bowlingbox;
using Alley::Phase;

namespace {
std::unique_ptr<Audio::IAudioOutput> createAudio() {
    auto device = Audio::createDeviceOutput();
    if (device.isValue())
        return std::move(device).value();
    std::fprintf(stderr, "sound disabled: %s\n", device.error().c_str());
    return Audio::makeSilentOutput();
}
}  // namespace

BowlingCanvas::BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs)
    : wxGLCanvas(parent, attrs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
      _timer(this),
      _audio(createAudio()),
      _sound(*_audio) {
    _audio->setVolume(_soundSettings.gain());
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
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { _keys.clear(); e.Skip(); });

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
    apply({Command::Type::NewGame});
}

void BowlingCanvas::setTheme(Render::ThemeId id) {
    _sound.setTheme(id, _themeChosen);
    _themeChosen = true;
    _theme = id;
    if (_scene)
        _scene->setTheme(id);  // CPU only, no GL context needed
    if (onThemeChanged)
        onThemeChanged(id);
    Refresh(false);
}

void BowlingCanvas::setSoundSettings(const SoundSettings& settings) {
    _soundSettings = settings;
    _audio->setVolume(settings.gain());
    _audio->setMuted(settings.muted);
    _sound.setAmbienceEnabled(settings.ambience);
    if (onSoundChanged)
        onSoundChanged(settings);
}

bool BowlingCanvas::ensureRenderer() {
    if (_ctx)
        return true;
    auto created = Render::Context::create();
    if (created.isError()) {
        std::fprintf(stderr, "OpenGL initialisation failed: %s\n", created.error().c_str());
        _failed = true;
        const wxString msg = wxString::FromUTF8(created.error());
        CallAfter([this, msg] {
            wxMessageBox("OpenGL initialisation failed:\n" + msg, "BowlingGL", wxICON_ERROR);
            if (onQuit) onQuit();
        });
        return false;
    }
    _ctx = std::move(created).value();
    _scene = std::make_unique<Render::SceneRenderer>(*_ctx, SceneAdapter::geometry(Alley::kLane), _theme);
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
    const auto now = std::chrono::steady_clock::now();
    const int steps = _clock.advance(std::chrono::duration<float>(now - _lastTick).count());
    _lastTick = now;
    for (int i = 0; i < steps; ++i)
        _lane.step(FixedStep::kDt);
    const auto s = _lane.snapshot();
    const auto impacts = _lane.drainImpacts();
    _sound.update(s, _lane.scoreSheet(), impacts, static_cast<float>(steps) * FixedStep::kDt);
    _follow.update(s, static_cast<float>(steps) * FixedStep::kDt);
    _follow.applyTo(_camera);
    const wxString status = wxString::FromUTF8(HudText::status(s, _lane.scoreSheet()) + "   |   " +
                                               HudText::soundStatus(_soundSettings));
    if (onStatus && status != _status) {
        _status = status;
        onStatus(status);
    }
    Refresh(false);
}

void BowlingCanvas::OnKeyDown(wxKeyEvent& e) {
    const Command c = InputMapper::keyDown(e.GetKeyCode(), e.ControlDown(), _lane.snapshot().phase);
    if (c.type == Command::Type::None) {
        e.Skip();
        return;
    }
    if (_keys.accept(e.GetKeyCode(), c))
        apply(c);
}

void BowlingCanvas::OnKeyUp(wxKeyEvent& e) {
    _keys.release(e.GetKeyCode());
    const Command c = InputMapper::keyUp(e.GetKeyCode(), _lane.snapshot().phase);
    if (c.type == Command::Type::None)
        e.Skip();
    else
        apply(c);
}

void BowlingCanvas::OnMouse(wxMouseEvent& e) {
    const Phase phase = _lane.snapshot().phase;
    if (e.LeftDown() || e.RightDown()) {
        SetFocus();
        apply(InputMapper::mouseDown(e.LeftDown(), phase));
    } else if (e.LeftUp() || e.RightUp()) {
        apply(InputMapper::mouseUp(e.LeftUp(), phase));
    } else if (e.Moving() || e.Dragging()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        apply(InputMapper::mouseMove(dx, e.LeftIsDown(), e.RightIsDown(), GetClientSize().x, phase));
    }
    _lastMouse = e.GetPosition();  // every event, so a left-drag leaves no stale origin
}

void BowlingCanvas::apply(Command command) {
    switch (targetOf(command.type)) {
        case CommandTarget::None:
            return;
        case CommandTarget::App:  // Quit
            if (onQuit) onQuit();
            return;
        case CommandTarget::View:  // CycleTheme
            setTheme(Render::nextTheme(_theme));
            return;
        case CommandTarget::Sound:
            setSoundSettings(_soundSettings.after(command.type));
            return;
        case CommandTarget::Lane:
            break;
    }
    auto r = LaneCommands::execute(command, _lane);
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
    else if (command.type == Command::Type::NewGame)
        _sound.onNewGame();
}

void BowlingCanvas::renderFrame() {
    const HudLayout layout = hudLayout(_ctx->width(), _ctx->height());
    const auto s = _lane.snapshot();

    _ctx->beginFrame({0.02f, 0.03f, 0.08f});  // kClearColor — splash backdrop
    _camera.setViewport(layout.width, layout.middlePx());
    _scene->draw(_camera, SceneAdapter::toScene(s), layout.bottomPx, layout.middlePx());
    _overlay->begin(layout.width, layout.height);
    drawHud(*_overlay, s, _lane.scoreSheet(), layout);
    _ctx->endFrame();
}
