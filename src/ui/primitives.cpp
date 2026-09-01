#include "ui/primitives.hpp"

#include <algorithm>
#include <map>
#include <tuple>

#include "ui/theme.hpp"

namespace {
struct CachedText {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

using TextKey = std::tuple<SDL_Renderer*, TTF_Font*, Uint32, std::string>;
std::map<TextKey, CachedText> textCache;

std::string fitText(TTF_Font* font, std::string value, int maxWidth) {
    if (maxWidth <= 0) return value;
    int width = 0;
    if (TTF_SizeUTF8(font, value.c_str(), &width, nullptr) != 0 || width <= maxWidth) return value;
    while (!value.empty()) {
        size_t start = value.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0) == 0x80) --start;
        value.erase(start);
        const std::string candidate = value + "...";
        if (TTF_SizeUTF8(font, candidate.c_str(), &width, nullptr) == 0 && width <= maxWidth)
            return candidate;
    }
    return "...";
}
}  // namespace

void fillRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
void clearTextCache() {
    for (auto& [_, cached] : textCache)
        if (cached.texture) SDL_DestroyTexture(cached.texture);
    textCache.clear();
}
void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& value, int x, int y, SDL_Color c,
              int maxWidth, bool centered) {
    if (value.empty()) return;
    const std::string fitted = fitText(f, value, maxWidth);
    const Uint32 color = (static_cast<Uint32>(c.r) << 24) | (static_cast<Uint32>(c.g) << 16) |
                         (static_cast<Uint32>(c.b) << 8) | c.a;
    const TextKey key{r, f, color, fitted};
    auto found = textCache.find(key);
    if (found == textCache.end()) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(f, fitted.c_str(), c);
        if (!surface) return;
        CachedText cached{SDL_CreateTextureFromSurface(r, surface), surface->w, surface->h};
        SDL_FreeSurface(surface);
        if (!cached.texture) return;
        found = textCache.emplace(key, cached).first;
    }
    SDL_Rect target{x, y, found->second.width, found->second.height};
    if (centered) target.x = x - target.w / 2;
    SDL_RenderCopy(r, found->second.texture, nullptr, &target);
}
void drawMarqueeText(SDL_Renderer* r, TTF_Font* f, const std::string& value, const SDL_Rect& bounds,
                     SDL_Color c, Uint64 clock) {
    int width = 0, height = 0;
    if (TTF_SizeUTF8(f, value.c_str(), &width, &height) != 0) return;
    if (width <= bounds.w) {
        drawText(r, f, value, bounds.x + bounds.w / 2, bounds.y, c, 0, true);
        return;
    }
    constexpr int pause = 900;
    constexpr float speed = 42.0f;
    int travel = width - bounds.w, move = std::max(1, static_cast<int>(travel / speed * 1000));
    int phase = static_cast<int>(clock % (pause * 2 + move * 2));
    float offset = 0;
    if (phase > pause && phase <= pause + move)
        offset = travel * static_cast<float>(phase - pause) / move;
    else if (phase > pause + move && phase <= pause * 2 + move)
        offset = static_cast<float>(travel);
    else if (phase > pause * 2 + move)
        offset = travel * (1.0f - static_cast<float>(phase - pause * 2 - move) / move);
    SDL_Rect old;
    SDL_RenderGetClipRect(r, &old);
    SDL_RenderSetClipRect(r, &bounds);
    drawText(r, f, value, bounds.x - static_cast<int>(offset), bounds.y, c);
    SDL_RenderSetClipRect(r, old.w == 0 ? nullptr : &old);
}
void drawChevron(SDL_Renderer* r, int x, int y, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int t = -2; t <= 2; ++t) {
        SDL_RenderDrawLine(r, x - 5 + t, y - 12, x + 7 + t, y);
        SDL_RenderDrawLine(r, x + 7 + t, y, x - 5 + t, y + 12);
    }
}
void drawPlayState(SDL_Renderer* r, int x, int y, bool paused, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    if (paused) {
        SDL_Rect a{x, y, 5, 18}, b{x + 10, y, 5, 18};
        SDL_RenderFillRect(r, &a);
        SDL_RenderFillRect(r, &b);
        return;
    }
    for (int i = 0; i < 8; ++i) SDL_RenderDrawLine(r, x + i, y + i, x + i, y + 18 - i);
}
