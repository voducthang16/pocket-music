#include "app/application.hpp"

#include <SDL_image.h>

#include <cstdlib>
#include <iostream>

#include "app/app_state.hpp"
#include "app/navigation.hpp"
#include "core/playback_session.hpp"
#include "ui/input.hpp"
#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer.hpp"

namespace {
std::filesystem::path resourcePath(const std::filesystem::path& relative) {
    if (std::filesystem::exists(relative)) return relative;
    if (char* rawBase = SDL_GetBasePath()) {
        const std::filesystem::path base(rawBase);
        SDL_free(rawBase);
        for (const auto& candidate : {base / relative, base.parent_path() / relative})
            if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

std::filesystem::path fontPath() {
    if (const char* custom = std::getenv("POCKET_MUSIC_FONT")) return custom;
    if (const auto bundled = resourcePath("assets/fonts/NotoSans-Regular.ttf"); !bundled.empty())
        return bundled;
#ifdef __APPLE__
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

SDL_Texture* loadResourceTexture(SDL_Renderer* renderer, const std::filesystem::path& relative) {
    const auto path = resourcePath(relative);
    return path.empty() ? nullptr : IMG_LoadTexture(renderer, path.c_str());
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
                                  1024, 768, windowFlags);
    app.renderer =
        SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (app.renderer) {
        SDL_RenderSetLogicalSize(app.renderer, layout::width, layout::height);
        SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
        app.backgroundTexture =
            loadResourceTexture(app.renderer, "assets/background-hello-kitty-v6.png");
        app.fallbackVinylTexture = loadResourceTexture(app.renderer, "assets/fallback-vinyl.png");
    }
    const auto font = fontPath();
    app.titleFont = TTF_OpenFont(font.c_str(), 36);
    app.bodyFont = TTF_OpenFont(font.c_str(), 30);
    app.smallFont = TTF_OpenFont(font.c_str(), 22);
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
    const auto session = loadSession(path);
    app.playback.setRepeatMode(static_cast<RepeatMode>(session.repeatMode));
    auto resolved = resolvePlaybackSession(session, app.library);
    if (!resolved) return;
    if (app.playback.restore(std::move(resolved->source), std::move(resolved->order),
                             std::move(resolved->history), resolved->cursor, session.shuffle,
                             session.positionSeconds, session.sourceTitle) &&
        session.screen == "now-playing")
        app.view = nowPlayingView();
}

void persist(AppState& app, const std::filesystem::path& path) {
    const auto session =
        capturePlaybackSession(app.playback, app.library, app.view.screen == Screen::NowPlaying);
    if (!saveSession(path, session)) std::cerr << "Could not save playback state\n";
}

void destroyUi(AppState& app) {
    for (auto* controller : app.controllers) SDL_GameControllerClose(controller);
    for (auto& [_, texture] : app.coverCache)
        if (texture) SDL_DestroyTexture(texture);
    clearTextCache();
    if (app.fallbackVinylTexture) SDL_DestroyTexture(app.fallbackVinylTexture);
    if (app.backgroundTexture) SDL_DestroyTexture(app.backgroundTexture);
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
                   bool fullscreen) {
    AppState app(musicPath);
    if (!app.library.scan()) {
        std::cerr << "Music scan failed: " << app.library.error() << '\n';
        app.message = app.library.error();
    }
    buildHomeView(app);
    if (!initializeUi(app, fullscreen)) {
        app.playback.shutdown();
        destroyUi(app);
        return 1;
    }
    restore(app, statePath);
    uint64_t savedRevision = app.playback.revision();
    Uint64 lastSessionSave = SDL_GetTicks64();
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
        const Uint64 now = SDL_GetTicks64();
        const bool periodicSave =
            app.playback.snapshot().trackIndex && now - lastSessionSave >= 15000;
        if (app.playback.revision() != savedRevision || periodicSave) {
            persist(app, statePath);
            savedRevision = app.playback.revision();
            lastSessionSave = now;
        }
        renderApp(app);
        SDL_Delay(16);
    }
    persist(app, statePath);
    app.playback.shutdown();
    destroyUi(app);
    return 0;
}
