/**
  @file display.h
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the declaration of the Qt score sheet window
*/

#ifndef DISPLAY_H
#define DISPLAY_H
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPushButton>

#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>

namespace Ui {
class Display;
}

namespace BowlingDisplay {

/// Qt score sheet: one button per pin count, the 21 roll cells and the 10 frame scores.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// `game` defaults to a Bowlingbox::Score::ScoreCalculator.
    explicit MainWindow(QWidget* parent = nullptr,
                        std::unique_ptr<Bowlingbox::Score::IBowlingGame> game = nullptr);
    ~MainWindow() override;

private:
    void roll(int pins);
    void restart();
    void refresh();
    void updateGrid();
    void updatePinButtons();

    std::unique_ptr<Ui::Display> _ui;
    std::unique_ptr<Bowlingbox::Score::IBowlingGame> _game;
    QList<QPushButton*> _pinButtons;  // _pinButtons[i] rolls i pins
    QList<QLabel*> _rollLabels;       // the 21 roll slots, in Score encoding order
    QList<QLabel*> _scoreLabels;      // the 10 frame totals
};
}  // namespace BowlingDisplay
#endif  // DISPLAY_H
