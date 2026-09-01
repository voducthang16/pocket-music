#pragma once
#include <filesystem>
int runApplication(const std::filesystem::path& musicPath, const std::filesystem::path& statePath,
                   const std::filesystem::path& preferencesPath, bool fullscreen);
