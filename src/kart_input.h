#ifndef KART_INPUT_H
#define KART_INPUT_H

#include <stdbool.h>
#include <common_structs.h>

#define KART_MENU_CONFIRM_BUTTON 0x0040
#define KART_MENU_CANCEL_BUTTON 0x0080
#define KART_CLUTCH_BUTTON 0x00010000
#define KART_SHIFT_GEAR_1_BUTTON 0x00020000
#define KART_SHIFT_GEAR_2_BUTTON 0x00040000
#define KART_SHIFT_GEAR_3_BUTTON 0x00080000
#define KART_SHIFT_GEAR_4_BUTTON 0x00100000
#define KART_SHIFT_GEAR_5_BUTTON 0x00200000
#define KART_SHIFT_GEAR_6_BUTTON 0x00400000
#define KART_SHIFT_REVERSE_BUTTON 0x00800000
#define KART_SHIFT_NEUTRAL_BUTTON 0x01000000
#define KART_USE_ITEM_FORWARD_BUTTON 0x02000000
#define KART_USE_ITEM_BACKWARD_BUTTON 0x04000000
#define KART_TOGGLE_MUSIC_BUTTON 0x08000000
#define KART_SHIFT_GEAR_UP_BUTTON 0x10000000
#define KART_SHIFT_GEAR_DOWN_BUTTON 0x20000000
#define KART_TOGGLE_HUD_BUTTON 0x40000000
#define KART_CAPTURE_SCREENSHOT_BUTTON 0x80000000

typedef enum {
    KART_INPUT_THROTTLE,
    KART_INPUT_BRAKE,
    KART_INPUT_JUMP,
    KART_INPUT_DRIFT,
    KART_INPUT_USE_ITEM,
    KART_INPUT_USE_ITEM_FORWARD,
    KART_INPUT_USE_ITEM_BACKWARD,
    KART_INPUT_MENU_CONFIRM,
    KART_INPUT_MENU_CANCEL,
    KART_INPUT_CLUTCH,
    KART_INPUT_SHIFT_GEAR_1,
    KART_INPUT_SHIFT_GEAR_2,
    KART_INPUT_SHIFT_GEAR_3,
    KART_INPUT_SHIFT_GEAR_4,
    KART_INPUT_SHIFT_GEAR_5,
    KART_INPUT_SHIFT_GEAR_6,
    KART_INPUT_SHIFT_REVERSE,
    KART_INPUT_SHIFT_NEUTRAL,
    KART_INPUT_TOGGLE_MUSIC,
    KART_INPUT_SHIFT_GEAR_UP,
    KART_INPUT_SHIFT_GEAR_DOWN,
    KART_INPUT_TOGGLE_HUD,
    KART_INPUT_CAPTURE_SCREENSHOT,
} KartInputCommand;

f32 kart_input_get_command_value(const struct Controller* controller, KartInputCommand command);
bool kart_input_is_command_active(const struct Controller* controller, KartInputCommand command);
bool kart_input_was_command_pressed(const struct Controller* controller, KartInputCommand command);
bool kart_input_was_command_released(const struct Controller* controller, KartInputCommand command);
void kart_input_consume_command_press(struct Controller* controller, KartInputCommand command);
void kart_input_consume_command_release(struct Controller* controller, KartInputCommand command);
f32 kart_input_get_forward_backward_axis(const struct Controller* controller);
u16 kart_input_get_menu_pressed(const struct Controller* controller);

#endif
