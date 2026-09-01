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
constexpr int kPagePadding = 40;
constexpr int kTopBarHeight = 102;
constexpr int kLibraryRowHeight = 108;
constexpr int kTrackRowHeight = 94;

std::string formatDuration(int seconds) {
    std::ostringstream stream;
    stream << seconds / 60 << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
    return stream.str();
}

std::string countLabel(size_t count, const char* singular, const char* plural) {
    return std::to_string(count) + ' ' + (count == 1 ? singular : plural);
}

void drawTopBar(AppState& app, const std::string& title, const std::string& eyebrow = {}) {
    fillRect(app.renderer, {0, 0, layout::width, kTopBarHeight}, app.theme.background);
    if (!eyebrow.empty()) {
        drawText(app.renderer, app.smallFont, eyebrow, kPagePadding, 20, app.theme.textMuted,
                 layout::width - kPagePadding * 2);
    }
    drawText(app.renderer, app.titleFont, title, kPagePadding, eyebrow.empty() ? 25 : 47,
             app.theme.text, layout::width - kPagePadding * 2);
    fillRect(app.renderer, {kPagePadding, kTopBarHeight - 1, layout::width - kPagePadding * 2, 1},
             app.theme.divider);
}

SDL_Texture* albumCover(AppState& app, const Track& track) {
    if (track.coverPath.empty()) return nullptr;
    const auto key = track.coverPath.string();
    if (app.coverCache.contains(key)) return app.coverCache[key];
    return app.coverCache[key] = IMG_LoadTexture(app.renderer, key.c_str());
}

void drawCover(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect& bounds) {
    int width = 0, height = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &width, &height) != 0 || width <= 0 ||
        height <= 0)
        return;
    SDL_Rect target = bounds;
    if (width * bounds.h > height * bounds.w) {
        target.h = bounds.w * height / width;
        target.y += (bounds.h - target.h) / 2;
    } else {
        target.w = bounds.h * width / height;
        target.x += (bounds.w - target.w) / 2;
    }
    SDL_RenderCopy(renderer, texture, nullptr, &target);
}

void drawCoverPlaceholder(AppState& app, const SDL_Rect& bounds) {
    fillRect(app.renderer, bounds, app.theme.surfaceRaised);
    const int centerX = bounds.x + bounds.w / 2;
    const int centerY = bounds.y + bounds.h / 2;
    fillRect(app.renderer,
             {centerX - bounds.w / 5, centerY - bounds.h / 5, bounds.w * 2 / 5, bounds.h * 2 / 5},
             app.theme.accentSoft);
    drawPlayState(app.renderer, centerX - 9, centerY - 9, false, app.theme.accent);
}

void drawMiniPlayer(AppState& app) {
    const SDL_Rect panel{kPagePadding, layout::height - 150, layout::width - kPagePadding * 2, 118};
    fillRect(app.renderer, panel, app.theme.surfaceRaised);
    const auto playback = app.playback.snapshot();
    const auto displayTrack = app.playback.displayTrackIndex();
    if (!displayTrack) {
        drawText(app.renderer, app.bodyFont, "Choose a song to start", panel.x + 28, panel.y + 24,
                 app.theme.text, panel.w - 56);
        drawText(app.renderer, app.smallFont, "Your music stays offline", panel.x + 28,
                 panel.y + 69, app.theme.textMuted, panel.w - 56);
        return;
    }

    const auto& track = app.library.tracks()[*displayTrack];
    const SDL_Rect art{panel.x + 10, panel.y + 10, 98, 98};
    if (SDL_Texture* texture = albumCover(app, track))
        drawCover(app.renderer, texture, art);
    else
        drawCoverPlaceholder(app, art);
    drawText(app.renderer, app.bodyFont, track.title, panel.x + 130, panel.y + 20, app.theme.text,
             panel.w - 210);
    drawText(app.renderer, app.smallFont, track.artist, panel.x + 130, panel.y + 66,
             app.theme.textMuted, panel.w - 210);
    drawPlayState(app.renderer, panel.x + panel.w - 54, panel.y + 49,
                  playback.phase == PlaybackPhase::Paused, app.theme.text);
}

