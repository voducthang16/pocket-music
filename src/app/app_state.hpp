#pragma once
#include <SDL.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/library.hpp"
#include "core/player.hpp"
#include "core/state.hpp"

enum class Screen { Menu, Songs, Albums, Artists, Playlists, Filtered, NowPlaying };

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* bodyFont = nullptr;
    TTF_Font* smallFont = nullptr;
    MusicLibrary library;
    MpvPlayer player;
    SavedState saved;
    Screen screen = Screen::Menu;
    Screen previous = Screen::Menu;
    int selected = 0, scroll = 0, currentTrack = -1;
    bool running = true, shuffle = false, trackEnded = false;
    int repeatMode = 0;
    std::vector<size_t> filtered;
    std::string filteredTitle;
    std::map<std::string, SDL_Texture*> coverCache;
    explicit AppState(std::filesystem::path path) : library(std::move(path)) {}
};
