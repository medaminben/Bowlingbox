#include "ScoreSheetFrame.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/init.h>
#include <wx/stattext.h>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <vector>

namespace {
class CountingGame final : public Bowlingbox::Score::IBowlingGame {
public:
    explicit CountingGame(int& resets) : _resets(resets) {}
    Bowlingbox::Score::RollStatus roll(int pins) override { return _inner.roll(pins); }
    void reset() override {
        ++_resets;
        _inner.reset();
    }
    std::vector<int> rolls() const override { return _inner.rolls(); }
    std::vector<int> scores() const override { return _inner.scores(); }
    int score() const override { return _inner.score(); }
    int frameScore(int frameIndex) const override { return _inner.frameScore(frameIndex); }
    int pinsLeft() const override { return _inner.pinsLeft(); }
    bool isOver() const override { return _inner.isOver(); }

private:
    Bowlingbox::Score::ScoreCalculator _inner;
    int& _resets;
};

class WindowGuard {
public:
    explicit WindowGuard(std::unique_ptr<Bowlingbox::Score::IBowlingGame> game = nullptr)
        : _window(new BowlingDisplay::MainWindow(nullptr, std::move(game))) {
        _window->Show();
    }
    ~WindowGuard() { _window->Destroy(); }
    BowlingDisplay::MainWindow& get() { return *_window; }

private:
    BowlingDisplay::MainWindow* _window;
};

wxString text(wxWindow& w, const char* name) {
    auto* label = dynamic_cast<wxStaticText*>(wxWindow::FindWindowByName(name, &w));
    return label->GetLabel();
}
wxButton* pinButton(wxWindow& w, int pins) {
    return dynamic_cast<wxButton*>(
        wxWindow::FindWindowByName(wxString::Format("btn_%d", pins), &w));
}
void click(wxButton* button) {
    wxCommandEvent event(wxEVT_BUTTON, button->GetId());
    event.SetEventObject(button);
    button->ProcessWindowEvent(event);
}
void roll(wxWindow& w, std::initializer_list<int> pins) {
    for (int p : pins)
        click(pinButton(w, p));
}
}  // namespace

TEST(test_display, pin_buttons_fill_the_sheet) {
    WindowGuard guard;
    auto& w = guard.get();
    roll(w, {7, 3, 10});
    EXPECT_EQ(text(w, "lblRoll1Frame1"), wxString("7"));
    EXPECT_EQ(text(w, "lblRoll2Frame1"), wxString("/"));
    EXPECT_EQ(text(w, "lblRoll1Frame2"), wxString("X"));
    EXPECT_EQ(text(w, "lblScoreFrame1"), wxString("20"));
    EXPECT_EQ(text(w, "lblScoreFrame2"), wxString(""));
}

TEST(test_display, only_standing_pin_counts_are_offered) {
    WindowGuard guard;
    auto& w = guard.get();
    roll(w, {7});
    EXPECT_TRUE(pinButton(w, 3)->IsShown());
    EXPECT_FALSE(pinButton(w, 4)->IsShown());
    EXPECT_FALSE(pinButton(w, 10)->IsShown());
}

TEST(test_display, tenth_frame_shows_three_rolls) {
    WindowGuard guard;
    auto& w = guard.get();
    for (int i = 0; i < 18; ++i)
        roll(w, {0});
    roll(w, {7, 3, 5});
    EXPECT_EQ(text(w, "lblRoll1Frame10"), wxString("7"));
    EXPECT_EQ(text(w, "lblRoll2Frame10"), wxString("/"));
    EXPECT_EQ(text(w, "lblRoll3Frame10"), wxString("5"));
    EXPECT_EQ(text(w, "lblScoreFrame10"), wxString("15"));
    EXPECT_EQ(text(w, "lblScore"), wxString("15"));
}

TEST(test_display, game_over_disables_the_pins) {
    WindowGuard guard;
    auto& w = guard.get();
    for (int i = 0; i < 12; ++i)
        roll(w, {10});
    EXPECT_EQ(text(w, "lblScore"), wxString("300"));
    EXPECT_FALSE(wxWindow::FindWindowByName("pinsGroup", &w)->IsEnabled());
    for (int pins = 0; pins <= 10; ++pins)
        EXPECT_TRUE(pinButton(w, pins)->IsShown()) << pins;
}

TEST(test_display, a_perfect_game_fits_the_score_boxes) {
    WindowGuard guard;
    auto& w = guard.get();
    for (int i = 0; i < 12; ++i)
        roll(w, {10});
    w.Layout();
    const auto width = [&w](const char* name) {
        return wxWindow::FindWindowByName(name, &w)->GetSize().GetWidth();
    };
    // The sizer hands its leftover pixel to the last box.
    for (const char* frame : {"frame1", "frame10"})
        EXPECT_NEAR(width("GameScore"), width(frame), 1) << frame;
    for (const char* name : {"lblScore", "lblScoreFrame10"}) {
        auto* label = wxWindow::FindWindowByName(name, &w);
        EXPECT_GE(label->GetSize().GetWidth(), label->GetTextExtent("300").GetWidth()) << name;
    }
}

TEST(test_display, restart_resets_once_and_clears_the_sheet) {
    int resets = 0;
    WindowGuard guard(std::make_unique<CountingGame>(resets));
    auto& w = guard.get();
    roll(w, {7});
    click(dynamic_cast<wxButton*>(wxWindow::FindWindowByName("btn_restart", &w)));
    EXPECT_EQ(resets, 1);
    EXPECT_EQ(text(w, "lblRoll1Frame1"), wxString(""));
    EXPECT_EQ(text(w, "lblScore"), wxString("0"));
    EXPECT_TRUE(pinButton(w, 10)->IsShown());
}

namespace {
class DisplayTestApp : public wxApp {
public:
    bool OnInit() override { return true; }
};
}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (GTEST_FLAG_GET(list_tests))
        return RUN_ALL_TESTS();
    wxApp::SetInstance(new DisplayTestApp());
    if (!wxEntryStart(argc, argv)) {
        std::fprintf(stderr, "test_bowlingdisplay needs a display; run it under xvfb-run\n");
        return 1;
    }
    const int result = RUN_ALL_TESTS();
    wxEntryCleanup();
    return result;
}
