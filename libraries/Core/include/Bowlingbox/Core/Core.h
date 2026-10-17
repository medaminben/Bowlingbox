#ifndef BOWLINGBOX_CORE_H
#define BOWLINGBOX_CORE_H
#include <Bowlingbox/Core/Bowlingbox_Core_export.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace Bowlingbox {
namespace Core {
/**
 * @brief An error wrapper.
 *
 * @tparam E Any movable error type.
 */
template <typename E = std::string>
struct Error {
public:
    constexpr Error(E error) : _error(std::move(error)) {}
    constexpr const E& value() const& noexcept { return _error; }
    constexpr E&& value() && noexcept { return std::move(_error); }

protected:
    E _error;
};
}  // namespace Core

/**
 * @brief A value or an error. Reading never moves: `r.value()` and `r.error()` return
 *        references, and `std::move(r).value()` takes the value out.
 *
 * @tparam T The value type.
 * @tparam ET The error type; it must have `value()` like Core::Error.
 */
template <typename T = std::monostate, typename ET = Core::Error<>>
class [[nodiscard]] Result {
    static_assert(!std::is_same_v<T, ET>, "Result needs distinct value and error types");

public:
    constexpr Result(T value) : _result(std::in_place_index<0>, std::move(value)) {}
    constexpr Result(ET error) : _result(std::in_place_index<1>, std::move(error)) {}
    constexpr bool isValue() const noexcept { return _result.index() == 0; }
    constexpr bool isError() const noexcept { return _result.index() == 1; }
    constexpr T& value() & { return std::get<0>(_result); }
    constexpr const T& value() const& { return std::get<0>(_result); }
    constexpr T&& value() && { return std::get<0>(std::move(_result)); }
    constexpr decltype(auto) error() const& { return std::get<1>(_result).value(); }
    constexpr decltype(auto) error() && { return std::get<1>(std::move(_result)).value(); }

private:
    std::variant<T, ET> _result;
};
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_CORE_H
