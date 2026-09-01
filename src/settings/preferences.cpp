#include "settings/preferences.hpp"

#include <fstream>
#include <string>

#include "core/atomic_file.hpp"

namespace {
const char* themeName(ThemeMode mode) { return mode == ThemeMode::Light ? "light" : "dark"; }

}  // namespace

AppPreferences loadPreferences(const std::filesystem::path& path) {
    AppPreferences preferences;
    std::ifstream stream(path);
    std::string key;
    std::string value;
    while (stream >> key >> value)
        if (key == "theme")
            preferences.theme = value == "light" ? ThemeMode::Light : ThemeMode::Dark;
    return preferences;
}

bool savePreferences(const std::filesystem::path& path, const AppPreferences& preferences) {
    return atomicWriteFile(path, std::string("theme ") + themeName(preferences.theme) + '\n');
}
