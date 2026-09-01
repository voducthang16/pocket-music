#include <filesystem>
#include <iostream>
#include <string>

#include "app/application.hpp"
#include "core/library.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path music = "Music", state = ".pocket-music-state";
    fs::path preferences = ".pocket-music-preferences";
    bool smoke = false, fullscreen = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--smoke-test")
            smoke = true;
        else if (argument == "--fullscreen")
            fullscreen = true;
        else if (argument == "--music" && i + 1 < argc)
            music = argv[++i];
        else if (argument == "--state" && i + 1 < argc)
            state = argv[++i];
        else if (argument == "--preferences" && i + 1 < argc)
            preferences = argv[++i];
    }
    if (smoke) {
        MusicLibrary library(music);
        if (!library.scan()) {
            std::cerr << library.error() << '\n';
            return 1;
        }
        std::cout << "Scanned " << library.tracks().size() << " tracks\n";
        return 0;
    }
    return runApplication(music, state, preferences, fullscreen);
}
