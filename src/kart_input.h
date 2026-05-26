#ifndef KART_INPUT_H
#define KART_INPUT_H

#include <stdbool.h>
#include <common_structs.h>

#define KART_MENU_CONFIRM_BUTTON 0x0040
#define KART_MENU_CANCEL_BUTTON 0x0080

typedef enum {
    KART_INPUT_THROTTLE,
    KART_INPUT_BRAKE,
    KART_INPUT_JUMP,
    KART_INPUT_DRIFT,
    KART_INPUT_USE_ITEM,
    KART_INPUT_MENU_CONFIRM,
    KART_INPUT_MENU_CANCEL,
} KartInputCommand;

f32 kart_input_get_command_value(const struct Controller* controller, KartInputCommand command);
bool kart_input_is_command_active(const struct Controller* controller, KartInputCommand command);
bool kart_input_was_command_pressed(const struct Controller* controller, KartInputCommand command);
void kart_input_consume_command_press(struct Controller* controller, KartInputCommand command);
u16 kart_input_get_menu_pressed(const struct Controller* controller);

#endif
