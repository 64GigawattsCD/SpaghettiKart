#pragma once
enum AndroidPhoneTouchMode {
    ANDROID_PHONE_TOUCH_HIDDEN = 0,
    ANDROID_PHONE_TOUCH_GAMEPLAY = 1,
    ANDROID_PHONE_TOUCH_PORT_GUI = 2,
    ANDROID_PHONE_TOUCH_MODE_MASK = 3
};
#ifdef __cplusplus
extern "C" {
#endif
void android_phone_merge_pad(void);
int android_phone_active(void);
int android_phone_touch_layout(void);
// Published by the game thread; callable without reading live game/GUI state.
int android_phone_touch_mode(void);
// Explicit kart_test + touch_dispatch_test session; SDL consumes dispatched events asynchronously.
int android_phone_touch_dispatch_test(void);
void android_phone_init(int argc, char** argv);
#ifdef __cplusplus
}
#endif
