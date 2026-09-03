#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ffmpeg_sdl_player.hpp"
#include "core/library.hpp"
#include "core/playback_controller.hpp"
#include "ui/theme.hpp"
#include "update/update_controller.hpp"

enum class Screen { Home, Songs, NowPlaying, About };
enum class ViewAction {
    None,
    OpenSongs,
    OpenNowPlaying,
    OpenAbout,
    CheckForUpdates,
    InstallUpdate
};

enum class NoticeSource { Application, Playback, Update };

struct AppNotice {
    NoticeSource source = NoticeSource::Application;
    std::string text;
};

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
    UpdateController updates;
    ThemePalette theme = resolveTheme();
    float vinylAngle = 0.0f;
    Uint64 vinylLastTick = 0;
    ViewState view;
    std::vector<ViewState> history;
    bool running = true;
    bool exitConfirmationOpen = false;
    int exitConfirmationSelection = 0;
    std::optional<AppNotice> notice;
    std::map<std::string, SDL_Texture*> coverCache;
    std::vector<SDL_GameController*> controllers;

    explicit AppState(
        std::filesystem::path path,
        std::unique_ptr<AudioPlayer> audioPlayer = std::make_unique<FfmpegSdlPlayer>(),
        UpdateRuntimePaths updatePaths = {})
        : library(std::move(path)),
          playback(library, std::move(audioPlayer)),
          updates(std::move(updatePaths)) {}
};
