#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void drawLibraryScreen(AppState& app) {
    drawText(app.renderer, app.titleFont, "Library", layout::headingX, layout::headingTitleY,
             app.theme.text, 360);
    drawText(app.renderer, app.smallFont, "YOUR MUSIC COLLECTION", layout::headingX,
             layout::headingSubtitleY, app.theme.accent, 360);
    const auto& items = app.view.items;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int y = layout::contentRowsY + index * layout::contentRowHeight;
        const bool active = index == app.view.selected;
        drawCompactRow(app, items[index], index, y, active, 420);
    }
    drawNowPlayingBand(app);
}
