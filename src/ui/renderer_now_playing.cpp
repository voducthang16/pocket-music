#include <algorithm>

#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void drawNowPlayingScreen(AppState& app) {
    drawHeader(app, "Now Playing");
    const auto playback = app.playback.snapshot();
    const auto index = app.playback.displayTrackIndex();
    if (!index || *index >= app.library.tracks().size()) {
        drawCover(app, nullptr, {80, 176, 320, 320});
        drawText(app.renderer, app.titleFont, "Nothing playing", 472, 236, app.theme.text, 440);
        drawText(app.renderer, app.bodyFont, "Return to Songs and choose a track", 472, 304,
                 app.theme.textMuted, 480);
        return;
    }
    const Track& track = app.library.tracks()[*index];
    drawCover(app, &track, {48, 128, 400, 400});
    drawMarqueeText(app.renderer, app.titleFont, track.title, {504, 148, 472, 54}, app.theme.text,
                    SDL_GetTicks64());
    drawText(app.renderer, app.bodyFont, track.artist, 504, 216, app.theme.text, 472);
    const int elapsed = std::max(0, static_cast<int>(playback.positionSeconds));
    const int duration = playback.durationSeconds > 0 ? static_cast<int>(playback.durationSeconds)
                                                      : track.durationSeconds;
    constexpr int progressWidth = 472;
    const int filled = std::min(progressWidth, progressWidth * elapsed / std::max(1, duration));
    fillRect(app.renderer, {504, 376, progressWidth, 5}, app.theme.surfaceRaised);
    fillRect(app.renderer, {504, 376, filled, 5}, app.theme.accent);
    drawText(app.renderer, app.smallFont, formatDuration(elapsed), 504, 397, app.theme.textMuted);
    drawText(app.renderer, app.smallFont, formatDuration(duration), 914, 397, app.theme.textMuted,
             62);
    const bool paused = playback.phase == PlaybackPhase::Paused;
    drawPlayState(app.renderer, 510, 476, !paused, app.theme.accent);
    drawText(app.renderer, app.bodyFont, paused ? "Paused" : "Playing", 552, 465, app.theme.text,
             190);
    drawText(app.renderer, app.smallFont, "L1  PREVIOUS", 504, 532, app.theme.textMuted, 180);
    drawText(app.renderer, app.smallFont, "R1  NEXT", 796, 532, app.theme.textMuted, 180);
    std::string status;
    if (playback.phase == PlaybackPhase::Loading) status = "Loading...";
    if (playback.phase == PlaybackPhase::Error) status = "START  Retry    R1  Next";
    if (playback.phase == PlaybackPhase::Finished) status = "Queue finished";
    if (!status.empty())
        drawText(app.renderer, app.smallFont, status, 504, 656,
                 playback.phase == PlaybackPhase::Error ? app.theme.accent : app.theme.textMuted,
                 472);
}
