/**
  @file display.cpp
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the definition of all needed methods to calculate the score and display it
*/
#include "display.h"
#include "ui_display.h"
#include <Sandbox/Gamebox/Gamebox.h>

namespace {
QString valueToConsole(const int& value) {
    // the highest number is equal to the
    // maximum score that can be achieved
    // which is 300, so any higher number
    // is wrong input
    if (value / 1000 > 1)
        return "   ";
    // extract corner caeses: game states
    switch (value) {
        case -4:  // Empty
        case -3:  // not set
            return QString(" ");
        case -2:  // strike
            return QString("X");
        case -1:  // spare
            return QString("/");
        case 0:  // Miss
            return QString("-");
        default:  // numbers call toConsole
            return QString::number(value);
    }
}
}  // namespace

DisplayF::DisplayF(QWidget* parent) : QMainWindow(parent), ui(new Ui::Display) {
    _game = std::make_unique<Sandbox::Gamebox::Bowlingbox>();
    ui->setupUi(this);
    organizeLists();
    _game->reset();
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

    int first = -3, second = -3, score = -3, last = -3;

    for (auto frame_index = 0; frame_index < 10; frame_index += 1) {
        first = rolls[2 * frame_index];
        second = rolls[2 * frame_index + 1];
        score = scores[frame_index];

        frameList.at(2 * frame_index)->setText(valueToConsole(first));
        frameList.at(2 * frame_index + 1)->setText(valueToConsole(second));

        if (frame_index == 9) {
            last = rolls[2 * frame_index + 2];
            frameList.at(2 * frame_index + 2)->setText(valueToConsole(last));
        }
        scoreList.at(frame_index)->setText(valueToConsole(score));
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
    for (int i = 0; i <= 10; i++) {
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
void DisplayF::on_btn_0_clicked() {
    roll(0);
}

void DisplayF::on_btn_1_clicked() {
    roll(1);
}

void DisplayF::on_btn_2_clicked() {
    roll(2);
}

void DisplayF::on_btn_3_clicked() {
    roll(3);
}

void DisplayF::on_btn_4_clicked() {
    roll(4);
}

void DisplayF::on_btn_5_clicked() {
    roll(5);
}

void DisplayF::on_btn_6_clicked() {
    roll(6);
}

void DisplayF::on_btn_7_clicked() {
    roll(7);
}

void DisplayF::on_btn_8_clicked() {
    roll(8);
}

void DisplayF::on_btn_9_clicked() {
    roll(9);
}

void DisplayF::on_btn_10_clicked() {
    roll(10);
}

void DisplayF::on_btn_restart_clicked() {
    _game->reset();
    refresh();
}
