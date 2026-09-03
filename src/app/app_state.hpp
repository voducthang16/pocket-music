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

enum class Screen { Home, Songs, NowPlaying, About };
enum class ViewAction {
    None,
    OpenSongs,
    OpenNowPlaying,
    OpenAbout,
    CheckForUpdates,
    InstallUpdate
};

enum class UpdatePhase {
    Idle,
    Checking,
    Downloading,
    Verifying,
    UpToDate,
    Ready,
    PreparingInstall,
    Result,
    Error
};

struct UpdateState {
    UpdatePhase phase = UpdatePhase::Idle;
    int processId = -1;
    std::string version;
    std::string detail;

    bool checking() const {
        return phase == UpdatePhase::Checking || phase == UpdatePhase::Downloading ||
               phase == UpdatePhase::Verifying;
    }

    bool preparingInstall() const { return phase == UpdatePhase::PreparingInstall; }
    bool modalVisible() const { return checking() || preparingInstall(); }
    bool cancellable() const { return checking(); }
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
    ThemePalette theme = resolveTheme();
    float vinylAngle = 0.0f;
    Uint64 vinylLastTick = 0;
    ViewState view;
    std::vector<ViewState> history;
    bool running = true;
    bool exitConfirmationOpen = false;
    int exitConfirmationSelection = 0;
    std::string message;
    UpdateState update;
    std::map<std::string, SDL_Texture*> coverCache;
    std::vector<SDL_GameController*> controllers;
    explicit AppState(std::filesystem::path path, std::unique_ptr<AudioPlayer> audioPlayer =
                                                      std::make_unique<FfmpegSdlPlayer>())
        : library(std::move(path)), playback(library, std::move(audioPlayer)) {}
};
