#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace KartNet::ItemWire {
constexpr size_t MaxOpaque = 1100, ChunkHeader = 16, ChunkData = MaxOpaque - ChunkHeader;
constexpr size_t MaxSlots = 8192, MaxActors = 256, Racers = 8, MaxFrame = 32768, MaxParts = 32;
constexpr uint8_t Banana = 6, GreenShell = 7, RedShell = 8, ItemBox = 12, FakeBox = 13,
    BananaBunch = 14, TripleGreen = 21, TripleRed = 22, BlueShell = 42, BalloonBox = 43,
    Truck = 37, Boat = 38, Bus = 40, Tanker = 41, Car = 44, Kiwano = 45;

inline bool Shell(uint8_t type) { return type == GreenShell || type == RedShell || type == BlueShell; }
inline bool Triple(uint8_t type) { return type == TripleGreen || type == TripleRed; }
inline bool Item(uint8_t type) {
    return Shell(type) || Triple(type) || type == Banana || type == ItemBox || type == FakeBox ||
           type == BananaBunch || type == BalloonBox;
}
// Only layouts whose common fields are floats/shorts, never the pointer union in railroad crossings.
inline bool Supported(uint8_t type) {
    return Item(type) || type == 9 || (type >= 15 && type <= 17) || type == Truck || type == Boat ||
           type == Bus || type == Tanker || type == Car || type == Kiwano;
}

struct Actor {
    uint16_t index = 0, flags = 0, owner = 0, target = 0;
    uint8_t type = ItemBox;
    int16_t state = 0, timer = 0, parent = -1, param = 0;
    std::array<int16_t, 3> rotation{};
    std::array<float, 3> position{}, velocity{};
    float auxiliary = 0, radius = 0;
    std::array<int16_t, 5> links{{-1, -1, -1, -1, -1}};
};
struct Inventory {
    uint8_t participant = 1, present = 0, item = 0, icon = 0, nextItem = 0, mode = 0;
    int16_t state = 0, copiedItem = 0;
    uint16_t goldenTimer = 0;
    int32_t timer = 0;
    uint32_t status = 0;
    int8_t balloons = -1;
    std::array<uint16_t, 3> balloonStatus{};
};
struct Frame {
    uint32_t generation = 0;
    uint16_t slots = 0, active = 0, permanent = 0;
    std::vector<Actor> actors;
    std::array<Inventory, Racers> inventories{};
};

inline bool Newer(uint32_t a, uint32_t b) { const uint32_t d = a - b; return d != 0 && d < 0x80000000u; }
inline bool Finite(float f, float magnitude) {
    uint32_t bits; std::memcpy(&bits, &f, 4);
    return (bits & 0x7f800000u) != 0x7f800000u && std::fabs(f) <= magnitude;
}
inline bool Index(int16_t index, uint16_t slots) { return index == -1 || (index >= 0 && index < slots); }
inline bool Valid(const Actor& a, uint16_t slots) {
    if (a.index >= slots || !a.flags || !Supported(a.type) || a.owner > Racers || a.target > Racers ||
        !Index(a.parent, slots) || !Finite(a.auxiliary, 1000000) || !Finite(a.radius, 10000) || a.radius < 0)
        return false;
    for (float f : a.position) if (!Finite(f, 1000000)) return false;
    for (float f : a.velocity) if (!Finite(f, 1000000)) return false;
    for (int16_t i : a.links) if (!Index(i, slots)) return false;
    if (a.state < 0) return false;
    if (Shell(a.type) && (a.state > 9 || !a.owner ||
        ((a.state == 4 || a.state == 6) && a.parent < 0) ||
        (a.type != GreenShell && (a.state == 3 || a.state == 8 || a.state == 9) && !a.target))) return false;
    if (a.type == Banana && (a.state > 5 || (a.state != 5 && !a.owner))) return false;
    if (a.type == Banana && (a.state == 2 || a.state == 3) &&
        (a.parent < 0 || a.param < 0 || a.param > 4)) return false;
    if (a.type == FakeBox && (a.state > 2 || !a.owner || a.auxiliary < 0 || a.auxiliary > 16)) return false;
    if ((a.type == ItemBox || a.type == BalloonBox) && a.state > 6) return false;
    if (a.type == BananaBunch && (a.state > 6 || !a.owner || a.param < 0 || a.param > 5)) return false;
    if (Triple(a.type) && (a.state > 4 || !a.owner || a.param < 0 || a.param > 3)) return false;
    if (a.type == Kiwano && (!a.owner || a.state > 2 || a.rotation[0] < 0 || a.rotation[0] > 2 ||
        a.rotation[1] < 0 || a.rotation[1] > 7)) return false;
    return true;
}
inline bool Valid(const Inventory& i) {
    if (!i.participant || i.participant > Racers || i.present > 1 || i.item > 15 || i.icon > 15 ||
        i.nextItem > 15 || i.mode > 3 || i.copiedItem < 0 || i.copiedItem > 15 || i.state < 0 ||
        i.state > 50 || i.timer < -1 || i.timer > 0x1000000 || i.balloons < -1 || i.balloons > 2) return false;
    for (uint16_t s : i.balloonStatus) if (s > 3) return false;
    return true;
}
inline bool Valid(const Frame& f) {
    if (f.slots > MaxSlots || f.active > f.slots || f.permanent > f.slots || f.actors.size() > MaxActors ||
        f.actors.size() > f.active)
        return false;
    std::array<bool, MaxSlots> indices{};
    for (const auto& a : f.actors) {
        if (!Valid(a, f.slots) || indices[a.index]) return false;
        indices[a.index] = true;
    }
    unsigned participants = 0;
    for (const auto& i : f.inventories) {
        if (!Valid(i) || (participants & (1u << (i.participant - 1)))) return false;
        participants |= 1u << (i.participant - 1);
    }
    return participants == 255;
}

