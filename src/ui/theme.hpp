#pragma once
#include <SDL.h>

struct ThemePalette {
    SDL_Color background;
    SDL_Color surface;
    SDL_Color surfaceRaised;
    SDL_Color text;
    SDL_Color textMuted;
    SDL_Color accent;
    SDL_Color accentSoft;
};

ThemePalette resolveTheme();
