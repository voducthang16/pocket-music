#pragma once
#include <filesystem>

enum class ThemeMode { Dark, Light };
struct AppPreferences {
    ThemeMode theme = ThemeMode::Dark;
};

AppPreferences loadPreferences(const std::filesystem::path& path);
bool savePreferences(const std::filesystem::path& path, const AppPreferences& preferences);
