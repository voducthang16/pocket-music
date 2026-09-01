#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/library.hpp"
#include "core/playback_controller.hpp"
#include "core/state.hpp"
#include "settings/preferences.hpp"
#include "ui/theme.hpp"

enum class Screen { Library, Albums, Artists, Playlists, Tracks, NowPlaying, Settings };
enum class ViewAction {
    None,
    OpenSongs,
    OpenAlbums,
    OpenArtists,
    OpenPlaylists,
    OpenNowPlaying,
    OpenSettings,
    ToggleTheme
};

struct ViewItem {
    std::string title;
    std::string subtitle;
    ViewAction action = ViewAction::None;
    std::vector<size_t> trackIndexes;
};

struct ViewState {
    Screen screen = Screen::Library;
    std::string title = "Library";
    std::string eyebrow;
    std::vector<ViewItem> items;
    int selected = 0;
    int scroll = 0;
};

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* bodyFont = nullptr;
    TTF_Font* smallFont = nullptr;
    MusicLibrary library;
    PlaybackController playback;
    PlaybackSession session;
    AppPreferences preferences;
    ThemePalette theme = resolveTheme(ThemeMode::Dark);
    ViewState view;
    std::vector<ViewState> history;
    bool running = true;
    std::string message;
    std::map<std::string, SDL_Texture*> coverCache;
    std::vector<SDL_GameController*> controllers;
    explicit AppState(std::filesystem::path path,
                      std::unique_ptr<AudioPlayer> audioPlayer = std::make_unique<MpvPlayer>())
        : library(std::move(path)), playback(library, std::move(audioPlayer)) {}
};
