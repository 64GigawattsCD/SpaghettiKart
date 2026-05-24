#include "kart_input.h"

#include <defines.h>

static u16 kart_input_get_legacy_button(KartInputCommand command) {
    switch (command) {
        case KART_INPUT_THROTTLE:
            return A_BUTTON;
        case KART_INPUT_BRAKE:
            return B_BUTTON;
        case KART_INPUT_JUMP:
            return R_TRIG;
        case KART_INPUT_DRIFT:
            return R_TRIG;
    }

    return 0;
}

f32 kart_input_get_command_value(const struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return 0.0f;
    }

    return (controller->button & kart_input_get_legacy_button(command)) ? 1.0f : 0.0f;
}

bool kart_input_is_command_active(const struct Controller* controller, KartInputCommand command) {
    return kart_input_get_command_value(controller, command) > 0.0f;
}

bool kart_input_was_command_pressed(const struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return false;
    }

    return (controller->buttonPressed & kart_input_get_legacy_button(command)) != 0;
}
