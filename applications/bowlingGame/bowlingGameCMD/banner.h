#ifndef CONSOLE_BANNER_H
#define CONSOLE_BANNER_H
#include <string>

namespace Console {
/// Title, introduction and credits between bars one column narrower than
/// `width`; the same text the console printed before the refactor.
std::string banner(int width);
}  // namespace Console
#endif  // CONSOLE_BANNER_H
