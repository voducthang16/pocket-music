#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/ffmpeg_sdl_player.hpp"
#include "core/library.hpp"
#include "core/playback_controller.hpp"
#include "ui/theme.hpp"

enum class Screen { Home, Songs, NowPlaying, LinerNotes };
enum class ViewAction { None, OpenSongs, OpenNowPlaying, OpenLinerNotes, CheckForUpdates };

struct ViewItem {
    std::string title;
    std::string subtitle;
    ViewAction action = ViewAction::None;
    std::optional<size_t> trackIndex;
};

struct ViewState {
    Screen screen = Screen::Home;
    std::string title = "Home";
    std::vector<ViewItem> items;
    int selected = 0;
    int scroll = 0;
};

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* backgroundTexture = nullptr;
    SDL_Texture* fallbackVinylTexture = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* bodyFont = nullptr;
    TTF_Font* smallFont = nullptr;
    MusicLibrary library;
    PlaybackController playback;
    ThemePalette theme = resolveTheme();
    float vinylAngle = 0.0f;
    Uint64 vinylLastTick = 0;
    ViewState view;
    std::vector<ViewState> history;
    bool running = true;
    bool exitConfirmationOpen = false;
    int exitConfirmationSelection = 0;
    std::string message;
    std::map<std::string, SDL_Texture*> coverCache;
    std::vector<SDL_GameController*> controllers;
    explicit AppState(std::filesystem::path path, std::unique_ptr<AudioPlayer> audioPlayer =
                                                      std::make_unique<FfmpegSdlPlayer>())
        : library(std::move(path)), playback(library, std::move(audioPlayer)) {}
};
