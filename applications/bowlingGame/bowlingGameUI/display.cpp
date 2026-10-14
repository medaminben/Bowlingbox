/**
  @file display.cpp
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the Qt score sheet window: rolls go to the game, the sheet is redrawn from it
*/
#include "display.h"
#include "ui_display.h"
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/Score.h>
#include <cstddef>

namespace BowlingDisplay {
namespace {
QString cellText(int value, bool isScore = false) {
    return QString::fromStdString(Bowlingbox::Score::notation(value, isScore));
}

/// The child of `parent` called `name`; display.ui must define it.
template <typename T>
T* widget(QWidget* parent, const QString& name) {
    T* found = parent->findChild<T*>(name);
    Q_ASSERT_X(found, "BowlingDisplay", qPrintable("display.ui has no widget " + name));
    return found;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent, std::unique_ptr<Bowlingbox::Score::IBowlingGame> game)
    : QMainWindow(parent),
      _ui(std::make_unique<Ui::Display>()),
      _game(game ? std::move(game) : std::make_unique<Bowlingbox::Score::ScoreCalculator>()) {
    _ui->setupUi(this);

    for (int pins = 0; pins <= Bowlingbox::Score::kPins; ++pins) {
        auto* button = widget<QPushButton>(this, QString("btn_%1").arg(pins));
        _pinButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, pins] { roll(pins); });
    }
    for (int frame = 1; frame <= Bowlingbox::Score::kFrames; ++frame) {
        const int rolls = frame == Bowlingbox::Score::kFrames ? 3 : 2;
        for (int r = 1; r <= rolls; ++r)
            _rollLabels.append(widget<QLabel>(this, QString("lblRoll%1Frame%2").arg(r).arg(frame)));
        _scoreLabels.append(widget<QLabel>(this, QString("lblScoreFrame%1").arg(frame)));
    }
    connect(_ui->btn_restart, &QPushButton::clicked, this, &MainWindow::restart);
    refresh();
}

MainWindow::~MainWindow() = default;

void MainWindow::roll(int pins) {
    _game->roll(pins);
    refresh();
}

void MainWindow::restart() {
    _game->reset();
    refresh();
}

void MainWindow::refresh() {
    updateGrid();
    updatePinButtons();
}

void MainWindow::updateGrid() {
    for (const auto& frame : Bowlingbox::Score::frames(*_game)) {
        for (int r = 0; r < frame.rollCount; ++r)
            _rollLabels.at(2 * frame.index + r)->setText(cellText(frame.rolls[static_cast<std::size_t>(r)]));
        _scoreLabels.at(frame.index)->setText(cellText(frame.score, true));
    }
    _ui->lblScore->setText(QString::number(_game->score()));
}

void MainWindow::updatePinButtons() {
    const bool over = _game->isOver();
    _ui->pinsGroup->setDisabled(over);
    const int left = _game->pinsLeft();
    for (int pins = 0; pins < _pinButtons.size(); ++pins)
        _pinButtons.at(pins)->setVisible(over || pins <= left);
}

}  // namespace BowlingDisplay
