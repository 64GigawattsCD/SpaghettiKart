#ifndef KART_INPUT_H
#define KART_INPUT_H

#include <stdbool.h>
#include <common_structs.h>

typedef enum {
    KART_INPUT_THROTTLE,
    KART_INPUT_BRAKE,
    KART_INPUT_JUMP,
    KART_INPUT_DRIFT,
    KART_INPUT_USE_ITEM,
} KartInputCommand;

f32 kart_input_get_command_value(const struct Controller* controller, KartInputCommand command);
bool kart_input_is_command_active(const struct Controller* controller, KartInputCommand command);
bool kart_input_was_command_pressed(const struct Controller* controller, KartInputCommand command);
void kart_input_consume_command_press(struct Controller* controller, KartInputCommand command);

#endif
