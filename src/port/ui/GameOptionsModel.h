#pragma once
#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace GameUI::Options {
enum class Page {
    Pause, Root, General, Display, Audio, Controls, Camera, Hud, Effects,
    Rules, Cheats, Network, Address, Saves, Bindings
};

struct Cursor {
    static constexpr int PageRows = 6;
    int selected = 0;
    void Clamp(int count) { selected = std::clamp(selected, 0, std::max(count - 1, 0)); }
    void Move(int direction, int count) {
        if (count <= 0) { selected = 0; return; }
        Clamp(count);
        selected = (selected + (direction < 0 ? count - 1 : 1)) % count;
    }
    void Scroll(int direction, int count) {
        selected += direction * PageRows;
        Clamp(count);
    }
    int First() const { return selected / PageRows * PageRows; }
};

inline int CycleKey(const std::vector<int>& keys, int current, int direction) {
    if (keys.empty()) return current;
    auto at = std::find(keys.begin(), keys.end(), current);
    if (at == keys.end()) return direction < 0 ? keys.back() : keys.front();
    int index = static_cast<int>(at - keys.begin());
    return keys[(index + (direction < 0 ? keys.size() - 1 : 1)) % keys.size()];
}

inline float Adjust(float current, float fallback, float minimum, float maximum, float step, int direction) {
    if (!std::isfinite(current)) current = fallback;
    if (!std::isfinite(step) || step <= 0) step = .01f;
    return std::clamp(current + step * (direction ? std::clamp(direction, -10, 10) : 1), minimum, maximum);
}

inline std::string FontText(std::string_view input) {
    std::string text;
    bool space = false;
    for (unsigned char c : input) {
        if (c >= 128) continue;
        if (std::isalnum(c) || c == '.' || c == '-' || c == '+' || c == '/' || c == ':') {
            if (space && !text.empty()) text += ' ';
            text += static_cast<char>(std::toupper(c));
            space = false;
        } else {
            space = true;
        }
    }
    return text;
}

inline Page Classify(std::string_view section, std::string_view sidebar, std::string_view cvar) {
    if (sidebar == "Audio") return Page::Audio;
    if (sidebar == "Graphics") return Page::Display;
    if (sidebar == "Controls") return Page::Controls;
    if (sidebar == "Camera" || sidebar == "VR") return Page::Camera;
    if (sidebar == "Rulesets") return Page::Rules;
    if (sidebar == "Cheats") return Page::Cheats;
    if (cvar.find("gPhone.") == 0 || cvar.find("Hud") != std::string_view::npos ||
        cvar.find("HUD") != std::string_view::npos || cvar.find("RpmDisplay") != std::string_view::npos ||
        cvar == "gEnableDigitalSpeedometer" || cvar == "gShowSpaghettiVersion") return Page::Hud;
    if (cvar.find("Wheel") != std::string_view::npos || cvar.find("Logitech") != std::string_view::npos ||
        cvar.find("Transmission") != std::string_view::npos ||
        cvar.find("AutoTransmission") != std::string_view::npos || cvar.find("Rumble") != std::string_view::npos)
        return Page::Controls;
    if (cvar.find("gArcadeKart.Camera.") == 0 || cvar == "gNoLookBehind") return Page::Camera;
    if (cvar.find("PostFx") != std::string_view::npos) return Page::Effects;
    if (section == "Enhancements") return Page::Rules;
    return Page::General;
}
} // namespace GameUI::Options

namespace GameUI::Lobby {
inline int Wrap(int index, int count) {
    return count > 0 ? (index % count + count) % count : 0;
}
inline int PageFirst(int selected, int rows = 6) { return std::max(0, selected) / rows * rows; }
inline bool PartyFits(int party, int occupied, int capacity) {
    return party > 0 && party <= 4 && occupied >= 0 && capacity <= 8 && occupied + party <= capacity;
}
inline bool BrowserJoinable(int party, int occupied, int capacity, bool compatible, int phase) {
    return compatible && phase == 2 && PartyFits(party, occupied, capacity);
}
inline std::string HiddenText(size_t size) { return size ? std::string(std::min<size_t>(size, 16), '-') : "NONE"; }
inline int KeyboardMove(int key, int horizontal, int vertical) {
    key = std::clamp(key, 0, 43);
    if (horizontal) return key < 40 ? key / 10 * 10 + Wrap(key % 10 + horizontal, 10) : 40 + Wrap(key - 40 + horizontal, 4);
    if (vertical > 0) return key >= 40 ? (key - 40) * 3 : key >= 30 ? 40 + std::min(3, key % 10 / 3) : key + 10;
    if (vertical < 0) return key >= 40 ? 30 + (key - 40) * 3 : key < 10 ? 40 + std::min(3, key / 3) : key - 10;
    return key;
}
inline bool ValidPort(std::string_view text) {
    if (text.empty() || text.size() > 5) return false;
    unsigned number = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        number = number * 10 + unsigned(c - '0');
    }
    return number > 0 && number <= 65535;
}
inline int PortKeyboardMove(int key, int horizontal, int vertical) {
    int position = key >= 27 && key <= 35 ? key - 27 : key == 40 ? 9 : key == 43 ? 11 : 10;
    if (horizontal) position = position / 3 * 3 + Wrap(position % 3 + horizontal, 3);
    else if (vertical) position = Wrap(position + vertical * 3, 12);
    return position < 9 ? position + 27 : position == 9 ? 40 : position == 10 ? 26 : 43;
}
} // namespace GameUI::Lobby
