#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

namespace {
const Track* currentTrack(const AppState& app) {
    const auto index = app.playback.displayTrackIndex();
    return index && *index < app.library.tracks().size() ? &app.library.tracks()[*index] : nullptr;
}

void drawCurrentTrack(AppState& app) {
    const SDL_Rect panel{386, 112, 590, 568};
    fillRect(app.renderer, panel, app.theme.surface);
    const Track* track = currentTrack(app);
    drawText(app.renderer, app.smallFont, track ? "NOW PLAYING" : "POCKET MUSIC", panel.x + 32,
             panel.y + 26, app.theme.accent, panel.w - 64);
    drawCover(app, track, {panel.x + 32, panel.y + 72, 280, 280});
    const int textX = panel.x + 344;
    drawText(app.renderer, app.bodyFont, track ? track->title : "Ready when you are", textX,
             panel.y + 92, app.theme.text, panel.w - 376);
    drawText(app.renderer, app.smallFont, track ? track->artist : "Choose Songs to begin", textX,
             panel.y + 143, app.theme.textMuted, panel.w - 376);
    drawText(app.renderer, app.smallFont, track ? track->album : "Music stays on your SD card",
             textX, panel.y + 184, app.theme.textMuted, panel.w - 376);
    fillRect(app.renderer, {textX, panel.y + 254, panel.w - 376, 6}, app.theme.surfaceRaised);
    drawText(app.renderer, app.smallFont, track ? "START  Play / Pause" : "A  Open library", textX,
             panel.y + 286, app.theme.textMuted, panel.w - 376);
    drawText(app.renderer, app.smallFont, countLabel(app.library.tracks().size(), "song", "songs"),
             panel.x + 32, panel.y + 510, app.theme.textMuted, 220);
}
}  // namespace

void drawLibraryScreen(AppState& app) {
    drawHeader(app, "Library", app.view.eyebrow);
    const auto& items = app.view.items;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int y = 112 + index * 90;
        const bool active = index == app.view.selected;
        if (active) {
            fillRect(app.renderer, {layout::pagePadding, y, 314, 78}, app.theme.surfaceRaised);
            fillRect(app.renderer, {layout::pagePadding, y, 5, 78}, app.theme.accent);
        }
        drawText(app.renderer, app.bodyFont, items[index].title, layout::pagePadding + 22, y + 9,
                 app.theme.text, 210);
        drawText(app.renderer, app.smallFont, items[index].subtitle, layout::pagePadding + 22,
                 y + 48, app.theme.textMuted, 220);
        drawChevron(app.renderer, 330, y + 39, active ? app.theme.accent : app.theme.textMuted);
    }
    drawCurrentTrack(app);
}
