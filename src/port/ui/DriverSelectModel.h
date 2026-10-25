#pragma once
#include <array>

namespace GameUI::DriverSelect {
struct Rect { int x, y, w, h; };

// Index is the compact local-player index, not the physical controller port.
constexpr Rect Viewport(int count, int index) {
    if (count == 1) return {0, 0, 320, 240};
    if (count == 2) return {0, index * 120, 320, 120};
    return {index % 2 * 160, index / 2 * 120, 160, 120};
}

constexpr Rect CharacterTile(int character) {
    return {16 + 74 * (character % 4), 43 + 72 * (character / 4), 66, 66};
}

constexpr Rect AutoButton(int count, int index) {
    const auto view = Viewport(count, index);
    return {view.x + view.w / 2 - 68, view.y + view.h / 2 - 11, 64, 26};
}

constexpr Rect ManualButton(int count, int index) {
    const auto view = Viewport(count, index);
    return {view.x + view.w / 2 + 4, view.y + view.h / 2 - 11, 64, 26};
}

constexpr Rect ConfirmButton(int count, int index) {
    const auto view = Viewport(count, index);
    return {view.x + view.w / 2 - 26, view.y + view.h / 2 + 23, 52, 24};
}

constexpr Rect ReadyButton(int count, int index) {
    const auto view = Viewport(count, index);
    return {view.x + view.w / 2 - 44, view.y + view.h / 2 + 8, 88, 28};
}

constexpr Rect BackButton(int count, int index) {
    const auto view = Viewport(count, index);
    return {view.x + 6, view.y + view.h - 22, 44, 16};
}

enum class Step { Character, Transmission, Ready };

struct Flow {
    unsigned mask = 0;
    Step step = Step::Character;
    std::array<bool, 4> characterLocked{}, transmissionLocked{};

    void Reset(unsigned joinedMask) {
        mask = joinedMask & 15u;
        step = Step::Character;
        characterLocked.fill(false);
        transmissionLocked.fill(false);
    }

    bool ConfirmCharacter(int port) {
        if (!Joined(port) || step != Step::Character || characterLocked[port]) return false;
        characterLocked[port] = true;
        if (AllLocked(characterLocked)) step = Step::Transmission;
        return true;
    }

    bool ConfirmTransmission(int port) {
        if (!Joined(port) || step != Step::Transmission || transmissionLocked[port]) return false;
        transmissionLocked[port] = true;
        if (AllLocked(transmissionLocked)) step = Step::Ready;
        return true;
    }

    bool Back(int port) {
        if (!Joined(port)) return false;
        if (step == Step::Ready) {
            step = Step::Transmission;
            transmissionLocked[port] = false;
            return true;
        }
        if (step == Step::Transmission) {
            step = Step::Character;
            characterLocked[port] = false;
            transmissionLocked.fill(false);
            return true;
        }
        const bool changed = characterLocked[port];
        characterLocked[port] = false;
        return changed;
    }

private:
    bool Joined(int port) const {
        return port >= 0 && port < 4 && (mask & (1u << port)) != 0;
    }

    bool AllLocked(const std::array<bool, 4>& locks) const {
        if (mask == 0) return false;
        for (int port = 0; port < 4; ++port)
            if ((mask & (1u << port)) && !locks[port]) return false;
        return true;
    }
};
} // namespace GameUI::DriverSelect
