#include "kart_input.h"

#include <defines.h>

static u16 kart_input_get_legacy_button(KartInputCommand command) {
    switch (command) {
        case KART_INPUT_THROTTLE:
            return A_BUTTON;
        case KART_INPUT_BRAKE:
            return B_BUTTON;
        case KART_INPUT_JUMP:
            return L_TRIG;
        case KART_INPUT_DRIFT:
            return R_TRIG;
        case KART_INPUT_USE_ITEM:
            return Z_TRIG;
        case KART_INPUT_MENU_CONFIRM:
            return KART_MENU_CONFIRM_BUTTON;
        case KART_INPUT_MENU_CANCEL:
            return KART_MENU_CANCEL_BUTTON;
    }

    return 0;
}

static f32 kart_input_get_analog_value(const struct Controller* controller, KartInputCommand command) {
    switch (command) {
        case KART_INPUT_THROTTLE:
            return controller->rightTrigger;
        case KART_INPUT_BRAKE:
            return controller->leftTrigger;
        default:
            break;
    }

    return 0.0f;
}

f32 kart_input_get_command_value(const struct Controller* controller, KartInputCommand command) {
    f32 analogValue;

    if (controller == NULL) {
        return 0.0f;
    }

    analogValue = kart_input_get_analog_value(controller, command);
    if (analogValue > 0.0f) {
        if (analogValue > 1.0f) {
            return 1.0f;
        }
        return analogValue;
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

void kart_input_consume_command_press(struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return;
    }

    controller->buttonPressed &= ~kart_input_get_legacy_button(command);
}

u16 kart_input_get_menu_pressed(const struct Controller* controller) {
    u16 pressed;

    if (controller == NULL) {
        return 0;
    }

    pressed = (controller->buttonPressed & ~(A_BUTTON | B_BUTTON)) | controller->stickPressed;
    if (kart_input_was_command_pressed(controller, KART_INPUT_MENU_CONFIRM)) {
        pressed |= A_BUTTON;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_MENU_CANCEL)) {
        pressed |= B_BUTTON;
    }

    return pressed;
}
