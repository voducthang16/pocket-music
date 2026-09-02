#include "test_suites.hpp"
#include "ui/layout.hpp"
#include "ui/primitives.hpp"

namespace {
SDL_Color pixelAt(SDL_Renderer* renderer, int x, int y) {
    Uint32 pixel = 0;
    const SDL_Rect sample{x, y, 1, 1};
    require(SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA8888, &pixel,
                                 sizeof(pixel)) == 0,
            "rounded rectangle pixels must be readable");
    SDL_Color color{};
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    SDL_GetRGBA(pixel, format, &color.r, &color.g, &color.b, &color.a);
    SDL_FreeFormat(format);
    return color;
}
}  // namespace

void addLayoutTests(TestCases& tests) {
    tests.push_back({"TrimUI layout uses the native landscape canvas", [] {
                         require(layout::width == 1024, "logical width must match the display");
                         require(layout::height == 768, "logical height must match the display");
                     }});
    tests.push_back({"Library layout preserves the Hello Kitty artwork", [] {
                         require(layout::contentRowsY <= 144,
                                 "first Library row must anchor the upper screen");
                         require(layout::contentRowsX + layout::contentRowsWidth <= 620,
                                 "Library rows must stay inside the quiet left region");
                         require(layout::miniPlayerX == 0,
                                 "mini-player must align with the left screen edge");
                         require(layout::miniPlayerWidth == layout::width,
                                 "mini-player must span the full screen width");
                         require(layout::miniPlayerY >= 520,
                                 "mini-player must begin below the cassette artwork");
                         require(layout::contentRowsY + layout::contentRowHeight * 5 <
                                     layout::miniPlayerY,
                                 "Library rows and mini-player must not overlap");
                         require(layout::miniPlayerY + layout::miniPlayerHeight <=
                                     layout::height - layout::footerHeight,
                                 "mini-player must stay above the controls");
                         require(layout::footerTextY -
                                         (layout::miniPlayerY + layout::miniPlayerHeight) >=
                                     18 &&
                                     layout::footerTextY -
                                             (layout::miniPlayerY + layout::miniPlayerHeight) <=
                                         24,
                                     "footer must retain breathing room below the mini-player");
                     }});
    tests.push_back({"List screens share the Library safe content region", [] {
                         require(layout::contentRowsX == 48 && layout::contentRowsWidth == 560,
                                 "all compact rows must share the artwork safe area");
                         require(layout::contentRowsY + layout::contentRowHeight * 5 <
                                     layout::miniPlayerY,
                                     "five list rows must stay clear of the mini-player");
                     }});
    tests.push_back({"Now Playing preserves artwork and transport regions", [] {
                         require(layout::nowPlayingCoverX + layout::nowPlayingCoverSize <= 620,
                                 "Now Playing cover must stay clear of Hello Kitty");
                         require(layout::nowPlayingCoverY + layout::nowPlayingCoverSize <
                                     layout::miniPlayerY,
                                 "Now Playing cover must stay above the transport strip");
                         require(layout::nowPlayingMetadataX + layout::nowPlayingMetadataWidth <=
                                     620,
                                 "Now Playing metadata must stay inside the quiet left region");
                         require(layout::nowPlayingTitleWidth > layout::nowPlayingMetadataWidth &&
                                     layout::nowPlayingMetadataX +
                                             layout::nowPlayingTitleWidth <=
                                         820,
                                 "Now Playing title may use the clear space above Hello Kitty");
                     }});
    tests.push_back({"message banner stays outside the playback surface", [] {
                         require(layout::messageBannerX + layout::messageBannerWidth <= 620,
                                 "message banner must stay clear of Hello Kitty");
                         require(layout::messageBannerY + layout::messageBannerHeight <=
                                     layout::miniPlayerY,
                                 "message banner must not cover the playback surface");
                     }});
    tests.push_back({"rounded surfaces keep solid side edges", [] {
                         SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
                             0, 64, 48, 32, SDL_PIXELFORMAT_RGBA8888);
                         require(surface != nullptr, "rounded rectangle surface must be created");
                         SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
                         require(renderer != nullptr,
                                 "rounded rectangle software renderer must be created");
                         SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
                         SDL_RenderClear(renderer);
                         fillRoundedRect(renderer, {12, 8, 40, 32}, 8, {229, 103, 111, 255});
                         const auto side = pixelAt(renderer, 12, 24);
                         const auto corner = pixelAt(renderer, 12, 8);
                         require(side.r == 229 && side.a == 255,
                                 "rounded rectangle side center must be filled");
                         require(corner.a == 0, "rounded rectangle corner must stay transparent");
                         SDL_DestroyRenderer(renderer);
                         SDL_FreeSurface(surface);
                     }});
    tests.push_back({"translucent rounded surfaces use one uniform fill", [] {
                         SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
                             0, 80, 48, 32, SDL_PIXELFORMAT_RGBA8888);
                         require(surface != nullptr, "rounded rectangle surface must be created");
                         SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
                         require(renderer != nullptr,
                                 "rounded rectangle software renderer must be created");
                         SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                         SDL_SetRenderDrawColor(renderer, 251, 237, 232, 255);
                         SDL_RenderClear(renderer);
                         fillRoundedRect(renderer, {12, 8, 56, 32}, 8, {249, 207, 211, 128});
                         const auto edge = pixelAt(renderer, 12, 24);
                         const auto center = pixelAt(renderer, 40, 24);
                         require(edge.r == center.r && edge.g == center.g && edge.b == center.b,
                                 "transparent rounded fill must not darken in overlapping regions");
                         SDL_DestroyRenderer(renderer);
                         SDL_FreeSurface(surface);
                     }});
}
