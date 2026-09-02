#include <cmath>

#include "test_suites.hpp"
#include "ui/vinyl.hpp"

void addVinylTests(TestCases& tests) {
    tests.push_back({"vinyl rotates only during active playback", [] {
                         require(std::abs(advanceVinylAngle(30.0f, 1000, false, false) - 30.0f) <
                                     0.001f,
                                 "paused vinyl must preserve its angle");
                         require(std::abs(advanceVinylAngle(30.0f, 1000, true, false) - 60.0f) <
                                     0.001f,
                                 "playing vinyl must rotate at a calm speed");
                         require(std::abs(advanceVinylAngle(350.0f, 1000, true, false) - 20.0f) <
                                     0.001f,
                                 "vinyl angle must wrap after one revolution");
                         require(std::abs(advanceVinylAngle(30.0f, 1000, true, true) - 90.0f) <
                                     0.001f,
                                 "loading vinyl must rotate faster than playback");
                     }});
}
