#include "InputMapper.h"
#include <wx/defs.h>
#include <gtest/gtest.h>
#include <ostream>

using Bowlingbox::Alley::Phase;
using Type = Command::Type;

// Readable failure messages for EXPECT_EQ on commands.
void PrintTo(const Command& c, std::ostream* os) {
    *os << "Command{type " << static_cast<int>(c.type) << ", amount " << c.amount << "}";
}

namespace {
Command cmd(Type type, float amount = 0.f) {
    return {type, amount};
}
}  // namespace

TEST(test_input_mapper, arrows_move_while_aiming_or_charging) {
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Aiming), cmd(Type::MoveAim, -InputMapper::kKeyOffsetStep));
    EXPECT_EQ(InputMapper::keyDown(WXK_RIGHT, false, Phase::Charging), cmd(Type::MoveAim, InputMapper::kKeyOffsetStep));
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Rolling), cmd(Type::None));
}

TEST(test_input_mapper, a_and_d_rotate_the_aim) {
    EXPECT_EQ(InputMapper::keyDown('A', false, Phase::Aiming), cmd(Type::RotateAim, -InputMapper::kKeyAngleStep));
    EXPECT_EQ(InputMapper::keyDown('d', false, Phase::Aiming), cmd(Type::RotateAim, InputMapper::kKeyAngleStep));
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::GameOver), cmd(Type::None));
}

TEST(test_input_mapper, space_charges_and_releases) {
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Aiming), cmd(Type::StartCharge));
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Charging), cmd(Type::None));  // key repeat
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Charging), cmd(Type::Release));
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, new_game_and_quit_work_in_any_phase) {
    EXPECT_EQ(InputMapper::keyDown('N', false, Phase::Rolling), cmd(Type::NewGame));
    EXPECT_EQ(InputMapper::keyDown('N', true, Phase::GameOver), cmd(Type::NewGame));
    EXPECT_EQ(InputMapper::keyDown(WXK_ESCAPE, false, Phase::Settling), cmd(Type::Quit));
    EXPECT_EQ(InputMapper::keyDown('Q', true, Phase::Aiming), cmd(Type::Quit));
    EXPECT_EQ(InputMapper::keyDown('Q', false, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, mouse_buttons_mirror_space) {
    EXPECT_EQ(InputMapper::mouseDown(true, Phase::Aiming), cmd(Type::StartCharge));
    EXPECT_EQ(InputMapper::mouseDown(false, Phase::Aiming), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Charging), cmd(Type::Release));
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Rolling), cmd(Type::None));
}

TEST(test_input_mapper, mouse_move_shifts_or_turns_only_while_aiming) {
    const Command move = InputMapper::mouseMove(100, false, false, 1000, Phase::Aiming);
    EXPECT_EQ(move.type, Type::MoveAim);
    EXPECT_NEAR(move.amount, 0.12f, 1e-5f);  // 1.2 m across the canvas
    const Command turn = InputMapper::mouseMove(10, false, true, 1000, Phase::Aiming);
    EXPECT_EQ(turn.type, Type::RotateAim);
    EXPECT_NEAR(turn.amount, 0.5f, 1e-5f);   // 0.05° per pixel
    EXPECT_EQ(InputMapper::mouseMove(10, true, false, 1000, Phase::Aiming), cmd(Type::None));  // left-drag
    EXPECT_EQ(InputMapper::mouseMove(10, false, false, 1000, Phase::Rolling), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseMove(0, false, false, 1000, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, zero_width_canvas_does_not_move_the_ball) {
    EXPECT_EQ(InputMapper::mouseMove(50, false, false, 0, Phase::Aiming), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseMove(50, false, false, -1, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, t_cycles_the_theme_in_every_phase) {
    for (Phase p : {Phase::Aiming, Phase::Charging, Phase::Rolling, Phase::Settling, Phase::FrameDone,
                    Phase::GameOver}) {
        EXPECT_EQ(InputMapper::keyDown('T', false, p), cmd(Type::CycleTheme));
        EXPECT_EQ(InputMapper::keyDown('t', false, p), cmd(Type::CycleTheme));
    }
}
