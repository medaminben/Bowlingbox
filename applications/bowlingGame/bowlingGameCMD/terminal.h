#ifndef TERMINAL_H
#define TERMINAL_H
#include <string>

namespace Console {
/// Text I/O seen by the console game. Lets the game run against a real
/// terminal or a scripted one.
class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual int width() const = 0;
    virtual bool clear() = 0;
    virtual void write(const std::string& text) = 0;
    virtual std::string readLine() = 0;
};

/// stdin / stdout with platform specific width and clear.
class SystemTerminal final : public ITerminal {
public:
    int width() const override;
    bool clear() override;
    void write(const std::string& text) override;
    std::string readLine() override;
};
}  // namespace Console
#endif  // TERMINAL_H
