#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Native front-end and pause settings share the port's option definitions. */
int game_options_frontend_input(void);
int game_options_pause_input(void);
int game_options_is_open(void);
int game_options_is_frontend(void);
void game_options_draw(void);
void game_options_draw_entry(void);
void game_options_request(void);
void game_options_before_frame(void);
void game_options_net_input(unsigned int buttons, int stickX, int stickY);
int game_options_is_live_network(void);
void game_options_request_back(void);
void game_options_scroll(int pages);
/* Called once by the original Options handler before its save-dialog logic. */
int game_options_legacy_input(unsigned short* pressed);
typedef struct {
    char label[96], value[96];
    int index, enabled, adjustable, visible;
} GameOptionRowInfo;
const char* game_options_page(void);
int game_options_context(void);
int game_options_row_count(void);
int game_options_row_info(int index, GameOptionRowInfo* info);

#ifdef __cplusplus
}
namespace GameUI {
void DrawOptionsBindings(float x, float y, float width, float height);
bool OptionsBindingsOpen();
}
#endif
