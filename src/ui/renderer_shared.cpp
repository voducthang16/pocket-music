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

void drawButtonHints(AppState& app) {
    constexpr int y = layout::footerTextY;
    const bool nowPlaying = app.view.screen == Screen::NowPlaying;
    drawText(app.renderer, app.smallFont, nowPlaying ? "START  PLAY / PAUSE" : "A  SELECT", 128, y,
             app.theme.text, 230, true);
    drawText(app.renderer, app.smallFont, "B  BACK", 384, y, app.theme.textMuted, 220, true);
    drawText(app.renderer, app.smallFont,
             app.playback.shuffle() ? "Y  SHUFFLE ON" : "Y  SHUFFLE OFF", 640, y,
             app.playback.shuffle() ? app.theme.accent : app.theme.textMuted, 230, true);
    drawText(app.renderer, app.smallFont, "SELECT  " + repeatLabel(app), 896, y,
             app.playback.repeatMode() == RepeatMode::Off ? app.theme.textMuted : app.theme.accent,
             230, true);
}

namespace {
int centeredTextY(TTF_Font* font, int rowY) {
    return rowY + (layout::contentRowHeight - TTF_FontHeight(font)) / 2;
}

void drawRowSelection(AppState& app, int y, bool active) {
    if (!active) return;
    SDL_Color selected = app.theme.accentSoft;
    selected.a = 196;
    fillRoundedRect(app.renderer, {layout::contentRowsX, y + 3, layout::contentRowsWidth, 58}, 14,
                    selected);
}

std::string rowNumber(int index) {
    return index < 9 ? "0" + std::to_string(index + 1) : std::to_string(index + 1);
}
}  // namespace

void drawHomeRow(AppState& app, const ViewItem& item, int index, int y, bool active,
                 const std::string& trailing, bool chevron) {
    drawRowSelection(app, y, active);
    const int smallY = centeredTextY(app.smallFont, y);
    drawTextRightAligned(app.renderer, app.smallFont, rowNumber(index), layout::contentRowsX + 48,
                         smallY, active ? app.theme.accent : app.theme.textMuted);
    drawText(app.renderer, app.bodyFont, item.title, layout::contentRowsX + 72,
             centeredTextY(app.bodyFont, y), app.theme.text, 380);
    constexpr int trailingRight = layout::contentRowsX + layout::contentRowsWidth - 20;
    if (!trailing.empty())
        drawTextRightAligned(app.renderer, app.smallFont, trailing, trailingRight, smallY,
                             app.theme.textMuted);
    else if (chevron)
        drawChevron(app.renderer, trailingRight - 9, y + layout::contentRowHeight / 2,
                    active ? app.theme.accent : app.theme.textMuted);
}

void drawTrackRow(AppState& app, const ViewItem& item, int index, int y, bool active,
                  const std::string& duration) {
    drawRowSelection(app, y, active);
    const std::string number = rowNumber(index);
    drawText(app.renderer, app.smallFont, number, layout::contentRowsX + 16, y + 18,
             active ? app.theme.accent : app.theme.textMuted, 52);
    drawText(app.renderer, app.bodyFont, item.title, layout::contentRowsX + 72, y + 1,
             app.theme.text, 316);
    drawText(app.renderer, app.smallFont, item.subtitle, layout::contentRowsX + 72, y + 36,
             app.theme.textMuted, 316);
    drawTextRightAligned(app.renderer, app.smallFont, duration,
                         layout::contentRowsX + layout::contentRowsWidth - 20, y + 18,
                         app.theme.textMuted);
}

const Track* currentTrack(const AppState& app) {
    const auto index = app.playback.displayTrackIndex();
    return index && *index < app.library.tracks().size() ? &app.library.tracks()[*index] : nullptr;
}

void drawPlaybackSurface(AppState& app) {
    SDL_Color glass = app.theme.surface;
    glass.a = 108;
    fillRect(app.renderer,
             {layout::miniPlayerX, layout::miniPlayerY, layout::miniPlayerWidth,
              layout::miniPlayerHeight},
             glass);
}

void drawPlaybackProgress(AppState& app, const PlaybackSnapshot& playback, int fallbackDuration,
                          const SDL_Rect& bar, int labelsY) {
    const int elapsed = std::max(0, static_cast<int>(playback.positionSeconds));
    const int duration = playback.durationSeconds > 0 ? static_cast<int>(playback.durationSeconds)
                                                      : fallbackDuration;
    const int boundedElapsed = duration > 0 ? std::min(elapsed, duration) : 0;
    const int filled =
        duration > 0 ? static_cast<int>(static_cast<double>(bar.w) * boundedElapsed / duration) : 0;
    fillRoundedRect(app.renderer, bar, 2, app.theme.surfaceRaised);
    fillRoundedRect(app.renderer, {bar.x, bar.y, filled, bar.h}, 2, app.theme.accent);
    drawText(app.renderer, app.smallFont, formatDuration(elapsed), bar.x, labelsY,
             app.theme.textMuted);
    drawText(app.renderer, app.smallFont, formatDuration(std::max(0, duration)), bar.x + bar.w - 62,
             labelsY, app.theme.textMuted, 62);
}

void drawNowPlayingBand(AppState& app) {
    constexpr int x = layout::miniPlayerX;
    constexpr int y = layout::miniPlayerY;
    drawPlaybackSurface(app);
    const Track* track = currentTrack(app);
    drawCover(app, track,
              {x + 48, layout::miniPlayerCoverY, layout::miniPlayerCoverSize,
               layout::miniPlayerCoverSize});
    drawText(app.renderer, app.bodyFont, track ? track->title : "Ready when you are", x + 232,
             y + 23, app.theme.text, 540);
    drawText(app.renderer, app.smallFont, track ? track->artist : "Choose a song to begin", x + 232,
             y + 67, app.theme.textMuted, 420);

    const auto playback = app.playback.snapshot();
    constexpr int progressX = x + 232;
    constexpr int progressWidth = 700;
    drawPlaybackProgress(app, playback, track ? track->durationSeconds : 0,
                         {progressX, y + 120, progressWidth, 5}, y + 139);
    if (track)
        drawText(app.renderer, app.smallFont,
                 playback.phase == PlaybackPhase::Paused ? "PAUSED" : "PLAYING", x + 820, y + 67,
                 app.theme.accent, 112);
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
        if (app.fallbackVinylTexture) {
            SDL_RenderCopyEx(app.renderer, app.fallbackVinylTexture, nullptr, &bounds,
                             app.vinylAngle, nullptr, SDL_FLIP_NONE);
        } else {
            fillRect(app.renderer, bounds, app.theme.surfaceRaised);
        }
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