void drawLibrary(AppState& app) {
    drawTopBar(app, "Library",
               countLabel(app.library.tracks().size(), "song", "songs") + " available offline");
    const auto& items = app.view.items;

    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int y = kTopBarHeight + 18 + index * kLibraryRowHeight;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer,
                     {kPagePadding, y, layout::width - kPagePadding * 2, kLibraryRowHeight - 8},
                     app.theme.surfaceRaised);
            fillRect(app.renderer, {kPagePadding, y + 15, 5, kLibraryRowHeight - 38},
                     app.theme.accent);
        }
        drawText(app.renderer, app.bodyFont, items[index].title, kPagePadding + 26, y + 14,
                 app.theme.text, layout::width - 210);
        drawText(app.renderer, app.smallFont, items[index].subtitle, kPagePadding + 26, y + 59,
                 app.theme.textMuted, layout::width - 210);
        drawChevron(app.renderer, layout::width - kPagePadding - 24, y + 47,
                    active ? app.theme.accent : app.theme.textMuted);
    }
    drawMiniPlayer(app);
}

const Track* trackForListRow(const AppState& app, int index) {
    if (app.view.screen == Screen::Tracks && index >= 0 &&
        index < static_cast<int>(app.view.items.size()) &&
        !app.view.items[index].trackIndexes.empty())
        return &app.library.tracks()[app.view.items[index].trackIndexes.front()];
    return nullptr;
}

void drawEmptyState(AppState& app) {
    const SDL_Rect art{layout::width / 2 - 70, 280, 140, 140};
    drawCoverPlaceholder(app, art);
    drawText(app.renderer, app.bodyFont, "Nothing here yet", layout::width / 2, 458, app.theme.text,
             560, true);
    drawText(app.renderer, app.smallFont, "Add music to the Music folder", layout::width / 2, 510,
             app.theme.textMuted, 620, true);
}

void drawList(AppState& app) {
    const auto& items = app.view.items;
    drawTopBar(app, app.view.title, app.view.eyebrow);
    if (items.empty()) {
        drawEmptyState(app);
        return;
    }
    const int visible = (layout::height - kTopBarHeight - 20) / kTrackRowHeight;
    if (app.view.selected < app.view.scroll) app.view.scroll = app.view.selected;
    if (app.view.selected >= app.view.scroll + visible)
        app.view.scroll = app.view.selected - visible + 1;

    for (int row = 0; row < visible && app.view.scroll + row < static_cast<int>(items.size());
         ++row) {
        const int index = app.view.scroll + row;
        const int y = kTopBarHeight + 10 + row * kTrackRowHeight;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer, {24, y, layout::width - 48, kTrackRowHeight - 4},
                     app.theme.surfaceRaised);
            fillRect(app.renderer, {24, y + 13, 5, kTrackRowHeight - 30}, app.theme.accent);
        }
        drawText(app.renderer, app.bodyFont, items[index].title, 52, y + 10, app.theme.text,
                 layout::width - 135);
        if (const Track* track = trackForListRow(app, index)) {
            drawText(app.renderer, app.smallFont,
                     track->artist + "  -  " + formatDuration(track->durationSeconds), 52, y + 54,
                     app.theme.textMuted, layout::width - 135);
        }
        drawChevron(app.renderer, layout::width - 48, y + 43,
                    active ? app.theme.accent : app.theme.textMuted);
        fillRect(app.renderer, {52, y + kTrackRowHeight - 5, layout::width - 100, 1},
                 app.theme.divider);
    }
}

