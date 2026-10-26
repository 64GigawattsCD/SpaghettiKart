#ifndef HIT_RADIO_H
#define HIT_RADIO_H

#ifdef __cplusplus
extern "C" {
#endif

void hit_radio_reset(void);
/* Compact local viewport index, not a controller port or network racer ID.
 * Returns 1 only for an accepted event; the caller owns paired audio. */
int hit_radio_show(int localVictimSlot, int attackerCharacter);
void hit_radio_tick(float deltaSeconds);
void hit_radio_draw(int playerId);

#ifdef __cplusplus
}
#endif
#endif
