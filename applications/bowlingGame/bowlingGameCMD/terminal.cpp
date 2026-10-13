#include "terminal.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <cstdlib>
#include <iostream>

namespace Console {
namespace {
constexpr int kFallbackWidth = 80;  // stdout is not a terminal (pipe, file)
}  // namespace

int SystemTerminal::width() const {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return kFallbackWidth;
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0)
        return kFallbackWidth;
    return size.ws_col;
#endif  // _WIN32
}

bool SystemTerminal::clear() {
#ifdef _WIN32
    return system("cls") != -1;
#else
    return system("clear") != -1;
#endif  // _WIN32
}

void SystemTerminal::write(const std::string& text) {
    std::cout << text;
}

std::string SystemTerminal::readLine() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

}  // namespace Console
