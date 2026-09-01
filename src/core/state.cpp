#include "core/state.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

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
    std::error_code error;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ostringstream stream;
    stream << "track " << std::quoted(state.trackPath) << '\n'
           << "position " << std::max(0, state.positionSeconds) << '\n'
           << "shuffle " << state.shuffle << '\n'
           << "repeat " << (state.repeatMode >= 0 && state.repeatMode <= 2 ? state.repeatMode : 0)
           << '\n'
           << "screen " << std::quoted(state.screen == "now-playing" ? "now-playing" : "library")
           << '\n';
    const auto content = stream.str();
    const auto temporary = path.string() + ".tmp-" + std::to_string(getpid());
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file < 0) return false;
    size_t written = 0;
    bool ok = true;
    while (written < content.size()) {
        const auto count = write(file, content.data() + written, content.size() - written);
        if (count > 0)
            written += static_cast<size_t>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else {
            ok = false;
            break;
        }
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), path.c_str()) == 0;
    if (!ok) unlink(temporary.c_str());
    return ok;
}
