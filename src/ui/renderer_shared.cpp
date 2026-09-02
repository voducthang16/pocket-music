#include <SDL_image.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

std::string formatDuration(int seconds) {
    std::ostringstream stream;
    stream << seconds / 60 << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
    return stream.str();
}

std::string repeatLabel(const AppState& app) {
    if (app.playback.repeatMode() == RepeatMode::One) return "REPEAT ONE";
    if (app.playback.repeatMode() == RepeatMode::All) return "REPEAT ALL";
    return "REPEAT OFF";
}

void drawHeader(AppState& app, const std::string& title, const std::string& eyebrow) {
    drawText(app.renderer, app.titleFont, title, layout::pagePadding, 22, app.theme.text, 620);
    if (!eyebrow.empty())
        drawText(app.renderer, app.smallFont, eyebrow, layout::width - 300, 31, app.theme.textMuted,
                 252);
    fillRect(
        app.renderer,
        {layout::pagePadding, layout::headerHeight - 1, layout::width - layout::pagePadding * 2, 1},
        app.theme.divider);
}

void drawButtonHints(AppState& app) {
    const int y = layout::height - layout::footerHeight;
    fillRect(app.renderer, {0, y, layout::width, layout::footerHeight}, app.theme.surface);
    fillRect(app.renderer, {0, y, layout::width, 1}, app.theme.divider);
    for (int column = 1; column < 4; ++column)
        fillRect(app.renderer, {column * 256, y + 12, 1, 32}, app.theme.divider);
    const bool nowPlaying = app.view.screen == Screen::NowPlaying;
    drawText(app.renderer, app.smallFont, nowPlaying ? "START  PLAY / PAUSE" : "A  SELECT", 128,
             y + 13, app.theme.text, 230, true);
    drawText(app.renderer, app.smallFont, "B  BACK", 384, y + 13, app.theme.textMuted, 220, true);
    drawText(app.renderer, app.smallFont,
             app.playback.shuffle() ? "Y  SHUFFLE ON" : "Y  SHUFFLE OFF", 640, y + 13,
             app.playback.shuffle() ? app.theme.accent : app.theme.textMuted, 230, true);
    drawText(app.renderer, app.smallFont, "SELECT  " + repeatLabel(app), 896, y + 13,
             app.playback.repeatMode() == RepeatMode::Off ? app.theme.textMuted : app.theme.accent,
             230, true);
}

const Track* currentTrack(const AppState& app) {
    const auto index = app.playback.displayTrackIndex();
    return index && *index < app.library.tracks().size() ? &app.library.tracks()[*index] : nullptr;
}

void drawNowPlayingBand(AppState& app) {
    constexpr int y = 520;
    fillRect(app.renderer, {0, y, layout::width, 192}, app.theme.surface);
    fillRect(app.renderer, {0, y, layout::width, 1}, app.theme.divider);
    const Track* track = currentTrack(app);
    drawCover(app, track, {48, y + 18, 156, 156});
    drawText(app.renderer, app.bodyFont, track ? track->title : "Ready when you are", 236, y + 26,
             app.theme.text, 420);
    drawText(app.renderer, app.smallFont, track ? track->artist : "Choose a song to begin", 236,
             y + 70, app.theme.textMuted, 420);

    const auto playback = app.playback.snapshot();
    const int elapsed = std::max(0, static_cast<int>(playback.positionSeconds));
    const int duration =
        track ? (playback.durationSeconds > 0 ? static_cast<int>(playback.durationSeconds)
                                              : track->durationSeconds)
              : 0;
    constexpr int progressX = 236;
    constexpr int progressWidth = 740;
    const int filled = duration > 0 ? progressWidth * elapsed / std::max(1, duration) : 0;
    fillRect(app.renderer, {progressX, y + 116, progressWidth, 4}, app.theme.surfaceRaised);
    fillRect(app.renderer, {progressX, y + 116, filled, 4}, app.theme.accent);
    drawText(app.renderer, app.smallFont, formatDuration(elapsed), progressX, y + 136,
             app.theme.textMuted);
    drawText(app.renderer, app.smallFont, formatDuration(duration), 914, y + 136,
             app.theme.textMuted, 62);
    if (track)
        drawText(app.renderer, app.smallFont,
                 playback.phase == PlaybackPhase::Paused ? "PAUSED" : "PLAYING", 700, y + 70,
                 app.theme.accent, 276);
}

SDL_Texture* albumCover(AppState& app, const Track& track) {
    if (track.coverPath.empty()) return nullptr;
    const auto key = track.coverPath.string();
    if (const auto cached = app.coverCache.find(key); cached != app.coverCache.end())
        return cached->second;
    return app.coverCache[key] = IMG_LoadTexture(app.renderer, key.c_str());
}

void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds) {
    SDL_Texture* texture = track ? albumCover(app, *track) : nullptr;
    if (!texture) {
        fillRect(app.renderer, bounds, app.theme.surfaceRaised);
        const int size = std::min(bounds.w, bounds.h) / 3;
        fillRect(app.renderer,
                 {bounds.x + (bounds.w - size) / 2, bounds.y + (bounds.h - size) / 2, size, size},
                 app.theme.accentSoft);
        drawPlayState(app.renderer, bounds.x + bounds.w / 2 - 11, bounds.y + bounds.h / 2 - 13,
                      false, app.theme.accent);
        return;
    }
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
    SDL_RenderCopy(app.renderer, texture, nullptr, &target);
}
