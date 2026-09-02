#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
std::string listSummary(const AppState& app) {
    const size_t count = app.view.items.size();
    const char* label = "ITEMS";
    if (app.view.screen == Screen::Tracks)
        label = count == 1 ? "TRACK" : "TRACKS";
    else if (app.view.screen == Screen::Albums)
        label = count == 1 ? "ALBUM" : "ALBUMS";
    else if (app.view.screen == Screen::Artists)
        label = count == 1 ? "ARTIST" : "ARTISTS";
    else if (app.view.screen == Screen::Playlists)
        label = count == 1 ? "PLAYLIST" : "PLAYLISTS";
    return std::to_string(count) + " " + label;
}

const Track* trackForRow(const AppState& app, int index) {
    if (app.view.screen != Screen::Tracks || index < 0 ||
        index >= static_cast<int>(app.view.items.size()) ||
        app.view.items[index].trackIndexes.empty())
        return nullptr;
    return &app.library.tracks()[app.view.items[index].trackIndexes.front()];
}

void drawEmptyState(AppState& app) {
    drawCover(app, nullptr, {48, 208, 152, 152});
    drawText(app.renderer, app.bodyFont, "Nothing here yet", 232, 230, app.theme.text, 360);
    drawText(app.renderer, app.smallFont, "Add music to the Music folder", 232, 274,
             app.theme.textMuted, 360);
}
}  // namespace

void drawListScreen(AppState& app) {
    drawText(app.renderer, app.titleFont, app.view.title, layout::headingX, layout::headingTitleY,
             app.theme.text, 400);
    drawText(app.renderer, app.smallFont, listSummary(app), layout::headingX,
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
        const bool active = index == app.view.selected;
        const Track* track = trackForRow(app, index);
        ViewItem rowItem = items[index];
        if (track) rowItem.subtitle = track->artist + "  ·  " + track->album;
        drawCompactRow(app, rowItem, index, y, active, track ? 316 : 400,
                       track ? formatDuration(track->durationSeconds) : "", !track);
    }
    drawNowPlayingBand(app);
}
