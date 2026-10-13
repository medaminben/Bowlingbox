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

#include <memory>
namespace Ui {
class Display;
}
namespace Sandbox {
namespace Gamebox {
class Bowlingbox;
}
}  // namespace Sandbox

class DisplayF : public QMainWindow {
    Q_OBJECT
public:
    explicit DisplayF(QWidget* parent = 0);
    ~DisplayF();

private:
    void updateGrid();
    void throwPins();
    void roll(const int&);
    void organizeLists();
    void refresh();
public slots:
    void on_btn_0_clicked();
    void on_btn_1_clicked();
    void on_btn_2_clicked();
    void on_btn_3_clicked();
    void on_btn_4_clicked();
    void on_btn_5_clicked();
    void on_btn_6_clicked();
    void on_btn_7_clicked();
    void on_btn_8_clicked();
    void on_btn_9_clicked();
    void on_btn_10_clicked();
    void on_btn_restart_clicked();

public:
    Ui::Display* ui;

private:
    QList<QPushButton*> pinsList;
    QList<QLabel*> frameList;
    QList<QLabel*> scoreList;

private:
    std::unique_ptr<Sandbox::Gamebox::Bowlingbox> _game;
};
#endif  // DISPLAY_H
