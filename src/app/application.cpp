#include "app/application.hpp"

#include <SDL_image.h>

#include <cstdlib>
#include <iostream>

#include "app/app_state.hpp"
#include "app/navigation.hpp"
#include "ui/input.hpp"
#include "ui/layout.hpp"
#include "ui/renderer.hpp"

namespace {
std::filesystem::path fontPath() {
    if (const char* custom = std::getenv("POCKET_MUSIC_FONT")) return custom;
#ifdef __APPLE__
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

bool initializeUi(AppState& app) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0 ||
        TTF_Init() != 0 ||
        (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) & (IMG_INIT_JPG | IMG_INIT_PNG)) == 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    app.window = SDL_CreateWindow("Pocket Music", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  480, 640, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    app.renderer =
        SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (app.renderer) SDL_RenderSetLogicalSize(app.renderer, layout::width, layout::height);
    const auto font = fontPath();
    app.titleFont = TTF_OpenFont(font.c_str(), 42);
    app.bodyFont = TTF_OpenFont(font.c_str(), 38);
    app.smallFont = TTF_OpenFont(font.c_str(), 26);
    if (!app.window || !app.renderer || !app.titleFont || !app.bodyFont || !app.smallFont) {
        std::cerr << "Could not create UI: " << SDL_GetError() << ' ' << TTF_GetError() << '\n';
        return false;
    }
    return true;
}

void restore(AppState& app, const std::filesystem::path& path) {
    app.saved = loadState(path);
    app.shuffle = app.saved.shuffle;
    app.repeatMode = app.saved.repeatMode;
    for (size_t i = 0; i < app.library.tracks().size(); ++i)
        if (app.library.tracks()[i].path.string() == app.saved.trackPath) {
            playTrack(app, i, app.saved.positionSeconds);
            if (app.saved.screen != "now-playing") app.screen = Screen::Menu;
            break;
        }
}

void persist(AppState& app, const std::filesystem::path& path) {
    if (app.currentTrack >= 0) {
        app.saved.trackPath = app.library.tracks()[app.currentTrack].path.string();
        app.saved.positionSeconds = app.player.elapsedSeconds();
    }
    app.saved.shuffle = app.shuffle;
    app.saved.repeatMode = app.repeatMode;
    app.saved.screen = app.screen == Screen::NowPlaying ? "now-playing" : "menu";
    saveState(path, app.saved);
}

void destroyUi(AppState& app) {
    for (auto& [_, texture] : app.coverCache)
        if (texture) SDL_DestroyTexture(texture);
    if (app.smallFont) TTF_CloseFont(app.smallFont);
    if (app.bodyFont) TTF_CloseFont(app.bodyFont);
    if (app.titleFont) TTF_CloseFont(app.titleFont);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window) SDL_DestroyWindow(app.window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}
}  // namespace

int runApplication(const std::filesystem::path& musicPath, const std::filesystem::path& statePath) {
    AppState app(musicPath);
    app.library.scan();
    if (!initializeUi(app)) {
        destroyUi(app);
        return 1;
    }
    restore(app, statePath);
    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                app.running = false;
            else if (event.type == SDL_KEYDOWN && !event.key.repeat)
                handleKey(app, event.key.keysym.sym);
            else if (event.type == SDL_MOUSEBUTTONDOWN)
                handleMouse(app, event.button);
            else if (event.type == SDL_CONTROLLERBUTTONDOWN)
                handleControllerButton(app, event.cbutton.button);
        }
        advanceWhenFinished(app);
        renderApp(app);
        SDL_Delay(16);
    }
    persist(app, statePath);
    destroyUi(app);
    return 0;
}
