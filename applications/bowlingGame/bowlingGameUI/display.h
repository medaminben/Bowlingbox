/**
  @file display.h
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the declaration of  display class
*/

#ifndef DISPLAY_H
#define DISPLAY_H
#include <QMainWindow>
#include <QList>
#include <QPushButton>
#include <QLabel>

#include <Gamebox/Score/IBowlingGame.h>
#include <memory>
namespace Ui {
class Display;
}

class DisplayF : public QMainWindow {
    Q_OBJECT
public:
    /// `game` defaults to a Gamebox::Score::Bowlingbox.
    explicit DisplayF(QWidget* parent = nullptr,
                      std::unique_ptr<Gamebox::Score::IBowlingGame> game = nullptr);
    ~DisplayF();

private:
    void updateGrid();
    void throwPins();
    void roll(const int&);
    void organizeLists();
    void connectPins();
    void refresh();
public slots:
    void on_btn_restart_clicked();

public:
    Ui::Display* ui;

private:
    QList<QPushButton*> pinsList;
    QList<QLabel*> frameList;
    QList<QLabel*> scoreList;

private:
    std::unique_ptr<Gamebox::Score::IBowlingGame> _game;
};
#endif  // DISPLAY_H
