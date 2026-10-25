#include "../../src/kart_texture_layout.h"
#include <cstdlib>
#include <iostream>
#include <set>

int main() {
    int textures[KART_TEXTURE_BUFFERS][KART_TEXTURE_SCREENS][KART_TEXTURE_RACERS]{};
    std::set<const int*> addresses;
    for (int buffer = -1; buffer <= 2; ++buffer)
        for (int screen = -1; screen <= 4; ++screen)
            for (int racer = -1; racer <= 8; ++racer) {
                bool expected = buffer >= 0 && buffer < 2 && screen >= 0 && screen < 4 && racer >= 0 && racer < 8;
                if (bool(kart_texture_slot_valid(buffer, screen, racer)) != expected) return EXIT_FAILURE;
                if (expected && !addresses.insert(&textures[buffer][screen][racer]).second) return EXIT_FAILURE;
            }
    if (addresses.size() != 64) return EXIT_FAILURE;
    std::cout << "Two buffers, four viewports and eight racers have independent texture slots\n";
}
