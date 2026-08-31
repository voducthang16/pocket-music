#include <SDL.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "app/application.hpp"
#include "core/library.hpp"
#include "core/player.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path music = "Music", state = ".classic-ipod-state";
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--audio-test" && i + 1 < argc) {
            SDL_Init(SDL_INIT_TIMER);
            MpvPlayer player;
            if (!player.load(argv[++i])) {
                std::cerr << "Audio test failed: " << player.error() << '\n';
                return 1;
            }
            std::cout << "Playing audio test for 4 seconds: " << argv[i] << '\n';
            SDL_Delay(4000);
            SDL_Quit();
            return 0;
        }
        if (argument == "--smoke-test")
            smoke = true;
        else if (argument == "--music" && i + 1 < argc)
            music = argv[++i];
        else if (argument == "--state" && i + 1 < argc)
            state = argv[++i];
    }
    if (smoke) {
        MusicLibrary library(music);
        library.scan();
        std::cout << "Scanned " << library.tracks().size() << " tracks\n";
        return 0;
    }
    return runApplication(music, state);
}
