#include "core/player.hpp"
#include "test_suites.hpp"
namespace {
void jsonEscaping() {
    require(jsonString("A \\\"quote\"\n.mp3") == "\"A \\\\\\\"quote\\\"\\n.mp3\"",
            "mpv commands must safely encode paths");
    require(jsonString(std::string("a\x01", 2)) == "\"a\\u0001\"", "control bytes must be escaped");
}

void typedPropertyEvents() {
    const auto events =
        decodePlayerMessage(R"({"data":12.5,"name":"time-pos","event":"property-change"})", 7);
    require(events.size() == 1 && events[0].type == PlayerEventType::PositionChanged,
            "time-pos must decode as a typed event");
    require(events[0].generation == 7 && events[0].number == 12.5,
            "property event must retain generation and value");
    require(decodePlayerMessage(R"({"event":"property-change","name":"duration","data":null})", 7)
                .empty(),
            "null property values must be ignored safely");
    const auto seekable =
        decodePlayerMessage(R"({"event":"property-change","name":"seekable","data":true})", 7);
    require(seekable.size() == 1 && seekable[0].type == PlayerEventType::SeekableChanged &&
                seekable[0].flag,
            "seekable must decode as a typed boolean event");
}

void typedEndEvents() {
    const auto ended = decodePlayerMessage(R"({"reason":"eof","event":"end-file"})", 9);
    require(ended.size() == 1 && ended[0].type == PlayerEventType::Ended,
            "EOF must decode separately from errors");
    const auto failed = decodePlayerMessage(
        R"({"event":"end-file","reason":"error","file_error":"broken file"})", 9);
    require(failed.size() == 1 && failed[0].type == PlayerEventType::Failed &&
                failed[0].message == "broken file",
            "decoder errors must retain their message");
}

void malformedMessagesFailSafely() {
    const auto events = decodePlayerMessage("{not-json", 4);
    require(events.size() == 1 && events[0].type == PlayerEventType::Failed,
            "malformed IPC must become a typed failure");
}
}  // namespace
void addPlayerTests(TestCases& tests) {
    tests.emplace_back("JSON escaping", jsonEscaping);
    tests.emplace_back("typed properties", typedPropertyEvents);
    tests.emplace_back("typed end events", typedEndEvents);
    tests.emplace_back("malformed IPC", malformedMessagesFailSafely);
}
