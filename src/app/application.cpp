#include "app/application.hpp"

#include <SDL_image.h>

#include <cstdlib>
#include <iostream>

#include "app/app_state.hpp"
#include "app/navigation.hpp"
#include "ui/input.hpp"
#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer.hpp"

namespace {
std::filesystem::path fontPath() {
    if (const char* custom = std::getenv("POCKET_MUSIC_FONT")) return custom;
    const std::filesystem::path relative = "assets/fonts/NotoSans-Regular.ttf";
    if (std::filesystem::exists(relative)) return relative;
    if (char* rawBase = SDL_GetBasePath()) {
        const std::filesystem::path base(rawBase);
        SDL_free(rawBase);
        for (const auto& candidate : {base / relative, base.parent_path() / relative})
            if (std::filesystem::exists(candidate)) return candidate;
    }
#ifdef __APPLE__
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

bool initializeUi(AppState& app, bool fullscreen) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0 ||
        TTF_Init() != 0 ||
        (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) & (IMG_INIT_JPG | IMG_INIT_PNG)) !=
            (IMG_INIT_JPG | IMG_INIT_PNG)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    const Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI |
                               (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);
    app.window = SDL_CreateWindow("Pocket Music", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  384, 512, windowFlags);
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
    TTF_SetFontHinting(app.titleFont, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(app.bodyFont, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(app.smallFont, TTF_HINTING_LIGHT);
    for (int index = 0; index < SDL_NumJoysticks(); ++index)
        if (SDL_IsGameController(index))
            if (auto* controller = SDL_GameControllerOpen(index))
                app.controllers.push_back(controller);
    return true;
}

void restore(AppState& app, const std::filesystem::path& path) {
    app.saved = loadState(path);
    app.shuffle = app.saved.shuffle;
    app.repeatMode = app.saved.repeatMode;
    for (size_t i = 0; i < app.library.tracks().size(); ++i)
        if (app.library.tracks()[i].path.string() == app.saved.trackPath) {
            if (playTrack(app, i, app.library.allTrackIndexes(), app.saved.positionSeconds) &&
                app.saved.screen != "now-playing")
                buildLibraryView(app);
            break;
        }
}

void persist(AppState& app, const std::filesystem::path& path) {
    if (app.currentTrack >= 0) {
        app.saved.trackPath = app.library.tracks()[app.currentTrack].path.string();
        app.saved.positionSeconds = static_cast<int>(app.player->snapshot().positionSeconds);
    }
    app.saved.shuffle = app.shuffle;
    app.saved.repeatMode = app.repeatMode;
    app.saved.screen = app.view.screen == Screen::NowPlaying ? "now-playing" : "library";
    if (!saveState(path, app.saved)) std::cerr << "Could not save playback state\n";
}

void destroyUi(AppState& app) {
    for (auto* controller : app.controllers) SDL_GameControllerClose(controller);
    for (auto& [_, texture] : app.coverCache)
        if (texture) SDL_DestroyTexture(texture);
    clearTextCache();
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

int runApplication(const std::filesystem::path& musicPath, const std::filesystem::path& statePath,
                   const std::filesystem::path& preferencesPath, bool fullscreen) {
    AppState app(musicPath);
    app.preferences = loadPreferences(preferencesPath);
    app.theme = resolveTheme(app.preferences.theme);
    if (!app.library.scan()) {
        std::cerr << "Music scan failed: " << app.library.error() << '\n';
        app.message = app.library.error();
    }
    buildLibraryView(app);
    if (!initializeUi(app, fullscreen)) {
        destroyUi(app);
        return 1;
    }
    restore(app, statePath);
    int heldButton = -1;
    Uint64 nextControllerRepeat = 0;
    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                app.running = false;
            else if (event.type == SDL_KEYDOWN &&
                     (!event.key.repeat || event.key.keysym.sym == SDLK_UP ||
                      event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_LEFT ||
                      event.key.keysym.sym == SDLK_RIGHT))
                handleKey(app, event.key.keysym.sym);
            else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                handleControllerButton(app, event.cbutton.button);
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
                    event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
                    event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                    event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    heldButton = event.cbutton.button;
                    nextControllerRepeat = SDL_GetTicks64() + 350;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONUP && event.cbutton.button == heldButton) {
                heldButton = -1;
            }
        }
        if (heldButton >= 0 && SDL_GetTicks64() >= nextControllerRepeat) {
            handleControllerButton(app, static_cast<Uint8>(heldButton));
            nextControllerRepeat = SDL_GetTicks64() + 90;
        }
        advanceWhenFinished(app);
        renderApp(app);
        SDL_Delay(16);
    }
    persist(app, statePath);
    if (!savePreferences(preferencesPath, app.preferences))
        std::cerr << "Could not save app preferences\n";
    destroyUi(app);
    return 0;
}
