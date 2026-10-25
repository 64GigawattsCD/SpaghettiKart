#pragma once

enum { KART_TEXTURE_BUFFERS = 2, KART_TEXTURE_SCREENS = 4, KART_TEXTURE_RACERS = 8 };
static inline int kart_texture_slot_valid(int buffer, int screen, int racer) {
    return buffer >= 0 && buffer < KART_TEXTURE_BUFFERS && screen >= 0 && screen < KART_TEXTURE_SCREENS &&
           racer >= 0 && racer < KART_TEXTURE_RACERS;
}
