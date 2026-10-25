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
void game_options_net_input_for(int localViewport, unsigned int buttons, int stickX, int stickY);
/* Packed local viewport owning the overlay (including a pending Start); -1 otherwise. */
int game_options_local_owner(void);
int game_options_is_live_network(void);
void game_options_request_back(void);
void game_options_scroll(int pages);
/* Local party, race setup, discovery and session pages use native menu drawing. */
int game_lobby_input(void);
void game_lobby_draw(void);
void game_lobby_open_online(void);
int game_lobby_is_open(void);
void game_lobby_request_back(void);
void game_lobby_scroll(int pages);
void game_lobby_net_input(unsigned int buttons, int stickX, int stickY);
void game_lobby_net_input_for(int localViewport, unsigned int buttons, int stickX, int stickY);
typedef struct {
    int open, step, localCount;
    int ports[4], characterLocked[4], transmissionLocked[4];
    int x[4], y[4], width[4], height[4];
} GameDriverSelectionInfo;
void game_lobby_driver_selection(GameDriverSelectionInfo* info);
int game_lobby_poll_race_load(void);
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
const char* game_lobby_page(void);
int game_lobby_context(void);
int game_lobby_row_count(void);
int game_lobby_row_info(int index, GameOptionRowInfo* info);

#ifdef __cplusplus
}
namespace GameUI {
void DrawOptionsBindings(float x, float y, float width, float height);
bool OptionsBindingsOpen();
}
#endif
