#include <algorithm>

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
void drawTransport(AppState& app, const Track* track) {
    constexpr int y = layout::miniPlayerY;
    drawPlaybackSurface(app);
    if (!track) {
        drawText(app.renderer, app.bodyFont, "Choose a song from Home", layout::width / 2, y + 66,
                 app.theme.text, 420, true);
        return;
    }

    const auto playback = app.playback.snapshot();
    constexpr int progressX = 80;
    constexpr int progressWidth = 864;
    drawPlaybackProgress(app, playback, track->durationSeconds,
                         {progressX, y + 28, progressWidth, 5}, y + 43);

    const bool paused = playback.phase == PlaybackPhase::Paused;
    drawText(app.renderer, app.smallFont, "L1  PREVIOUS", 252, y + 105, app.theme.textMuted, 180,
             true);
    drawPlayState(app.renderer, 501, y + 91, !paused, app.theme.accent);
    drawText(app.renderer, app.smallFont, paused ? "PLAY" : "PAUSE", 512, y + 129, app.theme.text,
             100, true);
    drawText(app.renderer, app.smallFont, "R1  NEXT", 772, y + 105, app.theme.textMuted, 180, true);

    std::string status;
    if (playback.phase == PlaybackPhase::Loading) status = "LOADING";
    if (playback.phase == PlaybackPhase::Error) status = "START  RETRY  ·  R1  NEXT";
    if (playback.phase == PlaybackPhase::Finished) status = "QUEUE FINISHED";
    if (!status.empty())
        drawText(app.renderer, app.smallFont, status, layout::width / 2, y + 62,
                 playback.phase == PlaybackPhase::Error ? app.theme.accent : app.theme.textMuted,
                 360, true);
}
}  // namespace

void drawNowPlayingScreen(AppState& app) {
    drawText(app.renderer, app.smallFont, "NOW PLAYING", layout::nowPlayingMetadataX, 52,
             app.theme.accent, layout::nowPlayingMetadataWidth);
    const auto index = app.playback.displayTrackIndex();
    if (!index || *index >= app.library.tracks().size()) {
        drawText(app.renderer, app.titleFont, "Nothing playing", layout::nowPlayingMetadataX, 84,
                 app.theme.text, layout::nowPlayingTitleWidth);
        drawText(app.renderer, app.bodyFont, "Return to Songs and choose a track",
                 layout::nowPlayingMetadataX, 142, app.theme.textMuted,
                 layout::nowPlayingMetadataWidth);
        drawCover(app, nullptr,
                  {layout::nowPlayingCoverX, layout::nowPlayingCoverY, layout::nowPlayingCoverSize,
                   layout::nowPlayingCoverSize});
        drawTransport(app, nullptr);
        return;
    }

    const Track& track = app.library.tracks()[*index];
    drawMarqueeText(app.renderer, app.titleFont, track.title,
                    {layout::nowPlayingMetadataX, 84, layout::nowPlayingTitleWidth, 48},
                    app.theme.text, SDL_GetTicks64());
    drawText(app.renderer, app.bodyFont, track.artist, layout::nowPlayingMetadataX, 140,
             app.theme.text, layout::nowPlayingMetadataWidth);
    if (!track.album.empty())
        drawText(app.renderer, app.smallFont, track.album, layout::nowPlayingMetadataX, 176,
                 app.theme.textMuted, layout::nowPlayingMetadataWidth);
    drawCover(app, &track,
              {layout::nowPlayingCoverX, layout::nowPlayingCoverY, layout::nowPlayingCoverSize,
               layout::nowPlayingCoverSize});
    drawTransport(app, &track);
}
