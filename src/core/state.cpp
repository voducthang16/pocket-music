#include "core/state.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "core/atomic_file.hpp"

SavedState loadState(const std::filesystem::path& path) {
    SavedState state;
    std::ifstream stream(path);
    std::string key;
    while (stream >> key) {
        if (key == "track")
            stream >> std::quoted(state.trackPath);
        else if (key == "position")
            stream >> state.positionSeconds;
        else if (key == "shuffle")
            stream >> state.shuffle;
        else if (key == "repeat")
            stream >> state.repeatMode;
        else if (key == "screen")
            stream >> std::quoted(state.screen);
        else {
            std::string ignored;
            std::getline(stream, ignored);
        }
    }
    state.positionSeconds = std::max(0, state.positionSeconds);
    if (state.repeatMode < 0 || state.repeatMode > 2) state.repeatMode = 0;
    if (state.screen != "library" && state.screen != "now-playing") state.screen = "library";
    return state;
}

bool saveState(const std::filesystem::path& path, const SavedState& state) {
    std::ostringstream stream;
    stream << "track " << std::quoted(state.trackPath) << '\n'
           << "position " << std::max(0, state.positionSeconds) << '\n'
           << "shuffle " << state.shuffle << '\n'
           << "repeat " << (state.repeatMode >= 0 && state.repeatMode <= 2 ? state.repeatMode : 0)
           << '\n'
           << "screen " << std::quoted(state.screen == "now-playing" ? "now-playing" : "library")
           << '\n';
    const auto content = stream.str();
    return atomicWriteFile(path, content);
}
