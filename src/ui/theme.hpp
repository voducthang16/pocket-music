#pragma once
#include <SDL.h>
namespace theme {
inline constexpr SDL_Color background{12, 14, 18, 255}, surface{24, 27, 34, 255};
inline constexpr SDL_Color surfaceRaised{34, 38, 47, 255};
inline constexpr SDL_Color text{244, 246, 250, 255}, textMuted{151, 158, 173, 255};
inline constexpr SDL_Color divider{48, 52, 62, 255};
inline constexpr SDL_Color accent{117, 153, 255, 255}, accentSoft{44, 58, 92, 255};
inline constexpr SDL_Color success{92, 205, 143, 255};
inline constexpr SDL_Color white{255, 255, 255, 255};
}  // namespace theme
