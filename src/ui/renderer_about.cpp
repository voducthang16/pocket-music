#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
void drawDetail(AppState& app, const std::string& label, const std::string& value, int y) {
    drawText(app.renderer, app.smallFont, label, layout::contentRowsX, y, app.theme.accent, 160);
    drawText(app.renderer, app.bodyFont, value, layout::contentRowsX + 176, y - 7, app.theme.text,
             384);
}
}  // namespace

void drawAboutScreen(AppState& app) {
    drawText(app.renderer, app.titleFont, "About", layout::headingX, layout::headingTitleY,
             app.theme.text, 400);
    drawText(app.renderer, app.smallFont, "POCKET MUSIC", layout::headingX,
             layout::headingSubtitleY, app.theme.accent, 360);
    drawDetail(app, "VERSION", POCKET_MUSIC_VERSION, 158);
    drawDetail(app, "BY", "voducthang16", 210);
    drawDetail(app, "SOURCE", "github.com/voducthang16/pocket-music", 262);
    drawDetail(app, "MUSIC", app.library.root().string(), 314);
    drawDetail(app, "FORMATS", "MP3 · FLAC · WAV · OGG", 366);
    drawNowPlayingBand(app);
}
