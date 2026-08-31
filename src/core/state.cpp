#include "core/state.hpp"

#include <fstream>
#include <iomanip>

SavedState loadState(const std::filesystem::path& path) {
    SavedState state;
    std::ifstream stream(path);
    std::string key;
    while (stream >> key) {
        if (key == "track") {
            stream >> std::quoted(state.trackPath);
        } else if (key == "position") {
            stream >> state.positionSeconds;
        } else if (key == "shuffle") {
            stream >> state.shuffle;
        } else if (key == "repeat") {
            stream >> state.repeatMode;
        } else if (key == "screen") {
            stream >> std::quoted(state.screen);
        }
    }
    return state;
}

void saveState(const std::filesystem::path& path, const SavedState& state) {
    std::ofstream stream(path, std::ios::trunc);
    stream << "track " << std::quoted(state.trackPath) << '\n'
           << "position " << state.positionSeconds << '\n'
           << "shuffle " << state.shuffle << '\n'
           << "repeat " << state.repeatMode << '\n'
           << "screen " << std::quoted(state.screen) << '\n';
}
