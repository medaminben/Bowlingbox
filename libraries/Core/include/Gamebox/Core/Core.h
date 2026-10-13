#ifndef CORE_H
#define CORE_H
#include <Gamebox/Core/Gamebox_Core_export.h>
#include <string>
#include <utility>
#include <variant>
static_assert(sizeof(void*) == 8, "expected 64-bit platform");
namespace Gamebox {
namespace Core {
/**
 * @brief an error wrapper
 *
 * @param E any movable error type
 */
template <typename E = std::string>
struct Error {
public:
    constexpr inline Error(E error) : _error(std::move(error)) {};

    constexpr inline auto&& operator()() { return std::move(_error); };

protected:
    E _error;
};

}  // namespace Core
}  // namespace Gamebox
namespace Sc = Gamebox::Core;

/**
 * @brief  a result wrapper
 *
 * @param T  a value type
 * @param ET  an error type
 */
template <typename T = std::monostate, typename ET = Sc::Error<>>
struct Result {
public:
    constexpr Result(T value) : result(std::move(value)) {};
    constexpr inline Result(ET error) : result(std::move(error)) {};

    constexpr inline auto isValue() const noexcept { return std::holds_alternative<T>(result); };

    constexpr inline auto isError() const noexcept { return std::holds_alternative<ET>(result); };

    constexpr inline auto&& operator()() { return std::move(std::get<T>(result)); };
    constexpr inline auto&& error() { return std::move(std::get<ET>(result)()); };

private:
    std::variant<T, ET> result;
};

#endif  // CORE_H
