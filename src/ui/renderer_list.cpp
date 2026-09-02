#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
const Track* trackForRow(const AppState& app, int index) {
    if (app.view.screen != Screen::Tracks || index < 0 ||
        index >= static_cast<int>(app.view.items.size()) ||
        app.view.items[index].trackIndexes.empty())
        return nullptr;
    return &app.library.tracks()[app.view.items[index].trackIndexes.front()];
}

void drawEmptyState(AppState& app) {
    drawCover(app, nullptr, {112, 224, 220, 220});
    drawText(app.renderer, app.titleFont, "Nothing here yet", 380, 264, app.theme.text, 520);
    drawText(app.renderer, app.bodyFont, "Add music to the Music folder", 380, 326,
             app.theme.textMuted, 520);
}
}  // namespace

void drawListScreen(AppState& app) {
    drawHeader(app, app.view.title, app.view.eyebrow);
    const auto& items = app.view.items;
    if (items.empty()) {
        drawEmptyState(app);
        return;
    }
    constexpr int visibleRows = 6;
    if (app.view.selected < app.view.scroll) app.view.scroll = app.view.selected;
    if (app.view.selected >= app.view.scroll + visibleRows)
        app.view.scroll = app.view.selected - visibleRows + 1;
    for (int row = 0; row < visibleRows && app.view.scroll + row < static_cast<int>(items.size());
         ++row) {
        const int index = app.view.scroll + row;
        const int y = layout::headerHeight + row * 72;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer, {layout::pagePadding, y, 928, 72}, app.theme.text);
            fillRect(app.renderer, {layout::pagePadding, y, 6, 72}, app.theme.accent);
        } else if (row > 0) {
            fillRect(app.renderer, {layout::pagePadding, y, 928, 1}, app.theme.divider);
        }
        const SDL_Color primary = active ? app.theme.background : app.theme.text;
        const SDL_Color secondary = active ? app.theme.surface : app.theme.textMuted;
        const std::string number =
            index < 9 ? "0" + std::to_string(index + 1) : std::to_string(index + 1);
        drawText(app.renderer, app.smallFont, number, 72, y + 23,
                 active ? app.theme.accent : app.theme.textMuted, 52);
        drawText(app.renderer, app.bodyFont, items[index].title, 142, y + 8, primary, 610);
        const Track* track = trackForRow(app, index);
        const std::string subtitle =
            track ? track->artist + "  ·  " + track->album : items[index].subtitle;
        drawText(app.renderer, app.smallFont, subtitle, 142, y + 43, secondary, 610);
        if (track)
            drawText(app.renderer, app.smallFont, formatDuration(track->durationSeconds), 858,
                     y + 23, secondary, 74);
        drawChevron(app.renderer, 950, y + 36, active ? app.theme.accent : app.theme.textMuted);
    }
    drawNowPlayingBand(app);
}
