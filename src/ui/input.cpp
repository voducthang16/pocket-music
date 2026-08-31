#include "ui/input.hpp"

#include "app/navigation.hpp"
#include "ui/layout.hpp"
void handleKey(AppState& app, SDL_Keycode code) {
    const auto items = visibleLabels(app);
    switch (code) {
        case SDLK_UP:
            if (!items.empty())
                app.selected = (app.selected - 1 + static_cast<int>(items.size())) %
                               static_cast<int>(items.size());
            break;
        case SDLK_DOWN:
            if (!items.empty()) app.selected = (app.selected + 1) % static_cast<int>(items.size());
            break;
        case SDLK_RETURN:
        case SDLK_a:
            selectCurrentItem(app);
            break;
        case SDLK_ESCAPE:
        case SDLK_b:
            navigateBack(app);
            break;
        case SDLK_SPACE:
        case SDLK_s:
            app.player.togglePause();
            break;
        case SDLK_LEFT:
            app.player.seek(-10);
            break;
        case SDLK_RIGHT:
            app.player.seek(10);
            break;
        case SDLK_q:
            playAdjacentTrack(app, -1);
            break;
        case SDLK_e:
            playAdjacentTrack(app, 1);
            break;
        case SDLK_x:
            app.previous = app.screen;
            app.screen = Screen::NowPlaying;
            break;
        case SDLK_y:
            app.shuffle = !app.shuffle;
            break;
        case SDLK_r:
            app.repeatMode = (app.repeatMode + 1) % 3;
            break;
        default:
            break;
    }
}
void handleMouse(AppState& app, const SDL_MouseButtonEvent& event) {
    float x = 0, y = 0;
    SDL_RenderWindowToLogical(app.renderer, event.x, event.y, &x, &y);
    if (event.button == SDL_BUTTON_RIGHT) {
        navigateBack(app);
        return;
    }
    if (event.button != SDL_BUTTON_LEFT) return;
    if (app.screen == Screen::NowPlaying) {
        app.player.togglePause();
        return;
    }
    if (y < layout::headerHeight || x < 0 || x >= layout::width) return;
    auto items = visibleLabels(app);
    int index = app.scroll + static_cast<int>(y - layout::headerHeight) / layout::rowHeight;
    if (index >= 0 && index < static_cast<int>(items.size())) {
        app.selected = index;
        selectCurrentItem(app);
    }
}
void handleControllerButton(AppState& app, Uint8 b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            handleKey(app, SDLK_UP);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            handleKey(app, SDLK_DOWN);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            handleKey(app, SDLK_LEFT);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            handleKey(app, SDLK_RIGHT);
            break;
        case SDL_CONTROLLER_BUTTON_A:
            handleKey(app, SDLK_RETURN);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            handleKey(app, SDLK_ESCAPE);
            break;
        case SDL_CONTROLLER_BUTTON_START:
            handleKey(app, SDLK_SPACE);
            break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            handleKey(app, SDLK_q);
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            handleKey(app, SDLK_e);
            break;
        case SDL_CONTROLLER_BUTTON_X:
            handleKey(app, SDLK_x);
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            handleKey(app, SDLK_y);
            break;
        default:
            break;
    }
}
