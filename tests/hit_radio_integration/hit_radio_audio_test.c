#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int8_t s8;
typedef float Vec3f[3];
enum { NUM_PLAYERS = 8, PLAYER_HUMAN = 0x4000 };
#define SOUND_ARG_LOAD(a,b,c,d) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(c)<<8)|(u32)(d))
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static struct { int type, characterId; } gPlayers[8];
static int gPlayerCountSelection1, D_800EA108, active, running, handled, accepted;
static int eventCalls, originalCalls, radioCalls, showCalls, shownVictim, shownCharacter, heardVictim;
static u32 heardSound;
static Vec3f* radioPositions[4];
static float D_800EA1D4;
static u8 D_800EA1DC;
static int net_race_active(void) { return active; }
static int net_running(void) { return running; }
static int net_hit_taunt(int a, int v) { ++eventCalls; CHECK(a != v); return handled; }
static int hit_radio_show(int v, int c) { ++showCalls; shownVictim = v; shownCharacter = c; return accepted; }
static void func_800C90F4(u8 a, u32 sound) { ++originalCalls; heardVictim = a; heardSound = sound; }
static void play_sound(u32 sound, Vec3f* position, u8 victim, float* pitch, float* volume, s8* reverb) {
    ++radioCalls; heardSound = sound; heardVictim = victim;
    CHECK(victim < 4 && (*position)[0] == 0 && (*position)[1] == 0 && (*position)[2] == 0);
    CHECK(pitch == &D_800EA1D4 && volume == &D_800EA1D4 && reverb == (s8*)&D_800EA1DC);
    radioPositions[victim] = position;
}
#include "radio_audio.inc"

static void reset(void) {
    memset(gPlayers, 0, sizeof(gPlayers));
    for (int i = 0; i < 8; ++i) { gPlayers[i].characterId = 7 - i; gPlayers[i].type = PLAYER_HUMAN; }
    active = handled = D_800EA108 = eventCalls = originalCalls = radioCalls = showCalls = 0;
    running = accepted = 1; gPlayerCountSelection1 = 4;
    memset(radioPositions, 0, sizeof(radioPositions));
    shownVictim = shownCharacter = heardVictim = -1; heardSound = 0;
}
int main(void) {
    for (int count = 1; count <= 4; ++count) for (int victim = 0; victim < count; ++victim) {
        reset(); gPlayerCountSelection1 = count;
        play_character_hit_taunt(7, victim);
        CHECK(showCalls == 1 && radioCalls == 1 && !originalCalls && !eventCalls);
        CHECK(shownVictim == victim && shownCharacter == 0 && heardVictim == victim && heardSound == 0x29008006u);
    }
    reset(); play_character_hit_taunt(0, 0); play_character_hit_taunt(8, 0); play_character_hit_taunt(0, 8);
    CHECK(!showCalls && !originalCalls && !eventCalls);
    reset(); accepted = 0; play_character_hit_taunt(7, 0);
    CHECK(showCalls == 1 && !radioCalls && !originalCalls);
    reset(); D_800EA108 = 1; play_character_hit_taunt(7, 0);
    CHECK(!showCalls && !radioCalls && !originalCalls);
    reset(); gPlayers[1].type = 0; play_character_hit_taunt(0, 1);
    CHECK(originalCalls == 1 && !showCalls && heardSound == 0x29008076u);
    reset(); gPlayerCountSelection1 = 1; play_character_hit_taunt(0, 6);
    CHECK(originalCalls == 1 && !showCalls && heardVictim == 0);
    reset(); active = handled = 1; play_character_hit_taunt(0, 1);
    CHECK(eventCalls == 1 && !radioCalls && !originalCalls && !showCalls);
    reset(); active = 1; play_character_hit_taunt(0, 1);
    CHECK(eventCalls == 1 && originalCalls == 1 && !showCalls);
    reset(); active = 1; running = 0; play_character_hit_taunt(0, 1);
    CHECK(!eventCalls && !originalCalls && !showCalls);
    reset(); play_hit_radio_taunt(4, 0); play_hit_radio_taunt(0, 8);
    CHECK(!showCalls && !radioCalls);
    reset();
    for (int i = 0; i < 4; ++i) play_hit_radio_taunt(i, 2);
    CHECK(radioCalls == 4);
    for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j) CHECK(radioPositions[i] != radioPositions[j]);
    (*radioPositions[1])[1] = 100000.f;
    play_hit_radio_taunt(1, 2);
    CHECK((*radioPositions[1])[1] == 0 && radioCalls == 5);
    puts("Radio audio: local victim routing, stock fallback, online delegation, cooldown and transition gates passed");
    return 0;
}
