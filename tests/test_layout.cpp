#include "test_suites.hpp"
#include "ui/layout.hpp"

void addLayoutTests(TestCases& tests) {
    tests.push_back({"TrimUI layout uses the native landscape canvas", [] {
                         require(layout::width == 1024, "logical width must match the display");
                         require(layout::height == 768, "logical height must match the display");
                     }});
}
