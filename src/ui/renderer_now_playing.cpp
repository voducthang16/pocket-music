#include <algorithm>

#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void drawNowPlayingScreen(AppState& app) {
    drawHeader(app, "Now Playing", "POCKET MUSIC");
    const auto playback = app.playback.snapshot();
    const auto index = app.playback.displayTrackIndex();
    if (!index || *index >= app.library.tracks().size()) {
        drawCover(app, nullptr, {90, 180, 300, 300});
        drawText(app.renderer, app.titleFont, "Nothing playing", 450, 236, app.theme.text, 480);
        drawText(app.renderer, app.bodyFont, "Return to Songs and choose a track", 450, 304,
                 app.theme.textMuted, 480);
        return;
    }
    const Track& track = app.library.tracks()[*index];
    drawCover(app, &track, {48, 112, 520, 520});
    drawMarqueeText(app.renderer, app.titleFont, track.title, {616, 145, 346, 54}, app.theme.text,
                    SDL_GetTicks64());
    drawText(app.renderer, app.bodyFont, track.artist, 616, 216, app.theme.textMuted, 346);
    drawText(app.renderer, app.smallFont, track.album, 616, 265, app.theme.textMuted, 346);
    const int elapsed = std::max(0, static_cast<int>(playback.positionSeconds));
    const int duration = playback.durationSeconds > 0 ? static_cast<int>(playback.durationSeconds)
                                                      : track.durationSeconds;
    constexpr int progressWidth = 346;
    const int filled = std::min(progressWidth, progressWidth * elapsed / std::max(1, duration));
    fillRect(app.renderer, {616, 352, progressWidth, 8}, app.theme.surfaceRaised);
    fillRect(app.renderer, {616, 352, filled, 8}, app.theme.accent);
    drawText(app.renderer, app.smallFont, formatDuration(elapsed), 616, 376, app.theme.textMuted);
    drawText(app.renderer, app.smallFont, formatDuration(duration), 888, 376, app.theme.textMuted,
             74);
    const bool paused = playback.phase == PlaybackPhase::Paused;
    fillRect(app.renderer, {616, 448, 96, 96}, app.theme.surfaceRaised);
    drawPlayState(app.renderer, 655, 487, paused, app.theme.text);
    drawText(app.renderer, app.smallFont, paused ? "PAUSED" : "PLAYING", 738, 470, app.theme.accent,
             180);
    drawText(app.renderer, app.smallFont, "L1  Previous", 738, 507, app.theme.textMuted, 180);
    drawText(app.renderer, app.smallFont, "R1  Next", 738, 544, app.theme.textMuted, 180);
    std::string status;
    if (playback.phase == PlaybackPhase::Loading) status = "Loading...";
    if (playback.phase == PlaybackPhase::Error) status = "START  Retry    R1  Next";
    if (playback.phase == PlaybackPhase::Finished) status = "Queue finished";
    if (!status.empty())
        drawText(app.renderer, app.smallFont, status, 616, 604,
                 playback.phase == PlaybackPhase::Error ? app.theme.accent : app.theme.textMuted,
                 346);
}
