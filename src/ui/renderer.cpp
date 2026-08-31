#include "ui/renderer.hpp"

#include <SDL_image.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "app/navigation.hpp"
#include "ui/layout.hpp"

namespace {
const SDL_Color white{250, 250, 250, 255}, black{25, 25, 28, 255}, gray{105, 105, 112, 255},
    blue{31, 114, 205, 255};
void fill(SDL_Renderer* r, SDL_Rect rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
void text(SDL_Renderer* r, TTF_Font* f, const std::string& v, int x, int y, SDL_Color c,
          int max = 0, bool center = false) {
    if (v.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, v.c_str(), c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect target{x, y, s->w, s->h};
    if (max > 0 && target.w > max) target.w = max;
    if (center) target.x = x - target.w / 2;
    SDL_RenderCopy(r, t, nullptr, &target);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}
std::string duration(int seconds) {
    std::ostringstream s;
    s << seconds / 60 << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
    return s.str();
}
void header(AppState& app) {
    fill(app.renderer, {0, 0, layout::width, layout::headerHeight}, {224, 225, 229, 255});
    text(app.renderer, app.titleFont, screenHeading(app), layout::width / 2, 17, black, 560, true);
    text(app.renderer, app.smallFont, app.player.paused() ? "II" : ">", 20, 22, gray);
    text(app.renderer, app.smallFont, "100%", layout::width - 87, 22, gray);
    fill(app.renderer, {layout::width - 34, 25, 22, 12}, {102, 190, 90, 255});
}
void list(AppState& app) {
    const auto items = visibleLabels(app);
    if (items.empty()) {
        text(app.renderer, app.bodyFont, "No music found", layout::width / 2, 250, gray, 600, true);
        text(app.renderer, app.smallFont, "Copy songs into the Music folder", layout::width / 2,
             315, gray, 650, true);
        return;
    }
    const int visible = (layout::height - layout::headerHeight) / layout::rowHeight;
    if (app.selected < app.scroll) app.scroll = app.selected;
    if (app.selected >= app.scroll + visible) app.scroll = app.selected - visible + 1;
    for (int row = 0; row < visible && app.scroll + row < static_cast<int>(items.size()); ++row) {
        int index = app.scroll + row, y = layout::headerHeight + row * layout::rowHeight;
        bool active = index == app.selected;
        if (active) fill(app.renderer, {0, y, layout::width, layout::rowHeight}, blue);
        text(app.renderer, app.bodyFont, items[index], 28, y + 18, active ? white : black,
             layout::width - 90);
        text(app.renderer, app.bodyFont, ">", layout::width - 48, y + 18, active ? white : gray);
        fill(app.renderer, {0, y + layout::rowHeight - 1, layout::width, 1}, {220, 220, 223, 255});
    }
}
SDL_Texture* cover(AppState& app, const Track& track) {
    if (track.coverPath.empty()) return nullptr;
    auto key = track.coverPath.string();
    if (app.coverCache.contains(key)) return app.coverCache[key];
    return app.coverCache[key] = IMG_LoadTexture(app.renderer, key.c_str());
}
void nowPlaying(AppState& app) {
    if (app.currentTrack < 0 || app.currentTrack >= static_cast<int>(app.library.tracks().size())) {
        text(app.renderer, app.bodyFont, "Nothing Playing", layout::width / 2, 300, gray, 600,
             true);
        return;
    }
    const auto& track = app.library.tracks()[app.currentTrack];
    SDL_Rect art{layout::width / 2 - 245, 115, 490, 490};
    if (auto* texture = cover(app, track))
        SDL_RenderCopy(app.renderer, texture, nullptr, &art);
    else {
        fill(app.renderer, art, {215, 216, 222, 255});
        text(app.renderer, app.titleFont, "MUSIC", layout::width / 2, 325, gray, 400, true);
    }
    text(app.renderer, app.titleFont, track.title, layout::width / 2, 650, black, 690, true);
    text(app.renderer, app.bodyFont, track.artist, layout::width / 2, 710, gray, 650, true);
    text(app.renderer, app.smallFont, track.album, layout::width / 2, 760, gray, 650, true);
    int elapsed = std::max(0, app.player.elapsedSeconds()),
        total = std::max(1, track.durationSeconds);
    fill(app.renderer, {55, 846, layout::width - 110, 12}, {205, 206, 210, 255});
    fill(app.renderer,
         {55, 846, std::min(layout::width - 110, (layout::width - 110) * elapsed / total), 12},
         blue);
    text(app.renderer, app.smallFont, duration(elapsed), 55, 874, gray);
    text(app.renderer, app.smallFont, duration(track.durationSeconds), layout::width - 125, 874,
         gray);
    std::string modes =
        std::string(app.shuffle ? "Shuffle  " : "") + (app.repeatMode == 1   ? "Repeat One"
                                                       : app.repeatMode == 2 ? "Repeat All"
                                                                             : "");
    text(app.renderer, app.smallFont, modes, layout::width / 2, 930, blue, 500, true);
}
}  // namespace
void renderApp(AppState& app) {
    fill(app.renderer, {0, 0, layout::width, layout::height}, white);
    header(app);
    if (app.screen == Screen::NowPlaying)
        nowPlaying(app);
    else
        list(app);
    SDL_RenderPresent(app.renderer);
}
