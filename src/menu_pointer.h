#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Targets use the centered native 320x240 coordinate space, including wide x. */
void menu_pointer_begin(void);
void menu_pointer_end(void);
void menu_pointer_clear_targets(void);
void menu_pointer_target(int context, int id, float x, float y, float width, float height);
int menu_pointer_take(int context);
void menu_pointer_inject(float x, float y, int down);
void menu_pointer_cancel(void);
int menu_pointer_count(void);
int menu_pointer_get(int index, int* context, int* id, float* x, float* y, float* w, float* h);
int menu_pointer_main_context(void);
int menu_pointer_character_context(void);
int menu_pointer_character_can_pick(int grid);
int menu_pointer_course_context(void);
void menu_pointer_draw_navigation(void);
#ifdef __cplusplus
}
#endif
