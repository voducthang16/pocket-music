#include "core/state.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

#include "core/atomic_file.hpp"

namespace {
PlaybackSession defaults() { return {}; }

bool isConsistent(const PlaybackSession& session) {
    if (session.sourcePaths.empty())
        return session.orderPaths.empty() && session.historyPaths.empty() &&
               session.currentTrackPath.empty() && session.cursor == 0;
    if (session.orderPaths.size() != session.sourcePaths.size() ||
        session.cursor >= session.orderPaths.size() || session.currentTrackPath.empty())
        return false;
    std::map<std::string, size_t> sourceCounts;
    std::map<std::string, size_t> orderCounts;
    for (const auto& path : session.sourcePaths) ++sourceCounts[path];
    for (const auto& path : session.orderPaths) ++orderCounts[path];
    if (sourceCounts != orderCounts ||
        session.orderPaths[session.cursor] != session.currentTrackPath)
        return false;
    for (const auto& path : session.historyPaths)
        if (sourceCounts.find(path) == sourceCounts.end()) return false;
    return true;
}
}  // namespace

PlaybackSession loadSession(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) return defaults();
    PlaybackSession session;
    std::string key;
    while (stream >> key) {
        if (key == "current")
            stream >> std::quoted(session.currentTrackPath);
        else if (key == "source_title")
            stream >> std::quoted(session.sourceTitle);
        else if (key == "source") {
            std::string value;
            stream >> std::quoted(value);
            session.sourcePaths.push_back(std::move(value));
        } else if (key == "order") {
            std::string value;
            stream >> std::quoted(value);
            session.orderPaths.push_back(std::move(value));
        } else if (key == "history") {
            std::string value;
            stream >> std::quoted(value);
            session.historyPaths.push_back(std::move(value));
        } else if (key == "cursor")
            stream >> session.cursor;
        else if (key == "position")
            stream >> session.positionSeconds;
        else if (key == "shuffle")
            stream >> session.shuffle;
        else if (key == "repeat")
            stream >> session.repeatMode;
        else if (key == "screen")
            stream >> std::quoted(session.screen);
        else {
            std::string ignored;
            std::getline(stream, ignored);
        }
        if (!stream) return defaults();
    }
    if (!std::isfinite(session.positionSeconds) || session.positionSeconds < 0)
        session.positionSeconds = 0;
    if (session.repeatMode < 0 || session.repeatMode > 2) session.repeatMode = 0;
    if (session.screen != "home" && session.screen != "now-playing") session.screen = "home";
    if (!isConsistent(session)) return defaults();
    return session;
}

bool saveSession(const std::filesystem::path& path, const PlaybackSession& session) {
    std::ostringstream stream;
    stream << "current " << std::quoted(session.currentTrackPath) << '\n'
           << "source_title " << std::quoted(session.sourceTitle) << '\n'
           << "cursor " << session.cursor << '\n'
           << "position " << std::setprecision(17)
           << (std::isfinite(session.positionSeconds) && session.positionSeconds >= 0
                   ? session.positionSeconds
                   : 0)
           << '\n'
           << "shuffle " << session.shuffle << '\n'
           << "repeat "
           << (session.repeatMode >= 0 && session.repeatMode <= 2 ? session.repeatMode : 0) << '\n'
           << "screen " << std::quoted(session.screen == "now-playing" ? "now-playing" : "home")
           << '\n';
    for (const auto& value : session.sourcePaths) stream << "source " << std::quoted(value) << '\n';
    for (const auto& value : session.orderPaths) stream << "order " << std::quoted(value) << '\n';
    for (const auto& value : session.historyPaths)
        stream << "history " << std::quoted(value) << '\n';
    return atomicWriteFile(path, stream.str());
}
