#include "screenTools.h"
#include <Gamebox/Score/Notation.h>
#include <algorithm>
#include <cctype>

namespace ScreenTools {
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
/// @brief convert value to a 3 chars string for console
/// @param value eather pins number, game state or score,
/// encoded as in Gamebox::Score::Mark
/// @return a 3 chars string of the value
std::string valueToConsole(const int& value, const bool& isScore) {
    // the highest number is equal to the
    // maximum score that can be achieved
    // which is 300, so any higher number
    // is wrong input
    if (value / 1000 > 1)
        return "   ";
    const std::string text = Gamebox::Score::notation(value, isScore);
    switch (text.length()) {
        case 0:
            return "   ";
        case 1:
            return " " + text + " ";
        case 2:
            return " " + text;
        default:
            return text;
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