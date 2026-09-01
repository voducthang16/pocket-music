#pragma once

#include <filesystem>
#include <string>

bool atomicWriteFile(const std::filesystem::path& path, const std::string& content);
