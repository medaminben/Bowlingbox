/**
  @file ScoreSheetFrame.h
  @details the wx score sheet window
*/
#ifndef SCORESHEETFRAME_H
#define SCORESHEETFRAME_H
#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>
#include <vector>
#include <wx/frame.h>

class wxButton;
class wxSizer;
class wxStaticText;

namespace BowlingDisplay {

/// wx score sheet: one button per pin count, the 21 roll cells and the 10 frame scores.
class MainWindow : public wxFrame {
public:
    /// `game` defaults to a Bowlingbox::Score::ScoreCalculator.
    explicit MainWindow(wxWindow* parent = nullptr,
                        std::unique_ptr<Bowlingbox::Score::IBowlingGame> game = nullptr);

private:
    void roll(int pins);
    void restart();
    void refresh();
    void updateGrid();
    void updatePinButtons();

    std::unique_ptr<Bowlingbox::Score::IBowlingGame> _game;
    wxSizer* _pinsSizer = nullptr;
    std::vector<wxButton*> _pinButtons;
    std::vector<wxStaticText*> _rollLabels;
    std::vector<wxStaticText*> _scoreLabels;
    wxStaticText* _total = nullptr;
};
}  // namespace BowlingDisplay
#endif  // SCORESHEETFRAME_H
