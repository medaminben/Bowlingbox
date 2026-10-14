/**
  @file main.cpp
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details entry point to the app
*/
#include "display.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    BowlingDisplay::MainWindow w;
    w.show();

    return a.exec();
}
