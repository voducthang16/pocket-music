#include "ui/renderer.hpp"

#include <SDL_image.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "app/navigation.hpp"
#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/theme.hpp"

namespace {
std::string duration(int seconds) {
    std::ostringstream stream;
    stream << seconds / 60 << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
    return stream.str();
}

void drawHeader(AppState& app) {
    verticalGradient(app.renderer, {0, 0, layout::width, layout::headerHeight}, theme::headerTop,
                     theme::headerBottom);
    fillRect(app.renderer, {0, layout::headerHeight - 2, layout::width, 2}, {153, 158, 166, 255});
    drawText(app.renderer, app.titleFont, screenHeading(app), layout::width / 2, 12, theme::text,
             520, true);
    if (app.currentTrack >= 0) {
        drawPlayState(app.renderer, 27, 27, app.player.paused(), theme::textMuted);
    }
    drawBattery(app.renderer, layout::width - 66, 27, 100);
}

void drawEmptyState(AppState& app) {
    fillRect(app.renderer, {layout::width / 2 - 68, 250, 136, 136}, {225, 228, 233, 255});
    drawPlayState(app.renderer, layout::width / 2 - 18, 300, false, theme::textMuted);
    drawText(app.renderer, app.bodyFont, "No music found", layout::width / 2, 430, theme::text, 600,
             true);
    drawText(app.renderer, app.smallFont, "Copy songs into the Music folder", layout::width / 2,
             490, theme::textMuted, 650, true);
}

void drawList(AppState& app) {
    const auto items = visibleLabels(app);
    if (items.empty()) {
        drawEmptyState(app);
        return;
    }
    const int visible = (layout::height - layout::headerHeight) / layout::rowHeight;
    if (app.selected < app.scroll) app.scroll = app.selected;
    if (app.selected >= app.scroll + visible) app.scroll = app.selected - visible + 1;
    for (int row = 0; row < visible && app.scroll + row < static_cast<int>(items.size()); ++row) {
        const int index = app.scroll + row;
        const int y = layout::headerHeight + row * layout::rowHeight;
        const bool active = index == app.selected;
        if (active) {
            verticalGradient(app.renderer, {0, y, layout::width, layout::rowHeight}, theme::blueTop,
                             theme::blueBottom);
        } else {
            fillRect(app.renderer, {0, y, layout::width, layout::rowHeight}, theme::surface);
        }
        drawText(app.renderer, app.bodyFont, items[index], 30, y + 18,
                 active ? theme::white : theme::text, layout::width - 105);
        drawChevron(app.renderer, layout::width - 43, y + layout::rowHeight / 2,
                    active ? theme::white : theme::textMuted);
        fillRect(app.renderer, {0, y + layout::rowHeight - 1, layout::width, 1},
                 active ? theme::blueBottom : theme::divider);
    }

    if (items.size() > static_cast<size_t>(visible)) {
        const int trackHeight = layout::height - layout::headerHeight - 24;
        const int thumbHeight =
            std::max(48, trackHeight * visible / static_cast<int>(items.size()));
        const int maxScroll = static_cast<int>(items.size()) - visible;
        const int thumbY = layout::headerHeight + 12 +
                           (trackHeight - thumbHeight) * app.scroll / std::max(1, maxScroll);
        fillRect(app.renderer, {layout::width - 8, thumbY, 4, thumbHeight}, {140, 145, 155, 180});
    }
}

SDL_Texture* albumCover(AppState& app, const Track& track) {
    if (track.coverPath.empty()) return nullptr;
    const auto key = track.coverPath.string();
    if (app.coverCache.contains(key)) return app.coverCache[key];
    return app.coverCache[key] = IMG_LoadTexture(app.renderer, key.c_str());
}

void drawPlaceholderCover(AppState& app, const SDL_Rect& art) {
    verticalGradient(app.renderer, art, {231, 233, 238, 255}, {189, 194, 203, 255});
    fillRect(app.renderer, {art.x + 118, art.y + 118, art.w - 236, art.h - 236}, {54, 60, 70, 255});
    fillRect(app.renderer, {art.x + art.w / 2 - 28, art.y + art.h / 2 - 28, 56, 56},
             theme::background);
    drawText(app.renderer, app.smallFont, "CLASSIC IPOD", art.x + art.w / 2, art.y + art.h - 62,
             theme::textMuted, art.w - 80, true);
}

void drawNowPlaying(AppState& app) {
    if (app.currentTrack < 0 || app.currentTrack >= static_cast<int>(app.library.tracks().size())) {
        drawEmptyState(app);
        return;
    }
    const auto& track = app.library.tracks()[app.currentTrack];
    const SDL_Rect shadow{layout::width / 2 - 224, 112, 448, 448};
    fillRect(app.renderer, shadow, {181, 184, 190, 255});
    const SDL_Rect art{shadow.x + 5, shadow.y + 5, shadow.w - 10, shadow.h - 10};
    if (SDL_Texture* texture = albumCover(app, track))
        SDL_RenderCopy(app.renderer, texture, nullptr, &art);
    else
        drawPlaceholderCover(app, art);

    drawMarqueeText(app.renderer, app.titleFont, track.title, {52, 598, layout::width - 104, 56},
                    theme::text, SDL_GetTicks64());
    drawText(app.renderer, app.bodyFont, track.artist, layout::width / 2, 666, theme::textMuted,
             layout::width - 110, true);
    drawText(app.renderer, app.smallFont, track.album, layout::width / 2, 719, theme::textMuted,
             layout::width - 130, true);

    const int elapsed = std::max(0, app.player.elapsedSeconds());
    const int total = std::max(1, track.durationSeconds);
    const int progressWidth = layout::width - 110;
    const int filled = std::min(progressWidth, progressWidth * elapsed / total);
    fillRect(app.renderer, {55, 805, progressWidth, 9}, {197, 201, 208, 255});
    verticalGradient(app.renderer, {55, 805, filled, 9}, theme::blueTop, theme::blueBottom);
    fillRect(app.renderer, {55 + std::max(0, filled - 4), 799, 9, 21}, theme::white);
    strokeRect(app.renderer, {55 + std::max(0, filled - 4), 799, 9, 21}, theme::textMuted);
    drawText(app.renderer, app.smallFont, duration(elapsed), 55, 833, theme::textMuted);
    drawText(app.renderer, app.smallFont, duration(track.durationSeconds), layout::width - 126, 833,
             theme::textMuted);

    const char* state = app.player.paused() ? "PAUSED" : "PLAYING";
    drawText(app.renderer, app.smallFont, state, layout::width / 2, 886, theme::blue, 200, true);
    std::string modes =
        std::string(app.shuffle ? "SHUFFLE  " : "") + (app.repeatMode == 1   ? "REPEAT ONE"
                                                       : app.repeatMode == 2 ? "REPEAT ALL"
                                                                             : "");
    drawText(app.renderer, app.smallFont, modes, layout::width / 2, 929, theme::textMuted, 500,
             true);
}
}  // namespace

void renderApp(AppState& app) {
    fillRect(app.renderer, {0, 0, layout::width, layout::height}, theme::background);
    drawHeader(app);
    if (app.screen == Screen::NowPlaying)
        drawNowPlaying(app);
    else
        drawList(app);
    SDL_RenderPresent(app.renderer);
}
