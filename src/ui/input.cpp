#include "ui/input.hpp"

#include "app/navigation.hpp"

std::optional<InputAction> keyboardInputAction(SDL_Keycode code) {
    switch (code) {
        case SDLK_UP:
            return InputAction::Up;
        case SDLK_DOWN:
            return InputAction::Down;
        case SDLK_LEFT:
            return InputAction::SeekBack;
        case SDLK_RIGHT:
            return InputAction::SeekForward;
        case SDLK_RETURN:
        case SDLK_a:
            return InputAction::Confirm;
        case SDLK_ESCAPE:
        case SDLK_b:
            return InputAction::Back;
        case SDLK_SPACE:
        case SDLK_s:
            return InputAction::PlayPause;
        case SDLK_q:
            return InputAction::Previous;
        case SDLK_e:
            return InputAction::Next;
        case SDLK_x:
            return InputAction::NowPlaying;
        case SDLK_y:
            return InputAction::Shuffle;
        case SDLK_r:
            return InputAction::Repeat;
        default:
            return std::nullopt;
    }
}

std::optional<InputAction> controllerInputAction(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return InputAction::Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return InputAction::Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return InputAction::SeekBack;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return InputAction::SeekForward;
        case SDL_CONTROLLER_BUTTON_A:
            return InputAction::Back;
        case SDL_CONTROLLER_BUTTON_B:
            return InputAction::Confirm;
        case SDL_CONTROLLER_BUTTON_START:
            return InputAction::PlayPause;
        case SDL_CONTROLLER_BUTTON_BACK:
            return InputAction::Repeat;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return InputAction::Previous;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return InputAction::Next;
        case SDL_CONTROLLER_BUTTON_X:
            return InputAction::Shuffle;
        case SDL_CONTROLLER_BUTTON_Y:
            return InputAction::NowPlaying;
        default:
            return std::nullopt;
    }
}

bool isRepeatable(InputAction action) {
    return action == InputAction::Up || action == InputAction::Down ||
           action == InputAction::SeekBack || action == InputAction::SeekForward;
}

void handleInputAction(AppState& app, InputAction action) {
    const auto& update = app.updates.state();
    if (update.modalVisible()) {
        if (update.cancellable() && action == InputAction::Back) app.updates.cancel();
        return;
    }
    if (app.exitConfirmationOpen) {
        switch (action) {
            case InputAction::Up:
            case InputAction::SeekBack:
                app.exitConfirmationSelection = 0;
                break;
            case InputAction::Down:
            case InputAction::SeekForward:
                app.exitConfirmationSelection = 1;
                break;
            case InputAction::Confirm:
                if (app.exitConfirmationSelection == 1)
                    app.running = false;
                else
                    app.exitConfirmationOpen = false;
                break;
            case InputAction::Back:
                app.exitConfirmationOpen = false;
                break;
            default:
                break;
        }
        return;
    }
    const int itemCount = static_cast<int>(app.view.itemCount());
    switch (action) {
        case InputAction::Up:
            if (itemCount > 0) app.view.selected = (app.view.selected - 1 + itemCount) % itemCount;
            break;
        case InputAction::Down:
            if (itemCount > 0) app.view.selected = (app.view.selected + 1) % itemCount;
            break;
        case InputAction::Confirm:
            selectCurrentItem(app);
            break;
        case InputAction::Back:
            navigateBack(app);
            break;
        case InputAction::PlayPause:
            if (app.playback.snapshot().phase == PlaybackPhase::Error)
                app.playback.retry();
            else
                app.playback.togglePause();
            break;
        case InputAction::SeekBack:
            app.playback.seekRelative(-10);
            break;
        case InputAction::SeekForward:
            app.playback.seekRelative(10);
            break;
        case InputAction::Previous:
            playAdjacentTrack(app, -1);
            break;
        case InputAction::Next:
            playAdjacentTrack(app, 1);
            break;
        case InputAction::NowPlaying:
            openNowPlaying(app);
            break;
        case InputAction::Shuffle:
            app.playback.setShuffle(!app.playback.shuffle());
            break;
        case InputAction::Repeat:
            app.playback.setRepeatMode(
                static_cast<RepeatMode>((static_cast<int>(app.playback.repeatMode()) + 1) % 3));
            break;
    }
}
