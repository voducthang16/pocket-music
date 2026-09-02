#include "ui/renderer.hpp"

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void renderApp(AppState& app) {
    fillRect(app.renderer, {0, 0, layout::width, layout::height}, app.theme.background);
    if (app.view.screen == Screen::Library)
        drawLibraryScreen(app);
    else if (app.view.screen == Screen::NowPlaying)
        drawNowPlayingScreen(app);
    else
        drawListScreen(app);
    if (!app.message.empty()) {
        const SDL_Rect banner{layout::pagePadding, layout::height - 116,
                              layout::width - layout::pagePadding * 2, 48};
        fillRect(app.renderer, banner, app.theme.surfaceRaised);
        drawText(app.renderer, app.smallFont, app.message, banner.x + 18, banner.y + 10,
                 app.theme.accent, banner.w - 36);
    }
    drawButtonHints(app);
    SDL_RenderPresent(app.renderer);
}
