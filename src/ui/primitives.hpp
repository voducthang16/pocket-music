#pragma once
#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
void fillRect(SDL_Renderer*, const SDL_Rect&, SDL_Color);
void clearTextCache();
void drawText(SDL_Renderer*, TTF_Font*, const std::string&, int, int, SDL_Color, int = 0,
              bool = false);
void drawMarqueeText(SDL_Renderer*, TTF_Font*, const std::string&, const SDL_Rect&, SDL_Color,
                     Uint64);
void drawChevron(SDL_Renderer*, int, int, SDL_Color);
void drawPlayState(SDL_Renderer*, int, int, bool, SDL_Color);
