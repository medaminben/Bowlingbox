// Include Project headrs
#include <Sandbox/Gamebox/Gamebox.h>
#include "bowlingScreen.h"

// Include 3rdparty headers

// Include std headers
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif
#include <unistd.h>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>

using namespace Sandbox;

int main(void) {
    // Setup
    const auto game = std::make_shared<Gamebox::Bowlingbox>();
    const auto screen = std::make_unique<Console::Screen>(game);
    //////////////////////////////////////////////////////////////
    // loop
    screen->start();
    //////////////////////////////////////////////////////////////
    return 0;
}