#include "display.h"
#include <Bowlingbox/Score/Score.h>
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <vector>

namespace {
/// ScoreCalculator that counts reset() calls.
class CountingGame final : public Bowlingbox::Score::IBowlingGame {
public:
    explicit CountingGame(int& resets) : _resets(resets) {}
    bool roll(int pins) override { return _inner.roll(pins); }
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

QString text(QWidget& w, const QString& label) {
    return w.findChild<QLabel*>(label)->text();
}
QPushButton* pinButton(QWidget& w, int pins) {
    return w.findChild<QPushButton*>(QString("btn_%1").arg(pins));
}
void roll(QWidget& w, std::initializer_list<int> pins) {
    for (int p : pins)
        pinButton(w, p)->click();
}
}  // namespace

TEST(test_display, pin_buttons_fill_the_sheet) {
    BowlingDisplay::MainWindow w;
    roll(w, {7, 3, 10});
    EXPECT_EQ(text(w, "lblRoll1Frame1"), "7");
    EXPECT_EQ(text(w, "lblRoll2Frame1"), "/");
    EXPECT_EQ(text(w, "lblRoll1Frame2"), "X");
    EXPECT_EQ(text(w, "lblScoreFrame1"), "20");
    EXPECT_EQ(text(w, "lblScoreFrame2"), "");
}

TEST(test_display, only_standing_pin_counts_are_offered) {
    BowlingDisplay::MainWindow w;
    roll(w, {7});
    EXPECT_FALSE(pinButton(w, 3)->isHidden());
    EXPECT_TRUE(pinButton(w, 4)->isHidden());
    EXPECT_TRUE(pinButton(w, 10)->isHidden());
}

TEST(test_display, tenth_frame_shows_three_rolls) {
    BowlingDisplay::MainWindow w;
    for (int i = 0; i < 18; ++i)
        roll(w, {0});
    roll(w, {7, 3, 5});
    EXPECT_EQ(text(w, "lblRoll1Frame10"), "7");
    EXPECT_EQ(text(w, "lblRoll2Frame10"), "/");
    EXPECT_EQ(text(w, "lblRoll3Frame10"), "5");
    EXPECT_EQ(text(w, "lblScoreFrame10"), "15");
    EXPECT_EQ(text(w, "lblScore"), "15");
}

TEST(test_display, game_over_disables_the_pins) {
    BowlingDisplay::MainWindow w;
    for (int i = 0; i < 12; ++i)
        roll(w, {10});
    EXPECT_EQ(text(w, "lblScore"), "300");
    EXPECT_FALSE(w.findChild<QGroupBox*>("pinsGroup")->isEnabled());
    for (int pins = 0; pins <= 10; ++pins)
        EXPECT_FALSE(pinButton(w, pins)->isHidden()) << pins;
}

TEST(test_display, restart_resets_once_and_clears_the_sheet) {
    int resets = 0;
    BowlingDisplay::MainWindow w(nullptr, std::make_unique<CountingGame>(resets));
    roll(w, {7});
    w.findChild<QPushButton*>("btn_restart")->click();
    EXPECT_EQ(resets, 1);  // an extra connectSlotsByName connection would make this 2
    EXPECT_EQ(text(w, "lblRoll1Frame1"), "");
    EXPECT_EQ(text(w, "lblScore"), "0");
    EXPECT_FALSE(pinButton(w, 10)->isHidden());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");  // no display needed
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
