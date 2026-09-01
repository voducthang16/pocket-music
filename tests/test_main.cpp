#include <iostream>

#include "test_suites.hpp"

int main() {
    TestCases tests;
    addLibraryTests(tests);
    addStateTests(tests);
    addPlayerTests(tests);
    addPlaybackTests(tests);
    addPreferencesTests(tests);
    addNavigationTests(tests);
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS  " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL  " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
