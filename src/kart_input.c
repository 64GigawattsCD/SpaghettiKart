#include "kart_input.h"

#include <defines.h>

static u32 kart_input_get_legacy_button(KartInputCommand command) {
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
        case KART_INPUT_USE_ITEM_FORWARD:
            return KART_USE_ITEM_FORWARD_BUTTON;
        case KART_INPUT_USE_ITEM_BACKWARD:
            return KART_USE_ITEM_BACKWARD_BUTTON;
        case KART_INPUT_MENU_CONFIRM:
            return KART_MENU_CONFIRM_BUTTON;
        case KART_INPUT_MENU_CANCEL:
            return KART_MENU_CANCEL_BUTTON;
        case KART_INPUT_CLUTCH:
            return KART_CLUTCH_BUTTON;
        case KART_INPUT_SHIFT_GEAR_1:
            return KART_SHIFT_GEAR_1_BUTTON;
        case KART_INPUT_SHIFT_GEAR_2:
            return KART_SHIFT_GEAR_2_BUTTON;
        case KART_INPUT_SHIFT_GEAR_3:
            return KART_SHIFT_GEAR_3_BUTTON;
        case KART_INPUT_SHIFT_GEAR_4:
            return KART_SHIFT_GEAR_4_BUTTON;
        case KART_INPUT_SHIFT_GEAR_5:
            return KART_SHIFT_GEAR_5_BUTTON;
        case KART_INPUT_SHIFT_GEAR_6:
            return KART_SHIFT_GEAR_6_BUTTON;
        case KART_INPUT_SHIFT_REVERSE:
            return KART_SHIFT_REVERSE_BUTTON;
        case KART_INPUT_SHIFT_NEUTRAL:
            return KART_SHIFT_NEUTRAL_BUTTON;
        case KART_INPUT_TOGGLE_MUSIC:
            return KART_TOGGLE_MUSIC_BUTTON;
        case KART_INPUT_SHIFT_GEAR_UP:
            return KART_SHIFT_GEAR_UP_BUTTON;
        case KART_INPUT_SHIFT_GEAR_DOWN:
            return KART_SHIFT_GEAR_DOWN_BUTTON;
        case KART_INPUT_TOGGLE_HUD:
            return KART_TOGGLE_HUD_BUTTON;
        case KART_INPUT_CAPTURE_SCREENSHOT:
            return KART_CAPTURE_SCREENSHOT_BUTTON;
    }

    return 0;
}

static f32 kart_input_get_analog_value(const struct Controller* controller, KartInputCommand command) {
    u32 button;

    switch (command) {
        case KART_INPUT_THROTTLE:
            return controller->rightTrigger;
        case KART_INPUT_BRAKE:
            return controller->leftTrigger;
        default:
            break;
    }

    button = kart_input_get_legacy_button(command);
    for (s32 i = 0; i < CONTROLLER_BUTTON_VALUE_COUNT; i++) {
        if (button == (u32)(1u << i)) {
            return controller->buttonValue[i];
        }
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

bool kart_input_was_command_released(const struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return false;
    }

    return (controller->buttonDepressed & kart_input_get_legacy_button(command)) != 0;
}

void kart_input_consume_command_press(struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return;
    }

    controller->buttonPressed &= ~kart_input_get_legacy_button(command);
}

void kart_input_consume_command_release(struct Controller* controller, KartInputCommand command) {
    if (controller == NULL) {
        return;
    }

    controller->buttonDepressed &= ~kart_input_get_legacy_button(command);
}

f32 kart_input_get_forward_backward_axis(const struct Controller* controller) {
    f32 axis;

    if (controller == NULL) {
        return 0.0f;
    }

    axis = controller->rawStickY / 85.0f;
    if (axis > 1.0f) {
        return 1.0f;
    }
    if (axis < -1.0f) {
        return -1.0f;
    }
    return axis;
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
