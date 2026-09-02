#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
void drawEmptyState(AppState& app) {
    drawCover(app, nullptr, {48, 208, 152, 152});
    drawText(app.renderer, app.bodyFont, "Nothing here yet", 232, 230, app.theme.text, 360);
    drawText(app.renderer, app.smallFont, "Add music to the Music folder", 232, 274,
             app.theme.textMuted, 360);
}
}  // namespace

void drawSongsScreen(AppState& app) {
    drawText(app.renderer, app.titleFont, "Songs", layout::headingX, layout::headingTitleY,
             app.theme.text, 400);
    const size_t count = app.view.items.size();
    drawText(app.renderer, app.smallFont,
             std::to_string(count) + (count == 1 ? " TRACK" : " TRACKS"), layout::headingX,
             layout::headingSubtitleY, app.theme.accent, 360);
    const auto& items = app.view.items;
    if (items.empty()) {
        drawEmptyState(app);
        drawNowPlayingBand(app);
        return;
    }
    constexpr int visibleRows = 5;
    if (app.view.selected < app.view.scroll) app.view.scroll = app.view.selected;
    if (app.view.selected >= app.view.scroll + visibleRows)
        app.view.scroll = app.view.selected - visibleRows + 1;
    for (int row = 0; row < visibleRows && app.view.scroll + row < static_cast<int>(items.size());
         ++row) {
        const int index = app.view.scroll + row;
        const int y = layout::contentRowsY + row * layout::contentRowHeight;
        const Track& track = app.library.tracks()[*items[index].trackIndex];
        ViewItem display = items[index];
        display.subtitle = track.artist;
        if (!track.album.empty()) display.subtitle += "  ·  " + track.album;
        drawTrackRow(app, display, index, y, index == app.view.selected,
                     formatDuration(track.durationSeconds));
    }
    drawNowPlayingBand(app);
}
