#include "ui/renderer.hpp"

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"
#include "ui/vinyl.hpp"

void renderApp(AppState& app) {
    const Uint64 now = SDL_GetTicks64();
    const Uint64 elapsed = app.vinylLastTick == 0 ? 0 : now - app.vinylLastTick;
    app.vinylLastTick = now;
    const auto phase = app.playback.snapshot().phase;
    const bool loading = phase == PlaybackPhase::Loading;
    app.vinylAngle = advanceVinylAngle(app.vinylAngle, elapsed,
                                       loading || phase == PlaybackPhase::Playing, loading);
    fillRect(app.renderer, {0, 0, layout::width, layout::height}, app.theme.background);
    if (app.backgroundTexture)
        SDL_RenderCopy(app.renderer, app.backgroundTexture, nullptr, nullptr);
    if (app.view.screen == Screen::Home)
        drawHomeScreen(app);
    else if (app.view.screen == Screen::NowPlaying)
        drawNowPlayingScreen(app);
    else if (app.view.screen == Screen::LinerNotes)
        drawLinerNotesScreen(app);
    else
        drawSongsScreen(app);
    if (!app.message.empty() && app.view.screen != Screen::NowPlaying) {
        const SDL_Rect banner{layout::messageBannerX, layout::messageBannerY,
                              layout::messageBannerWidth, layout::messageBannerHeight};
        SDL_Color surface = app.theme.surfaceRaised;
        surface.a = 220;
        fillRoundedRect(app.renderer, banner, 12, surface);
        drawText(app.renderer, app.smallFont, app.message, banner.x + 18, banner.y + 10,
                 app.theme.accent, banner.w - 36);
    }
    drawButtonHints(app);
    SDL_RenderPresent(app.renderer);
}
