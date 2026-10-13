#include "screenTools.h"
#include <iostream>
#include <algorithm>

namespace ScreenTools {
auto print(std::string message) -> void {
    std::cout << message << "\n";
}
/// @brief  convert int to a 3 chars string
/// @param i int to convert, pins number or score
/// @return
std::string toConsole(const int& i) {
    // convert to string
    auto i_str = std::to_string(i);
    // format to 3 chars string
    switch (i_str.length()) {
        // one digit number
        case 1:
            return " " + i_str + " ";
        case 2:
            return " " + i_str;
        default:
            return i_str;
    }
}
/// @brief convert value to string for console
/// @param value eather pins number, bonus case or score.
/// the game states are tolerates as negative numbers see
/// @enum Gamebox::Bowlingbox::Bonus
/// @return a 3 chars string of the value
std::string valueToConsole(const int& value, const bool& isScore) {
    // the highest number is equal to the
    // maximum score that can be achieved
    // which is 300, so any higher number
    // is wrong input
    if (value / 1000 > 1)
        return "   ";
    if (isScore && value == 0)
        return toConsole(value);
    // extract corner caeses: game states
    switch (value) {       //                                                        .-------.
        case -4:           // empty, usually after a strike in regular frame | X |___|
            return "   ";  // a special case of none (not set arbitrary)
        case -3:           // not set yet (not played)
            return "   ";
        case -2:  // strike
            return " X ";
        case -1:  // spare
            return " / ";
        case 0:  // miss
            return " - ";
        default:  // numbers call toConsole
            return toConsole(value);
    }
}
std::string generateBar(const int& length, const std::string& stone, const bool& with_return) {
    std::string bar = "";
    for (int i = 0; i < length - 1; i++)
        bar += stone;
    if (with_return)
        return bar + "\n";
    return bar;
}
std::string formatTitle(const int& length, const std::string& title) {
    std::string space = " ", new_line = "\n", colomn = "|";

    int space_length = (length - title.length() - 1) / 2;
    std::string spacer = generateBar(space_length, space, false);
    return spacer + title + spacer + new_line;
}
std::string giveAbout(const std::string& author_name) {
    std::string about = "=About=\n Made by: ";
    return about + author_name + "\n";
}

/// @brief check if string contains only digits
/// @param str the string
/// @return true if the string contains only digits
bool isNumber(const std::string& str) {
    // fast return if empty
    if (str.empty())
        return false;
    // check if all digits
    return std::find_if(str.begin(), str.end(), [](unsigned char c) { return !std::isdigit(c); }) ==
           str.end();
}
}  // namespace ScreenTools