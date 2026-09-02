#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void drawLibraryScreen(AppState& app) {
    drawHeader(app, "Library");
    const auto& items = app.view.items;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int y = layout::headerHeight + index * 72;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer, {48, y, 928, 72}, app.theme.text);
            fillRect(app.renderer, {48, y, 6, 72}, app.theme.accent);
        } else if (index > 0) {
            fillRect(app.renderer, {48, y, 928, 1}, app.theme.divider);
        }
        const SDL_Color primary = active ? app.theme.background : app.theme.text;
        const SDL_Color secondary = active ? app.theme.surface : app.theme.textMuted;
        const std::string number =
            index < 9 ? "0" + std::to_string(index + 1) : std::to_string(index + 1);
        drawText(app.renderer, app.smallFont, number, 72, y + 23,
                 active ? app.theme.accent : app.theme.textMuted, 52);
        drawText(app.renderer, app.bodyFont, items[index].title, 142, y + 14, primary, 520);
        drawText(app.renderer, app.smallFont, items[index].subtitle, 742, y + 22, secondary, 190);
        drawChevron(app.renderer, 948, y + 36, active ? app.theme.accent : app.theme.textMuted);
    }
    drawNowPlayingBand(app);
}
