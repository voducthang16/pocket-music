#include <SDL_image.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "ui/layout.hpp"
#include "ui/primitives.hpp"
#include "ui/renderer_internal.hpp"

std::string formatDuration(int seconds) {
    std::ostringstream stream;
    stream << seconds / 60 << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
    return stream.str();
}

std::string countLabel(size_t count, const char* singular, const char* plural) {
    return std::to_string(count) + ' ' + (count == 1 ? singular : plural);
}

void drawHeader(AppState& app, const std::string& title, const std::string& eyebrow) {
    if (!eyebrow.empty())
        drawText(app.renderer, app.smallFont, eyebrow, layout::pagePadding, 14, app.theme.textMuted,
                 360);
    drawText(app.renderer, app.titleFont, title, layout::pagePadding, eyebrow.empty() ? 20 : 37,
             app.theme.text, 540);
    drawText(app.renderer, app.smallFont, "OFFLINE", layout::width - 174, 31, app.theme.accent,
             126);
    fillRect(
        app.renderer,
        {layout::pagePadding, layout::headerHeight - 1, layout::width - layout::pagePadding * 2, 1},
        app.theme.divider);
}

void drawButtonHints(AppState& app) {
    const int y = layout::height - layout::footerHeight;
    fillRect(app.renderer, {0, y, layout::width, layout::footerHeight}, app.theme.surface);
    fillRect(app.renderer, {0, y, layout::width, 1}, app.theme.divider);
    drawText(app.renderer, app.smallFont, "A  SELECT", 48, y + 13, app.theme.text, 160);
    drawText(app.renderer, app.smallFont, "B  BACK", 230, y + 13, app.theme.textMuted, 150);
    drawText(app.renderer, app.smallFont, "START  PLAY / PAUSE", 430, y + 13, app.theme.textMuted,
             270);
    drawText(app.renderer, app.smallFont, "L1 / R1  TRACK", 780, y + 13, app.theme.textMuted, 210);
}

SDL_Texture* albumCover(AppState& app, const Track& track) {
    if (track.coverPath.empty()) return nullptr;
    const auto key = track.coverPath.string();
    if (const auto cached = app.coverCache.find(key); cached != app.coverCache.end())
        return cached->second;
    return app.coverCache[key] = IMG_LoadTexture(app.renderer, key.c_str());
}

void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds) {
    SDL_Texture* texture = track ? albumCover(app, *track) : nullptr;
    if (!texture) {
        fillRect(app.renderer, bounds, app.theme.surfaceRaised);
        const int size = std::min(bounds.w, bounds.h) / 3;
        fillRect(app.renderer,
                 {bounds.x + (bounds.w - size) / 2, bounds.y + (bounds.h - size) / 2, size, size},
                 app.theme.accentSoft);
        drawPlayState(app.renderer, bounds.x + bounds.w / 2 - 8, bounds.y + bounds.h / 2 - 9, false,
                      app.theme.accent);
        return;
    }
    int width = 0, height = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &width, &height) != 0 || width <= 0 ||
        height <= 0)
        return;
    SDL_Rect target = bounds;
    if (width * bounds.h > height * bounds.w) {
        target.h = bounds.w * height / width;
        target.y += (bounds.h - target.h) / 2;
    } else {
        target.w = bounds.h * width / height;
        target.x += (bounds.w - target.w) / 2;
    }
    SDL_RenderCopy(app.renderer, texture, nullptr, &target);
}
