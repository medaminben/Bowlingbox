#ifndef GAMEBOX_SCORE_NOTATION_H
#define GAMEBOX_SCORE_NOTATION_H
#include <Gamebox/Score/Gamebox_Score_export.h>
#include <string>

namespace Gamebox {
namespace Score {

inline constexpr int kFrames = 10;
inline constexpr int kPins = 10;
inline constexpr int kRollSlots = 21;  // 9 frames x 2 + 3 for the last frame

/// Sentinel values used in the 21 roll slots and the 10 frame scores.
/// Any value >= 1 is a plain pin count (rolls) or a running total (scores).
enum class Mark : int { Empty = -4, None = -3, Strike = -2, Spare = -1, Miss = 0 };

constexpr int toInt(Mark m) noexcept {
    return static_cast<int>(m);
}
/// True for slots that hold nothing to show (not played yet, or the cell after a strike).
constexpr bool isBlank(int value) noexcept {
    return value == toInt(Mark::None) || value == toInt(Mark::Empty);
}

/// Standard score sheet notation: blank -> "", Strike -> "X", Spare -> "/",
/// Miss -> "-" (or "0" when isScore), otherwise the number.
GAMEBOX_SCORE_API std::string notation(int value, bool isScore = false);

}  // namespace Score
}  // namespace Gamebox
#endif  // GAMEBOX_SCORE_NOTATION_H
