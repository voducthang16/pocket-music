#include "ui/input.hpp"

#include "app/navigation.hpp"
#include "ui/layout.hpp"
void handleKey(AppState& app, SDL_Keycode code) {
    if (app.exitConfirmationOpen) {
        switch (code) {
            case SDLK_LEFT:
            case SDLK_UP:
                app.exitConfirmationSelection = 0;
                break;
            case SDLK_RIGHT:
            case SDLK_DOWN:
                app.exitConfirmationSelection = 1;
                break;
            case SDLK_RETURN:
            case SDLK_a:
                if (app.exitConfirmationSelection == 1)
                    app.running = false;
                else
                    app.exitConfirmationOpen = false;
                break;
            case SDLK_ESCAPE:
            case SDLK_b:
                app.exitConfirmationOpen = false;
                break;
            default:
                break;
        }
        return;
    }
    const auto& items = app.view.items;
    switch (code) {
        case SDLK_UP:
            if (!items.empty())
                app.view.selected = (app.view.selected - 1 + static_cast<int>(items.size())) %
                                    static_cast<int>(items.size());
            break;
        case SDLK_DOWN:
            if (!items.empty())
                app.view.selected = (app.view.selected + 1) % static_cast<int>(items.size());
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
            if (app.playback.snapshot().phase == PlaybackPhase::Error)
                app.playback.retry();
            else
                app.playback.togglePause();
            break;
        case SDLK_LEFT:
            app.playback.seekRelative(-10);
            break;
        case SDLK_RIGHT:
            app.playback.seekRelative(10);
            break;
        case SDLK_q:
            playAdjacentTrack(app, -1);
            break;
        case SDLK_e:
            playAdjacentTrack(app, 1);
            break;
        case SDLK_x:
            openNowPlaying(app);
            break;
        case SDLK_y:
            app.playback.setShuffle(!app.playback.shuffle());
            break;
        case SDLK_r:
            app.playback.setRepeatMode(
                static_cast<RepeatMode>((static_cast<int>(app.playback.repeatMode()) + 1) % 3));
            break;
        default:
            break;
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
            handleKey(app, SDLK_ESCAPE);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            handleKey(app, SDLK_RETURN);
            break;
        case SDL_CONTROLLER_BUTTON_START:
            handleKey(app, SDLK_SPACE);
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            handleKey(app, SDLK_r);
            break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            handleKey(app, SDLK_q);
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            handleKey(app, SDLK_e);
            break;
        case SDL_CONTROLLER_BUTTON_X:
            handleKey(app, SDLK_y);
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            handleKey(app, SDLK_x);
            break;
        default:
            break;
    }
}
