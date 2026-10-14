/**
  @file ScoreSheetFrame.cpp
  @details the wx score sheet window: rolls go to the game, the sheet is redrawn from it
*/
#include "ScoreSheetFrame.h"
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/Notation.h>
#include <Bowlingbox/Score/Score.h>
#include <cstddef>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

namespace BowlingDisplay {
namespace {
wxStaticText* makeCell(wxWindow* parent, const wxString& name, const wxFont& font) {
    auto* label = new wxStaticText(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxSize(30, 30), wxALIGN_CENTRE_HORIZONTAL);
    label->SetName(name);
    label->SetFont(font);
    return label;
}
}  // namespace

MainWindow::MainWindow(wxWindow* parent, std::unique_ptr<Bowlingbox::Score::IBowlingGame> game)
    : wxFrame(parent, wxID_ANY, "https://github.com/medaminben", wxDefaultPosition, wxSize(1200, 200)),
      _game(game ? std::move(game) : std::make_unique<Bowlingbox::Score::ScoreCalculator>()) {
    SetMinSize(wxSize(1200, 200));
    SetMaxSize(wxSize(1200, 200));

    const wxFont rollFont(wxFontInfo(12).Bold());
    const wxFont frameFont(wxFontInfo(14));
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* pinsBox = new wxStaticBox(this, wxID_ANY, "Pins Down");
    pinsBox->SetName("pinsGroup");
    _pinsSizer = new wxStaticBoxSizer(pinsBox, wxHORIZONTAL);
    for (int pins = 0; pins <= Bowlingbox::Score::kPins; ++pins) {
        auto* button = new wxButton(pinsBox, wxID_ANY, wxString::Format("%d", pins),
                                    wxDefaultPosition, wxSize(30, 30));
        button->SetName(wxString::Format("btn_%d", pins));
        button->SetMinSize(wxSize(30, 30));
        button->SetMaxSize(wxSize(30, 30));
        button->Bind(wxEVT_BUTTON, [this, pins](wxCommandEvent&) { roll(pins); });
        _pinButtons.push_back(button);
        _pinsSizer->Add(button, 0, wxALL, 2);
    }

    auto* restart = new wxButton(this, wxID_ANY, wxString::FromUTF8("⟲"), wxDefaultPosition,
                                 wxSize(100, 73));
    restart->SetName("btn_restart");
    restart->SetFont(wxFontInfo(28));
    restart->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { this->restart(); });

    auto* top = new wxBoxSizer(wxHORIZONTAL);
    top->Add(_pinsSizer, 0, wxEXPAND | wxALL, 4);
    top->AddStretchSpacer(1);
    top->Add(restart, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    root->Add(top, 0, wxEXPAND);

    auto* frames = new wxBoxSizer(wxHORIZONTAL);
    for (int frame = 1; frame <= Bowlingbox::Score::kFrames; ++frame) {
        auto* box = new wxStaticBox(this, wxID_ANY, wxEmptyString);
        box->SetName(wxString::Format("frame%d", frame));
        auto* boxSizer = new wxStaticBoxSizer(box, wxVERTICAL);
        auto* rolls = new wxBoxSizer(wxHORIZONTAL);
        const int rollCount = frame == Bowlingbox::Score::kFrames ? 3 : 2;
        for (int r = 1; r <= rollCount; ++r) {
            auto* label = makeCell(box, wxString::Format("lblRoll%dFrame%d", r, frame), rollFont);
            rolls->Add(label, 1, wxALIGN_CENTER | wxALL, 1);
            _rollLabels.push_back(label);
        }
        boxSizer->Add(rolls, 0, wxEXPAND);
        auto* score = makeCell(box, wxString::Format("lblScoreFrame%d", frame), frameFont);
        boxSizer->Add(score, 1, wxALIGN_CENTER);
        _scoreLabels.push_back(score);
        frames->Add(boxSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 2);
    }

    auto* gameBox = new wxStaticBox(this, wxID_ANY, wxEmptyString);
    gameBox->SetName("GameScore");
    auto* gameSizer = new wxStaticBoxSizer(gameBox, wxVERTICAL);
    _total = makeCell(gameBox, "lblScore", wxFontInfo(20).Bold());
    gameSizer->Add(_total, 1, wxALIGN_CENTER);

    auto* bottom = new wxBoxSizer(wxHORIZONTAL);
    bottom->Add(frames, 1, wxEXPAND);
    bottom->Add(gameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);
    root->Add(bottom, 1, wxEXPAND);

    SetSizer(root);
    refresh();
}

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
        for (int r = 0; r < frame.rollCount; ++r) {
            _rollLabels.at(static_cast<std::size_t>(2 * frame.index + r))
                ->SetLabel(wxString::FromUTF8(
                    Bowlingbox::Score::notation(frame.rolls[static_cast<std::size_t>(r)])));
        }
        _scoreLabels.at(static_cast<std::size_t>(frame.index))
            ->SetLabel(wxString::FromUTF8(Bowlingbox::Score::notation(frame.score, true)));
    }
    _total->SetLabel(wxString::Format("%d", _game->score()));
}

void MainWindow::updatePinButtons() {
    const bool over = _game->isOver();
    auto* pinsGroup = wxWindow::FindWindowByName("pinsGroup", this);
    pinsGroup->Enable(!over);
    const int left = _game->pinsLeft();
    for (int pins = 0; pins <= Bowlingbox::Score::kPins; ++pins)
        _pinButtons.at(static_cast<std::size_t>(pins))->Show(over || pins <= left);
    _pinsSizer->Layout();
}
}  // namespace BowlingDisplay
