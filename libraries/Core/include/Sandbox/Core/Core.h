#ifndef CORE_H
#define CORE_H
#include <Sandbox/Core/Sandbox_Core_export.h>
#include <variant>
#include <string>
#include <exception>
#include <memory>
static_assert(sizeof(void*) == 8, "expected 64-bit platform");
namespace Sandbox {
namespace Core {
/**
 * @brief
 *
 */
struct SANDBOX_CORE_API SandException : public std::exception {
    SandException(const char* str);
    ~SandException() throw();
    const char* what() const throw();

private:
    std::unique_ptr<std::string> message;
};

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

static constexpr const std::string unknown = "unknown";
/**
 * @brief Get an unique point id related to the actual session
 * It can be lead to duplicate if the used id source is external to the actual session
 * @return size_t
 */
SANDBOX_CORE_API size_t get_unique_index() noexcept;
}  // namespace Core
}  // namespace Sandbox
namespace Sc = Sandbox::Core;

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
