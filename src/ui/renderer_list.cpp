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
        const int y = 102 + row * 96;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer, {layout::pagePadding, y, 928, 86}, app.theme.surfaceRaised);
            fillRect(app.renderer, {layout::pagePadding, y, 5, 86}, app.theme.accent);
        }
        drawText(app.renderer, app.bodyFont, items[index].title, 68, y + 10, app.theme.text, 680);
        const Track* track = trackForRow(app, index);
        const std::string subtitle =
            track ? track->artist + "  ·  " + track->album : items[index].subtitle;
        drawText(app.renderer, app.smallFont, subtitle, 68, y + 51, app.theme.textMuted, 700);
        if (track)
            drawText(app.renderer, app.smallFont, formatDuration(track->durationSeconds), 850,
                     y + 28, app.theme.textMuted, 80);
        drawChevron(app.renderer, 950, y + 42, active ? app.theme.accent : app.theme.textMuted);
    }
}
