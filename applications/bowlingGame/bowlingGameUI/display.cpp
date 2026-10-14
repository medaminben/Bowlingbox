/**
  @file display.cpp
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the definition of all needed methods to calculate the score and display it
*/
#include "display.h"
#include "ui_display.h"
#include <Bowlingbox/Score/Score.h>

namespace {
QString cellText(int value, bool isScore = false) {
    return QString::fromStdString(Bowlingbox::Score::notation(value, isScore));
}
}  // namespace

DisplayF::DisplayF(QWidget* parent, std::unique_ptr<Bowlingbox::Score::IBowlingGame> game)
    : QMainWindow(parent),
      ui(new Ui::Display),
      _game(game ? std::move(game) : std::make_unique<Bowlingbox::Score::ScoreCalculator>()) {
    ui->setupUi(this);
    organizeLists();
    connectPins();
    refresh();
}

DisplayF::~DisplayF() {
    delete ui;
}

void DisplayF::roll(const int& pins) {
    _game->roll(pins);
    refresh();
}
// ######################################################
// Gui Updaters

void DisplayF::updateGrid() {
    auto rolls = _game->rolls();
    auto scores = _game->scores();
    auto gameScore = _game->score();

    const int none = Bowlingbox::Score::toInt(Bowlingbox::Score::Mark::None);
    int first = none, second = none, score = none, last = none;

    for (auto frame_index = 0; frame_index < Bowlingbox::Score::kFrames; frame_index += 1) {
        first = rolls[2 * frame_index];
        second = rolls[2 * frame_index + 1];
        score = scores[frame_index];

        frameList.at(2 * frame_index)->setText(cellText(first));
        frameList.at(2 * frame_index + 1)->setText(cellText(second));

        if (frame_index == 9) {
            last = rolls[2 * frame_index + 2];
            frameList.at(2 * frame_index + 2)->setText(cellText(last));
        }
        scoreList.at(frame_index)->setText(cellText(score, true));
    }
    ui->lblScore->setText(QString::number(gameScore));
}

void DisplayF::throwPins() {
    if (_game->isOver()) {
        for (auto pins : pinsList)
            pins->show();
        ui->pinsGroup->setDisabled(true);
        return;
    }

    auto left = _game->pinsLeft();
    ui->pinsGroup->setDisabled(false);
    for (int i = 0; i <= Bowlingbox::Score::kPins; i++) {
        if (i <= left)
            pinsList.at(i)->show();
        else
            pinsList.at(i)->hide();
    }
}

// #######################################################
// Helpers
void DisplayF::organizeLists() {
    // bind the pins buttons with a list to ease the control and iteration
    pinsList.append(ui->btn_0);
    pinsList.append(ui->btn_1);
    pinsList.append(ui->btn_2);
    pinsList.append(ui->btn_3);
    pinsList.append(ui->btn_4);
    pinsList.append(ui->btn_5);
    pinsList.append(ui->btn_6);
    pinsList.append(ui->btn_7);
    pinsList.append(ui->btn_8);
    pinsList.append(ui->btn_9);
    pinsList.append(ui->btn_10);

    // bind the rollingframe in a list to ease the control and iteration (21 labels)
    frameList.append(ui->lblRoll1Frame1);
    frameList.append(ui->lblRoll2Frame1);
    frameList.append(ui->lblRoll1Frame2);
    frameList.append(ui->lblRoll2Frame2);
    frameList.append(ui->lblRoll1Frame3);
    frameList.append(ui->lblRoll2Frame3);
    frameList.append(ui->lblRoll1Frame4);
    frameList.append(ui->lblRoll2Frame4);
    frameList.append(ui->lblRoll1Frame5);
    frameList.append(ui->lblRoll2Frame5);
    frameList.append(ui->lblRoll1Frame6);
    frameList.append(ui->lblRoll2Frame6);
    frameList.append(ui->lblRoll1Frame7);
    frameList.append(ui->lblRoll2Frame7);
    frameList.append(ui->lblRoll1Frame8);
    frameList.append(ui->lblRoll2Frame8);
    frameList.append(ui->lblRoll1Frame9);
    frameList.append(ui->lblRoll2Frame9);
    frameList.append(ui->lblRoll1Frame10);
    frameList.append(ui->lblRoll2Frame10);
    frameList.append(ui->lblRoll3Frame10);

    // bind the frame score labels with a list to ease the control and iteration
    scoreList.append(ui->lblScoreFrame1);
    scoreList.append(ui->lblScoreFrame2);
    scoreList.append(ui->lblScoreFrame3);
    scoreList.append(ui->lblScoreFrame4);
    scoreList.append(ui->lblScoreFrame5);
    scoreList.append(ui->lblScoreFrame6);
    scoreList.append(ui->lblScoreFrame7);
    scoreList.append(ui->lblScoreFrame8);
    scoreList.append(ui->lblScoreFrame9);
    scoreList.append(ui->lblScoreFrame10);
}

void DisplayF::refresh() {
    updateGrid();
    throwPins();
}

// #######################################################
// Buttons Signals
void DisplayF::connectPins() {
    // button i rolls i pins
    for (int pins = 0; pins < pinsList.size(); ++pins)
        connect(pinsList.at(pins), &QPushButton::clicked, this, [this, pins] { roll(pins); });
}

void DisplayF::on_btn_restart_clicked() {
    _game->reset();
    refresh();
}
