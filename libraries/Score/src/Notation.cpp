#include <Gamebox/Score/Notation.h>

namespace Gamebox {
namespace Score {

std::string notation(int value, bool isScore) {
    if (isBlank(value))
        return "";
    switch (static_cast<Mark>(value)) {
        case Mark::Strike:
            return "X";
        case Mark::Spare:
            return "/";
        case Mark::Miss:
            return isScore ? "0" : "-";
        default:
            return std::to_string(value);
    }
}

}  // namespace Score
}  // namespace Gamebox