class Writer {
public:
    std::vector<uint8_t> bytes;
    void U8(uint8_t n) { bytes.push_back(n); }
    void U16(uint16_t n) { U8(uint8_t(n)); U8(uint8_t(n >> 8)); }
    void U32(uint32_t n) { U16(uint16_t(n)); U16(uint16_t(n >> 16)); }
    void F32(float n) { uint32_t bits; static_assert(sizeof(bits) == sizeof(n)); std::memcpy(&bits, &n, 4); U32(bits); }
};
class Reader {
    const uint8_t* bytes;
    size_t length, offset = 0;
public:
    bool ok = true;
    Reader(const uint8_t* b, size_t n) : bytes(b), length(n) { if (!b && n) ok = false; }
    uint8_t U8() { if (!ok || offset >= length) { ok = false; return 0; } return bytes[offset++]; }
    uint16_t U16() { const auto a = U8(); const auto b = U8(); return uint16_t(a | uint16_t(b) << 8); }
    int16_t I16() { const uint16_t n = U16(); return int16_t(n <= 32767 ? int32_t(n) : int32_t(n) - 65536); }
    uint32_t U32() { const auto a = U16(); const auto b = U16(); return uint32_t(a) | uint32_t(b) << 16; }
    int32_t I32() { const uint32_t n = U32(); return n <= 0x7fffffffu ? int32_t(n) : int32_t(int64_t(n) - 0x100000000ll); }
    float F32() { const auto bits = U32(); float n; std::memcpy(&n, &bits, 4); return n; }
    bool Done() const { return ok && offset == length; }
};

