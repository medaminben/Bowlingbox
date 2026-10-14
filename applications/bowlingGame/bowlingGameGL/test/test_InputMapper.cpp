#include "InputMapper.h"
#include <wx/defs.h>
#include <gtest/gtest.h>

using Bowlingbox::Alley::Phase;

TEST(test_input_mapper, arrows_move_while_aiming_or_charging) {
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Aiming), Command::MoveLeft);
    EXPECT_EQ(InputMapper::keyDown(WXK_RIGHT, false, Phase::Charging), Command::MoveRight);
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Rolling), Command::None);
}

TEST(test_input_mapper, a_and_d_rotate_the_aim) {
    EXPECT_EQ(InputMapper::keyDown('A', false, Phase::Aiming), Command::AngleLeft);
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::Aiming), Command::AngleRight);
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::GameOver), Command::None);
}

TEST(test_input_mapper, space_charges_and_releases) {
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Aiming), Command::StartCharge);
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Charging), Command::None);  // key repeat
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Charging), Command::Release);
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Aiming), Command::None);
}

TEST(test_input_mapper, new_game_and_quit_work_in_any_phase) {
    EXPECT_EQ(InputMapper::keyDown('N', false, Phase::Rolling), Command::NewGame);
    EXPECT_EQ(InputMapper::keyDown('N', true, Phase::GameOver), Command::NewGame);
    EXPECT_EQ(InputMapper::keyDown(WXK_ESCAPE, false, Phase::Settling), Command::Quit);
    EXPECT_EQ(InputMapper::keyDown('Q', true, Phase::Aiming), Command::Quit);
    EXPECT_EQ(InputMapper::keyDown('Q', false, Phase::Aiming), Command::None);
}

TEST(test_input_mapper, mouse_buttons_mirror_space) {
    EXPECT_EQ(InputMapper::mouseDown(true, Phase::Aiming), Command::StartCharge);
    EXPECT_EQ(InputMapper::mouseDown(false, Phase::Aiming), Command::None);
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Charging), Command::Release);
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Rolling), Command::None);
}

TEST(test_input_mapper, mouse_deltas_scale_to_lane_units) {
    EXPECT_NEAR(InputMapper::mouseDxToOffset(100, 1000), 0.12f, 1e-5f);  // 1.2 m across the canvas
    EXPECT_NEAR(InputMapper::mouseDxToAngle(10), 0.5f, 1e-5f);           // 0.05° per pixel
}
