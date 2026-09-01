#include "core/player.hpp"
#include "test_suites.hpp"
namespace {
void jsonEscaping() {
    require(jsonString("A \\\"quote\"\n.mp3") == "\"A \\\\\\\"quote\\\"\\n.mp3\"",
            "mpv commands must safely encode paths");
    require(jsonString(std::string("a\x01", 2)) == "\"a\\u0001\"", "control bytes must be escaped");
}
}  // namespace
void addPlayerTests(TestCases& tests) { tests.emplace_back("JSON escaping", jsonEscaping); }