inline std::vector<uint8_t> Encode(const Frame& f) {
    if (!Valid(f)) return {};
    Writer w;
    w.U32(0x31465749); w.U32(f.generation); w.U16(f.slots); w.U16(f.active); w.U16(f.permanent);
    w.U16(uint16_t(f.actors.size()));
    for (const auto& a : f.actors) {
        w.U16(a.index); w.U8(a.type); w.U16(a.flags); w.U16(uint16_t(a.state));
        w.U16(uint16_t(a.timer)); w.U16(a.owner); w.U16(a.target); w.U16(uint16_t(a.parent)); w.U16(uint16_t(a.param));
        for (auto n : a.rotation) w.U16(uint16_t(n));
        for (auto n : a.position) w.F32(n);
        for (auto n : a.velocity) w.F32(n);
        w.F32(a.auxiliary); w.F32(a.radius);
        for (auto n : a.links) w.U16(uint16_t(n));
    }
    for (const auto& i : f.inventories) {
        w.U8(i.participant); w.U8(i.present); w.U8(i.item); w.U8(i.icon); w.U8(i.nextItem); w.U8(i.mode);
        w.U16(uint16_t(i.state)); w.U16(uint16_t(i.copiedItem)); w.U16(i.goldenTimer);
        w.U32(uint32_t(i.timer)); w.U32(i.status); w.U8(uint8_t(i.balloons));
        for (auto n : i.balloonStatus) w.U16(n);
    }
    return w.bytes.size() <= MaxFrame ? std::move(w.bytes) : std::vector<uint8_t>{};
}
inline bool Decode(const uint8_t* bytes, size_t length, Frame& result) {
    if (length < 16 || length > MaxFrame) return false;
    Reader r(bytes, length);
    if (r.U32() != 0x31465749) return false;
    Frame f; f.generation = r.U32(); f.slots = r.U16(); f.active = r.U16(); f.permanent = r.U16();
    const auto count = r.U16();
    if (count > MaxActors || f.slots > MaxSlots) return false;
    f.actors.resize(count);
    for (auto& a : f.actors) {
        a.index = r.U16(); a.type = r.U8(); a.flags = r.U16(); a.state = r.I16();
        a.timer = r.I16(); a.owner = r.U16(); a.target = r.U16(); a.parent = r.I16(); a.param = r.I16();
        for (auto& n : a.rotation) n = r.I16();
        for (auto& n : a.position) n = r.F32();
        for (auto& n : a.velocity) n = r.F32();
        a.auxiliary = r.F32(); a.radius = r.F32();
        for (auto& n : a.links) n = r.I16();
    }
    for (auto& i : f.inventories) {
        i.participant = r.U8(); i.present = r.U8(); i.item = r.U8(); i.icon = r.U8();
        i.nextItem = r.U8(); i.mode = r.U8(); i.state = r.I16(); i.copiedItem = r.I16();
        i.goldenTimer = r.U16(); i.timer = r.I32(); i.status = r.U32();
        const auto balloons = r.U8(); i.balloons = balloons == 255 ? -1 : int8_t(balloons <= 127 ? balloons : 127);
        for (auto& n : i.balloonStatus) n = r.U16();
    }
    if (!r.Done() || !Valid(f)) return false;
    result = std::move(f); return true;
}
inline uint32_t Digest(const std::vector<uint8_t>& data) {
    uint32_t hash = 2166136261u;
    for (auto n : data) { hash ^= n; hash *= 16777619u; }
    return hash;
}
inline std::vector<std::vector<uint8_t>> Chunk(const Frame& f) {
    auto bytes = Encode(f);
    if (bytes.empty()) return {};
    const auto count = (bytes.size() + ChunkData - 1) / ChunkData;
    if (count > MaxParts) return {};
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t part = 0; part < count; ++part) {
        Writer w; w.U32(0x31435749); w.U32(f.generation); w.U16(uint16_t(bytes.size()));
        w.U8(uint8_t(part)); w.U8(uint8_t(count)); w.U32(Digest(bytes));
        const auto begin = bytes.begin() + part * ChunkData;
        const auto end = bytes.begin() + std::min(bytes.size(), (part + 1) * ChunkData);
        w.bytes.insert(w.bytes.end(), begin, end); chunks.push_back(std::move(w.bytes));
    }
    return chunks;
}

class Reassembler {
    uint32_t pendingGeneration = 0, completedGeneration = 0, digest = 0, seen = 0;
    bool pending = false, completed = false;
    size_t parts = 0;
    std::vector<uint8_t> bytes;
public:
    void Reset() { *this = Reassembler{}; }
    // The caller must authenticate the source/epoch before invoking this codec.
    bool Push(const std::vector<uint8_t>& packet, bool authenticatedHost, Frame& result) {
        if (!authenticatedHost || packet.size() <= ChunkHeader || packet.size() > MaxOpaque) return false;
        Reader r(packet.data(), ChunkHeader);
        if (r.U32() != 0x31435749) return false;
        const auto generation = r.U32(); const auto size = r.U16();
        const auto part = r.U8(); const auto count = r.U8(); const auto hash = r.U32();
        if (!r.Done() || size < 16 || size > MaxFrame || !count || count > MaxParts || part >= count ||
            count != (size + ChunkData - 1) / ChunkData ||
            packet.size() != ChunkHeader + std::min(ChunkData, size_t(size) - part * ChunkData)) return false;
        if (completed && !Newer(generation, completedGeneration)) return false;
        if (pending && generation != pendingGeneration && !Newer(generation, pendingGeneration)) return false;
        if (!pending || generation != pendingGeneration) {
            pending = true; pendingGeneration = generation; digest = hash; parts = count; seen = 0;
            bytes.assign(size, 0);
        }
        if (bytes.size() != size || parts != count || digest != hash) return false;
        const auto first = packet.begin() + ChunkHeader;
        auto dest = bytes.begin() + part * ChunkData;
        if (seen & (1u << part)) {
            if (!std::equal(first, packet.end(), dest)) { pending = false; bytes.clear(); seen = 0; }
            return false;
        }
        std::copy(first, packet.end(), dest); seen |= 1u << part;
        const uint32_t mask = count == 32 ? 0xffffffffu : (1u << count) - 1u;
        if (seen != mask) return false;
        Frame frame;
        if (Digest(bytes) != digest || !Decode(bytes.data(), bytes.size(), frame) || frame.generation != generation) {
            pending = false; bytes.clear(); return false;
        }
        completed = true; completedGeneration = generation; pending = false; bytes.clear();
        result = std::move(frame); return true;
    }
};
}
