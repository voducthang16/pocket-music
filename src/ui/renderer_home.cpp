#include "ui/layout.hpp"
#include "ui/presentation.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

void drawHomeScreen(AppState& app) {
    drawText(app.renderer, app.titleFont, "Home", layout::headingX, layout::headingTitleY,
             app.theme.text, 360);
    drawText(app.renderer, app.smallFont, "YOUR MUSIC COLLECTION  |  v" POCKET_MUSIC_VERSION,
             layout::headingX, layout::headingSubtitleY, app.theme.accent, 360);
    const auto& items = app.view.items;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int y = layout::contentRowsY + index * layout::contentRowHeight;
        const auto presentation = homeRowPresentation(items[index]);
        drawHomeRow(app, items[index], index, y, index == app.view.selected,
                    presentation.chevron);
    }
    drawNowPlayingBand(app);
}
