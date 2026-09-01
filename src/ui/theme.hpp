#pragma once
#include <SDL.h>

#include "settings/preferences.hpp"

struct ThemePalette {
    SDL_Color background;
    SDL_Color surface;
    SDL_Color surfaceRaised;
    SDL_Color text;
    SDL_Color textMuted;
    SDL_Color divider;
    SDL_Color accent;
    SDL_Color accentSoft;
};

ThemePalette resolveTheme(ThemeMode mode);
