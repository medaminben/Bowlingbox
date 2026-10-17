#include <Bowlingbox/Core/Core.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using Bowlingbox::Result;
using Bowlingbox::Core::Error;

static_assert(std::is_same_v<Result<>, Bowlingbox::Result<std::monostate, Error<std::string>>>,
              "Result lives in namespace Bowlingbox with Core::Error<> as the default error");

TEST(test_core_result, result_properties) {
    auto result = Result<int>(5);
    EXPECT_EQ(result.value(), 5);
    EXPECT_TRUE(result.isValue());
    EXPECT_FALSE(result.isError());
}

TEST(test_core_result, error_properties) {
    const auto error = Error<>("error");
    auto result = Result<int>(error);
    EXPECT_EQ(result.error(), error.value());
    EXPECT_FALSE(result.isValue());
    EXPECT_TRUE(result.isError());
}

TEST(test_core_result, void_result_properties) {
    auto ok = Result<>(std::monostate {});
    EXPECT_TRUE(ok.isValue());
    auto refused = Result<>(Error<>("refused"));
    EXPECT_TRUE(refused.isError());
    EXPECT_EQ(refused.error(), "refused");
}

TEST(test_core_result, value_can_be_read_twice) {
    auto result = Result<std::string>(std::string("pins"));
    EXPECT_EQ(result.value(), "pins");
    EXPECT_EQ(result.value(), "pins");
}

TEST(test_core_result, error_can_be_read_twice) {
    auto result = Result<int>(Error<>("no GL context"));
    EXPECT_EQ(result.error(), "no GL context");
    EXPECT_EQ(result.error(), "no GL context");
}

TEST(test_core_result, move_only_value_is_taken_with_std_move) {
    auto result = Result<std::unique_ptr<int>>(std::make_unique<int>(7));
    std::unique_ptr<int> taken = std::move(result).value();
    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(*taken, 7);
}

TEST(test_core_result, const_result_is_readable) {
    const auto result = Result<int>(3);
    EXPECT_EQ(result.value(), 3);
}
