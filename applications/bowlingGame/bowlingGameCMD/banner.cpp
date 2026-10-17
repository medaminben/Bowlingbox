#include "banner.h"
#include <algorithm>
#include <cstddef>

namespace Console {

std::string banner(int width) {
    const std::string title = "Execution of BowlingGame from Bowlingbox library";
    const auto barWidth = static_cast<std::size_t>(std::max(width - 1, 0));
    // Today's padding, on both sides: (width - title - 1) / 2 - 1 spaces, 14 at 80 columns.
    const int padding = (width - static_cast<int>(title.size()) - 1) / 2 - 1;
    const std::string pad(static_cast<std::size_t>(std::max(padding, 0)), ' ');
    const std::string hashBar(barWidth, '#');
    return hashBar + "\n" + pad + title + pad + "\n" + hashBar + "\n" +
           "This console game is an interface to the bowling game implemented in the Bowlingbox library.\n"
           "=About=\n Made by: github.com/medaminben\n" +
           std::string(barWidth, '~') + "\n\n";
}

}  // namespace Console
