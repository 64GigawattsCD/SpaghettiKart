#include "Protocol.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace KartNet {
int LocalSlot(uint16_t participant, uint16_t localParticipant) {
    if (participant < 1 || participant > 2 || localParticipant < 1 || localParticipant > 2) return -1;
    return participant == localParticipant ? 0 : 1;
}
bool Newer(uint32_t next, uint32_t previous) { return next != previous && uint32_t(next - previous) < 0x80000000u; }
namespace {
struct Writer {
    std::vector<uint8_t> bytes;
    void u(uint64_t n, size_t width) { for (size_t i = width; i; --i) bytes.push_back(uint8_t(n >> ((i - 1) * 8))); }
    void f(float n) { uint32_t bits; std::memcpy(&bits, &n, 4); u(bits, 4); }
};
struct Reader {
    const uint8_t* data; size_t size, offset = 0;
    uint64_t u(size_t width) {
        if (offset + width > size) throw std::runtime_error("Truncated packet");
        uint64_t n = 0; while (width--) n = (n << 8) | data[offset++]; return n;
    }
    float f() {
        uint32_t bits = uint32_t(u(4)); float n; std::memcpy(&n, &bits, 4);
        if (!std::isfinite(n) || std::abs(n) > 10000000.0f) throw std::runtime_error("Invalid float");
        return n;
    }
};
void writeKart(Writer& w, const Kart& k) {
    w.u(k.participant, 2); w.u(k.type, 2); w.u(k.effects, 4);
    for (auto f : k.pos) w.f(f); for (auto f : k.velocity) w.f(f);
    for (auto f : k.oldPos) w.f(f);
    for (auto n : k.rotation) w.u(uint16_t(n), 2);
    for (auto f : k.orientation) w.f(f); for (auto f : k.ground) w.f(f);
    for (auto f : k.tyres) w.f(f);
    w.f(k.speed); w.f(k.rpm); w.u(uint32_t(k.steering), 4);
    w.u(uint16_t(k.driftYaw), 2); w.u(uint16_t(k.slope), 2);
    w.u(uint8_t(k.gear), 1); w.u(k.lap, 1); w.u(k.rank, 1);
    for (auto t : k.lapTimes) w.u(t, 4);
}
Kart readKart(Reader& r) {
    Kart k; k.participant = uint16_t(r.u(2)); k.type = uint16_t(r.u(2)); k.effects = uint32_t(r.u(4));
    for (auto& f : k.pos) f = r.f(); for (auto& f : k.velocity) f = r.f();
    for (auto& f : k.oldPos) f = r.f();
    for (auto& n : k.rotation) n = int16_t(r.u(2));
    for (auto& f : k.orientation) f = r.f(); for (auto& f : k.ground) f = r.f();
    for (auto& f : k.tyres) f = r.f();
    k.speed = r.f(); k.rpm = r.f(); k.steering = int32_t(r.u(4));
    k.driftYaw = int16_t(r.u(2)); k.slope = int16_t(r.u(2));
    k.gear = int8_t(r.u(1)); k.lap = uint8_t(r.u(1)); k.rank = uint8_t(r.u(1));
    for (auto& t : k.lapTimes) t = uint32_t(r.u(4));
    if (k.participant < 1 || k.participant > 2 || k.lap > 3 || k.rank > 1 || k.gear < -1 || k.gear > 8)
        throw std::runtime_error("Invalid kart");
    return k;
}
}
std::vector<uint8_t> Encode(const Message& m) {
    Writer w; w.u(0x534b4e31, 4); w.u(ProtocolVersion, 2); w.u(uint8_t(m.kind), 1); w.u(0, 1);
    w.u(m.session, 8);
    switch (m.kind) {
    case Kind::Hello: w.u(m.rules, 4); w.u(m.content, 8); w.u(m.character, 1); break;
    case Kind::Welcome:
        w.u(m.rules, 4); w.u(m.content, 8);
        for (const auto& p : m.roster) {
            w.u(p.participant, 2); w.u(p.connection, 2); w.u(p.racer, 1); w.u(p.controller, 1); w.u(p.character, 1);
        }
        break;
    case Kind::Input:
        w.u(m.input.sequence, 4); w.u(m.input.participant, 2); w.u(m.input.buttons, 4);
        w.u(uint8_t(m.input.x), 1); w.u(uint8_t(m.input.y), 1); w.u(m.input.throttle, 1); w.u(m.input.brake, 1);
        break;
    case Kind::Snapshot: case Kind::Finish:
        w.u(m.tick, 4); w.u(m.acknowledgedInput, 4); w.f(m.time);
        for (const auto& k : m.karts) writeKart(w, k);
        break;
    default: break;
    }
    return w.bytes;
}
bool Decode(const uint8_t* data, size_t length, Message& result) {
    if (!data || length < 16 || length > MaxPacket) return false;
    try {
        Reader r{data, length}; Message m;
        if (r.u(4) != 0x534b4e31 || r.u(2) != ProtocolVersion) return false;
        auto kind = r.u(1); if (kind < 1 || kind > uint8_t(Kind::Finish) || r.u(1) != 0) return false;
        m.kind = Kind(kind); m.session = r.u(8);
        switch (m.kind) {
        case Kind::Hello:
            m.rules = uint32_t(r.u(4)); m.content = r.u(8); m.character = uint8_t(r.u(1));
            if (m.character > 7 || m.session != 0) return false;
            break;
        case Kind::Welcome:
            m.rules = uint32_t(r.u(4)); m.content = r.u(8);
            for (size_t i = 0; i < m.roster.size(); ++i) {
                auto& p = m.roster[i];
                p.participant = uint16_t(r.u(2)); p.connection = uint16_t(r.u(2));
                p.racer = uint8_t(r.u(1)); p.controller = uint8_t(r.u(1)); p.character = uint8_t(r.u(1));
                if (p.participant != i+1 || p.connection != i+1 || p.racer != i || p.controller != 0 || p.character > 7)
                    return false;
            }
            if (!m.session || m.roster[0].character == m.roster[1].character) return false;
            break;
        case Kind::Input:
            m.input.sequence = uint32_t(r.u(4)); m.input.participant = uint16_t(r.u(2));
            m.input.buttons = uint32_t(r.u(4)); m.input.x = int8_t(r.u(1)); m.input.y = int8_t(r.u(1));
            m.input.throttle = uint8_t(r.u(1)); m.input.brake = uint8_t(r.u(1));
            if (m.input.participant < 1 || m.input.participant > 2 || std::abs(int(m.input.x)) > 80 ||
                std::abs(int(m.input.y)) > 80 || (m.input.buttons & ~uint32_t(0xc0d0))) return false;
            break;
        case Kind::Snapshot: case Kind::Finish:
            m.tick = uint32_t(r.u(4)); m.acknowledgedInput = uint32_t(r.u(4)); m.time = r.f();
            for (auto& k : m.karts) k = readKart(r);
            if (m.karts[0].participant != 1 || m.karts[1].participant != 2 || m.time < 0) return false;
            break;
        default: break;
        }
        if (r.offset != length) return false;
        result = m; return true;
    } catch (const std::runtime_error&) { return false; }
}
}
