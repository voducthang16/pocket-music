#include "ui/renderer.hpp"

#include <cmath>

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"
#include "ui/vinyl.hpp"

namespace {
void drawUpdateSpinner(AppState& app, int centerX, int centerY, Uint64 now) {
    constexpr int segments = 12;
    constexpr float pi = 3.14159265358979323846f;
    const int head = static_cast<int>((now / 85) % segments);
    for (int index = 0; index < segments; ++index) {
        const float angle = (static_cast<float>(index) / segments) * 2.0f * pi;
        const int distance = (index - head + segments) % segments;
        SDL_Color color = app.theme.accent;
        color.a = static_cast<Uint8>(235 - distance * 14);
        SDL_SetRenderDrawColor(app.renderer, color.r, color.g, color.b, color.a);
        const int x1 = centerX + static_cast<int>(std::cos(angle) * 15.0f);
        const int y1 = centerY + static_cast<int>(std::sin(angle) * 15.0f);
        const int x2 = centerX + static_cast<int>(std::cos(angle) * 25.0f);
        const int y2 = centerY + static_cast<int>(std::sin(angle) * 25.0f);
        SDL_RenderDrawLine(app.renderer, x1, y1, x2, y2);
    }
}

void drawUpdateModal(AppState& app, Uint64 now) {
    if (!app.update.modalVisible()) return;

    SDL_Color dim = app.theme.text;
    dim.a = 44;
    fillRect(app.renderer, {0, 0, layout::width, layout::height}, dim);

    const SDL_Rect card{252, 270, 520, 214};
    SDL_Color surface = app.theme.surfaceRaised;
    surface.a = 248;
    fillRoundedRect(app.renderer, card, 18, surface);

    const int centerX = card.x + card.w / 2;
    drawUpdateSpinner(app, centerX, card.y + 55, now);

    const bool installing = app.update.preparingInstall();
    const std::string title = installing ? "Installing Update" : "Checking for Updates";
    drawText(app.renderer, app.bodyFont, title, centerX, card.y + 91, app.theme.text, card.w - 64,
             true);
    drawText(app.renderer, app.smallFont, app.update.detail, centerX, card.y + 139,
             app.theme.accent, card.w - 64, true);
    drawText(app.renderer, app.smallFont,
             installing ? "Please don't power off" : "B / Back to cancel", centerX,
             card.y + 172, app.theme.textMuted, card.w - 64, true);
}

std::string updateResultMessage(const AppState& app) {
    if (app.update.modalVisible() || app.update.phase == UpdatePhase::Idle) return {};
    return app.update.detail;
}
}  // namespace

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

    const std::string updateMessage = updateResultMessage(app);
    const std::string& bannerMessage = updateMessage.empty() ? app.message : updateMessage;
    if (!bannerMessage.empty() && app.view.screen != Screen::NowPlaying && !app.update.modalVisible()) {
        const SDL_Rect banner{layout::messageBannerX, layout::messageBannerY,
                              layout::messageBannerWidth, layout::messageBannerHeight};
        SDL_Color surface = app.theme.surfaceRaised;
        surface.a = 220;
        fillRoundedRect(app.renderer, banner, 12, surface);
        drawText(app.renderer, app.smallFont, bannerMessage, banner.x + 18, banner.y + 10,
                 app.theme.accent, banner.w - 36);
    }
    if (app.exitConfirmationOpen) drawExitConfirmation(app);
    drawButtonHints(app);
    drawUpdateModal(app, now);
    SDL_RenderPresent(app.renderer);
}
