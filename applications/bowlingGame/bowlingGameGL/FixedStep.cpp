#include "FixedStep.h"
#include <algorithm>

int FixedStep::advance(float elapsedSeconds) {
    _pending += std::max(elapsedSeconds, 0.f);
    const int steps = static_cast<int>(_pending / kDt);
    if (steps > kMaxSteps) {
        _pending = 0.f;
        return kMaxSteps;
    }
    _pending -= static_cast<float>(steps) * kDt;
    return steps;
}
