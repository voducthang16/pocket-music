#include "ui/primitives.hpp"

#include <algorithm>

#include "ui/theme.hpp"

void fillRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
void strokeRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(r, &rect);
}
void verticalGradient(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color top, SDL_Color bottom) {
    for (int row = 0; row < rect.h; ++row) {
        float a = rect.h <= 1 ? 0.0f : static_cast<float>(row) / (rect.h - 1);
        SDL_Color c{static_cast<Uint8>(top.r + (bottom.r - top.r) * a),
                    static_cast<Uint8>(top.g + (bottom.g - top.g) * a),
                    static_cast<Uint8>(top.b + (bottom.b - top.b) * a), 255};
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLine(r, rect.x, rect.y + row, rect.x + rect.w - 1, rect.y + row);
    }
}
void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& value, int x, int y, SDL_Color c,
              int maxWidth, bool centered) {
    if (value.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, value.c_str(), c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect target{x, y, s->w, s->h};
    if (maxWidth > 0 && target.w > maxWidth) {
        target.h = std::max(1, target.h * maxWidth / target.w);
        target.w = maxWidth;
    }
    if (centered) target.x = x - target.w / 2;
    SDL_RenderCopy(r, t, nullptr, &target);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
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
void drawBattery(SDL_Renderer* r, int x, int y, int percent) {
    strokeRect(r, {x, y, 39, 18}, theme::textMuted);
    fillRect(r, {x + 40, y + 5, 3, 8}, theme::textMuted);
    fillRect(r, {x + 2, y + 2, std::clamp(percent, 0, 100) * 35 / 100, 14},
             percent > 20 ? theme::green : theme::blue);
}
