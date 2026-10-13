#ifndef SCREEN_TOOLS_H
#define SCREEN_TOOLS_H
#include <string>
namespace ScreenTools {

std::string toConsole(const int&);

std::string valueToConsole(const int&, const bool& isScore = false);

std::string generateBar(const int&, const std::string&, const bool& with_return = true);

std::string formatTitle(const int&, const std::string&);

std::string giveAbout(const std::string&);

bool isNumber(const std::string&);

void print(std::string);

int getConsoleWidth();

bool clearScreen();
}  // namespace ScreenTools
#endif  // SCREEN_TOOLS_H