#include <Gamebox/Core/Core.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <iostream>
#include <string>

TEST(test_Core_result, result_properties) {
    auto result = Result<int>(5);
    EXPECT_EQ(result(), 5);
    EXPECT_EQ(result.isValue(), true);
    EXPECT_EQ(result.isError(), false);
}

TEST(test_Core_error, error_properties) {
    auto error = Sc::Error<>("error");
    auto result = Result<int>(error);
    EXPECT_STREQ(result.error().c_str(), error().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}

TEST(test_Core_result_simple, simple_result_properties) {
    auto error = Sc::Error<>("error");
    auto result = Result<int>(error);
    EXPECT_STREQ(result.error().c_str(), error().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}

TEST(test_Core_result_void, void_result_properties) {
    auto error = Sc::Error<>("error");
    auto result = Result<>(error);
    EXPECT_STREQ(result.error().c_str(), error().c_str());
    EXPECT_EQ(result.isValue(), false);
    EXPECT_EQ(result.isError(), true);
}