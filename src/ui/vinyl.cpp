#include "ui/vinyl.hpp"

#include <cmath>

float advanceVinylAngle(float angle, Uint64 elapsedMs, bool spinning, bool loading) {
    if (!spinning) return angle;
    const float degreesPerMillisecond = loading ? 0.06f : 0.03f;
    return std::fmod(angle + static_cast<float>(elapsedMs) * degreesPerMillisecond, 360.0f);
}
