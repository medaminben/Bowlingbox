#include <Sandbox/Core/Core.h>
#include <atomic>
namespace Sandbox {
namespace Core {
static std::atomic<size_t> index_counter {0};
size_t get_unique_index() noexcept {
    return ++index_counter;
}

SandException::SandException(const char* str) {
    message = std::make_unique<std::string>(str);
}

SandException::~SandException() throw() {}

const char* SandException::what() const throw() {
    return message->c_str();
}

}  // namespace Core
}  // namespace Sandbox