void drawNowPlaying(AppState& app) {
    drawTopBar(app, "Now Playing", "POCKET MUSIC");
    const auto playback = app.playback.snapshot();
    const auto displayTrack = app.playback.displayTrackIndex();
    if (!displayTrack || *displayTrack >= app.library.tracks().size()) {
        drawEmptyState(app);
        return;
    }
    const auto& track = app.library.tracks()[*displayTrack];
    const SDL_Rect art{layout::width / 2 - 220, 132, 440, 440};
    if (SDL_Texture* texture = albumCover(app, track))
        drawCover(app.renderer, texture, art);
    else
        drawCoverPlaceholder(app, art);

    drawMarqueeText(app.renderer, app.titleFont, track.title, {48, 612, layout::width - 96, 54},
                    app.theme.text, SDL_GetTicks64());
    drawText(app.renderer, app.bodyFont, track.artist, layout::width / 2, 674, app.theme.textMuted,
             layout::width - 120, true);
    drawText(app.renderer, app.smallFont, track.album, layout::width / 2, 724, app.theme.textMuted,
             layout::width - 140, true);

    const int elapsed = std::max(0, static_cast<int>(playback.positionSeconds));
    const int duration = playback.durationSeconds > 0 ? static_cast<int>(playback.durationSeconds)
                                                      : track.durationSeconds;
    const int total = std::max(1, duration);
    const int progressWidth = layout::width - 112;
    const int filled = std::min(progressWidth, progressWidth * elapsed / total);
    fillRect(app.renderer, {56, 806, progressWidth, 8}, app.theme.surfaceRaised);
    fillRect(app.renderer, {56, 806, filled, 8}, app.theme.accent);
    drawText(app.renderer, app.smallFont, formatDuration(elapsed), 56, 830, app.theme.textMuted);
    drawText(app.renderer, app.smallFont, formatDuration(duration), layout::width - 124, 830,
             app.theme.textMuted);

    const SDL_Rect control{layout::width / 2 - 48, 894, 96, 96};
    fillRect(app.renderer, control, app.theme.surfaceRaised);
    drawPlayState(app.renderer, control.x + 39, control.y + 39,
                  playback.phase == PlaybackPhase::Paused, app.theme.text);
    if (app.playback.shuffle())
        drawText(app.renderer, app.smallFont, "SHUFFLE", 56, 925, app.theme.accent, 160);
    if (app.playback.repeatMode() != RepeatMode::Off)
        drawText(app.renderer, app.smallFont,
                 app.playback.repeatMode() == RepeatMode::One ? "REPEAT 1" : "REPEAT", 555, 925,
                 app.theme.accent, 160);
    if (playback.phase == PlaybackPhase::Loading)
        drawText(app.renderer, app.smallFont, "Loading...", layout::width / 2, 990,
                 app.theme.textMuted, 300, true);
    else if (playback.phase == PlaybackPhase::Error)
        drawText(app.renderer, app.smallFont, "Start: Retry   R1: Next", layout::width / 2, 990,
                 app.theme.accent, 460, true);
    else if (playback.phase == PlaybackPhase::Finished)
        drawText(app.renderer, app.smallFont, "Queue finished", layout::width / 2, 990,
                 app.theme.textMuted, 320, true);
}
}  // namespace

void renderApp(AppState& app) {
    fillRect(app.renderer, {0, 0, layout::width, layout::height}, app.theme.background);
    if (app.view.screen == Screen::Library)
        drawLibrary(app);
    else if (app.view.screen == Screen::NowPlaying)
        drawNowPlaying(app);
    else
        drawList(app);
    if (!app.message.empty()) {
        const SDL_Rect banner{40, layout::height - 88, layout::width - 80, 56};
        fillRect(app.renderer, banner, app.theme.surfaceRaised);
        drawText(app.renderer, app.smallFont, app.message, banner.x + 18, banner.y + 12,
                 app.theme.accent, banner.w - 36);
    }
    SDL_RenderPresent(app.renderer);
}
