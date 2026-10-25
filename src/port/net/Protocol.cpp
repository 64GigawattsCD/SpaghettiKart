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
    void text(const std::string& s, size_t max) {
        if (s.size() > max) throw std::length_error("Network text is too long");
        u(s.size(), 1); bytes.insert(bytes.end(), s.begin(), s.end());
    }
};
struct Reader {
    const uint8_t* data; size_t size, offset = 0;
    uint64_t u(size_t width) {
        if (width > size - offset) throw std::runtime_error("Truncated packet");
        uint64_t n = 0; while (width--) n = (n << 8) | data[offset++]; return n;
    }
    bool flag() { auto n = u(1); if (n > 1) throw std::runtime_error("Invalid flag"); return n != 0; }
    float f() {
        uint32_t bits = uint32_t(u(4)); float n; std::memcpy(&n, &bits, 4);
        if (!std::isfinite(n) || std::abs(n) > 10000000.0f) throw std::runtime_error("Invalid float");
        return n;
    }
    std::string text(size_t max) {
        size_t n = u(1);
        if (n > max || n > size - offset) throw std::runtime_error("Invalid text");
        std::string s(reinterpret_cast<const char*>(data + offset), n); offset += n;
        for (unsigned char c : s) if (c < 32 || c > 126) throw std::runtime_error("Invalid text character");
        return s;
    }
};
void writeSettings(Writer& w, const RaceSettings& s) {
    if (!s.Valid()) throw std::runtime_error("Invalid race settings");
    for (auto v : {s.mode, s.scope, s.maxPlayers, s.cup, s.course, s.cc, s.laps, s.round, s.items, s.balloons}) w.u(v, 1);
    w.u(s.cpuFill, 1); w.u(s.mirror, 1); w.text(s.name, 47);
}
RaceSettings readSettings(Reader& r) {
    RaceSettings s;
    s.mode = r.u(1); s.scope = r.u(1); s.maxPlayers = r.u(1); s.cup = r.u(1); s.course = r.u(1);
    s.cc = r.u(1); s.laps = r.u(1); s.round = r.u(1); s.items = r.u(1); s.balloons = r.u(1);
    s.cpuFill = r.flag(); s.mirror = r.flag(); s.name = r.text(47);
    if (!s.Valid()) throw std::runtime_error("Invalid race settings");
    return s;
}
void writeParty(Writer& w, Party p, uint8_t fallback = 0) {
    if (p.empty()) p.push_back({0, fallback, 0, false});
    if (!ValidParty(p)) throw std::runtime_error("Invalid local party");
    w.u(p.size(), 1);
    for (auto v : p) { w.u(v.controller, 1); w.u(v.character, 1); w.u(v.transmission, 1); w.u(v.ready, 1); }
}
Party readParty(Reader& r) {
    size_t count = r.u(1); if (!count || count > MaxLocalPlayers) throw std::runtime_error("Invalid party count");
    Party p;
    for (size_t i = 0; i < count; ++i) p.push_back({uint8_t(r.u(1)), uint8_t(r.u(1)), uint8_t(r.u(1)), r.flag()});
    if (!ValidParty(p)) throw std::runtime_error("Invalid local party");
    return p;
}
void writeKart(Writer& w, const Kart& k) {
    w.u(k.participant, 2); w.u(k.type, 2); w.u(k.effects, 4);
    for (auto f : k.pos) w.f(f); for (auto f : k.velocity) w.f(f); for (auto f : k.oldPos) w.f(f);
    for (auto n : k.rotation) w.u(uint16_t(n), 2);
    for (auto f : k.orientation) w.f(f); for (auto f : k.ground) w.f(f); for (auto f : k.tyres) w.f(f);
    w.f(k.speed); w.f(k.rpm); w.f(k.size); w.u(uint32_t(k.steering), 4);
    w.u(uint16_t(k.driftYaw), 2); w.u(uint16_t(k.slope), 2);
    w.u(uint8_t(k.gear), 1); w.u(k.lap, 1); w.u(k.rank, 1);
    for (auto t : k.lapTimes) w.u(t, 4);
}
Kart readKart(Reader& r) {
    Kart k; k.participant = r.u(2); k.type = r.u(2); k.effects = uint32_t(r.u(4));
    for (auto& f : k.pos) f = r.f(); for (auto& f : k.velocity) f = r.f(); for (auto& f : k.oldPos) f = r.f();
    for (auto& n : k.rotation) n = int16_t(r.u(2));
    for (auto& f : k.orientation) f = r.f(); for (auto& f : k.ground) f = r.f(); for (auto& f : k.tyres) f = r.f();
    k.speed = r.f(); k.rpm = r.f(); k.size = r.f(); k.steering = int32_t(r.u(4));
    k.driftYaw = int16_t(r.u(2)); k.slope = int16_t(r.u(2));
    k.gear = int8_t(r.u(1)); k.lap = r.u(1); k.rank = r.u(1);
    for (auto& t : k.lapTimes) t = uint32_t(r.u(4));
    if (k.participant < 1 || k.participant > MaxRacers || k.lap > 9 || k.rank >= MaxRacers || k.gear < -1 || k.gear > 8 ||
        k.size <= 0 || k.size > 8)
        throw std::runtime_error("Invalid kart");
    return k;
}
}
std::vector<uint8_t> Encode(const Message& m) {
    Writer w; w.u(0x534b4e31, 4); w.u(ProtocolVersion, 2); w.u(uint8_t(m.kind), 1); w.u(0, 1); w.u(m.session, 8); w.u(m.race, 4);
    switch (m.kind) {
    case Kind::Hello:
        w.u(m.rules, 4); w.u(m.content, 8); writeParty(w, m.party, m.character);
        w.bytes.insert(w.bytes.end(), m.ticket.begin(), m.ticket.end()); break;
    case Kind::Welcome: case Kind::Lobby:
        w.u(m.rules, 4); w.u(m.content, 8); w.u(m.connection, 2); w.u(m.revision, 4); writeSettings(w, m.settings);
        w.u(uint8_t(m.phase), 1); w.bytes.insert(w.bytes.end(), m.ticket.begin(), m.ticket.end());
        if (!ValidRoster(m.roster)) throw std::runtime_error("Invalid roster");
        w.u(m.roster.size(), 1);
        for (auto p : m.roster) {
            w.u(p.participant, 2); w.u(p.connection, 2); w.u(p.racer, 1); w.u(p.controller, 1);
            w.u(p.character, 1); w.u(p.transmission, 1); w.u(p.ready, 1); w.u(p.connected, 1);
        }
        for (auto points : m.standings) w.u(points, 2);
        w.u(m.cupComplete, 1); w.u(m.ready, 1); break;
    case Kind::Load: writeSettings(w, m.settings); w.u(m.revision, 4); break;
    case Kind::Loaded: case Kind::Begin: w.u(m.round, 1); break;
    case Kind::Ready: w.u(m.ready, 1); w.u(m.controller, 1); break;
    case Kind::UpdateParty: writeParty(w, m.party); break;
    case Kind::Reject: w.text(m.reason, 96); break;
    case Kind::World:
        if (m.payload.empty() || m.payload.size() > 1100) throw std::length_error("World payload exceeds packet budget");
        w.u(m.payload.size(), 2); w.bytes.insert(w.bytes.end(), m.payload.begin(), m.payload.end()); break;
    case Kind::Input:
        w.u(m.round, 1); w.u(m.input.sequence, 4); w.u(m.input.participant, 2); w.u(m.input.buttons, 4);
        w.u(uint8_t(m.input.x), 1); w.u(uint8_t(m.input.y), 1); w.u(m.input.throttle, 1); w.u(m.input.brake, 1); break;
    case Kind::Snapshot: case Kind::Finish:
        if (m.karts.empty() || m.karts.size() > KartsPerPacket) throw std::length_error("Snapshot chunk exceeds packet budget");
        w.u(m.tick, 4); w.u(m.acknowledgedInput, 4); w.f(m.time); w.u(m.round, 1);
        w.u(m.part, 1); w.u(m.parts, 1); w.u(m.karts.size(), 1);
        for (auto p : m.standings) w.u(p, 2);
        w.u(m.cupComplete, 1);
        for (const auto& k : m.karts) writeKart(w, k); break;
    }
    if (w.bytes.size() > MaxPacket) throw std::length_error("Packet exceeds budget");
    return w.bytes;
}
bool Decode(const uint8_t* data, size_t length, Message& result) {
    if (!data || length < 20 || length > MaxPacket) return false;
    try {
        Reader r{data, length}; Message m;
        if (r.u(4) != 0x534b4e31 || r.u(2) != ProtocolVersion) return false;
        auto kind = r.u(1); if (kind < 1 || kind > uint8_t(Kind::World) || r.u(1) != 0) return false;
        m.kind = Kind(kind); m.session = r.u(8); m.race = uint32_t(r.u(4));
        switch (m.kind) {
        case Kind::Hello:
            m.rules = uint32_t(r.u(4)); m.content = r.u(8); m.party = readParty(r); m.character = m.party.front().character;
            for (auto& b : m.ticket) b = r.u(1);
            if (m.session || m.race) return false; break;
        case Kind::Welcome: case Kind::Lobby: {
            m.rules = uint32_t(r.u(4)); m.content = r.u(8); m.connection = r.u(2); m.revision = uint32_t(r.u(4));
            m.settings = readSettings(r); auto phase = r.u(1); if (phase > uint8_t(Phase::Error)) return false;
            m.phase = Phase(phase); for (auto& b : m.ticket) b = r.u(1);
            size_t count = r.u(1); if (!count || count > MaxRacers) return false;
            for (size_t i = 0; i < count; ++i) {
                Identity p; p.participant = r.u(2); p.connection = r.u(2); p.racer = r.u(1); p.controller = r.u(1);
                p.character = r.u(1); p.transmission = r.u(1); p.ready = r.flag(); p.connected = r.flag(); m.roster.push_back(p);
            }
            for (auto& p : m.standings) p = r.u(2);
            m.cupComplete = r.flag(); m.ready = r.flag();
            if (!m.session || m.connection < 1 || m.connection > 8 || !ValidRoster(m.roster) ||
                m.roster.size() > m.settings.maxPlayers) return false;
            break;
        }
        case Kind::Load: m.settings = readSettings(r); m.revision = uint32_t(r.u(4)); break;
        case Kind::Loaded: case Kind::Begin: m.round = r.u(1); if (m.round >= 4) return false; break;
        case Kind::Ready:
            m.ready = r.flag(); m.controller = r.u(1);
            if (m.controller != 255 && m.controller >= MaxLocalPlayers) return false;
            break;
        case Kind::UpdateParty: m.party = readParty(r); break;
        case Kind::Reject: m.reason = r.text(96); break;
        case Kind::World: {
            size_t size = r.u(2); if (!size || size > 1100 || size > r.size - r.offset) return false;
            m.payload.assign(r.data + r.offset, r.data + r.offset + size); r.offset += size; break;
        }
        case Kind::Input:
            m.round = r.u(1); m.input.sequence = uint32_t(r.u(4)); m.input.participant = r.u(2);
            m.input.buttons = uint32_t(r.u(4)); m.input.x = int8_t(r.u(1)); m.input.y = int8_t(r.u(1));
            m.input.throttle = r.u(1); m.input.brake = r.u(1);
            if (m.round >= 4 || m.input.participant < 1 || m.input.participant > MaxRacers ||
                std::abs(int(m.input.x)) > 80 || std::abs(int(m.input.y)) > 80 || (m.input.buttons & ~DrivingButtonMask)) return false;
            break;
        case Kind::Snapshot: case Kind::Finish: {
            m.tick = uint32_t(r.u(4)); m.acknowledgedInput = uint32_t(r.u(4)); m.time = r.f(); m.round = r.u(1);
            m.part = r.u(1); m.parts = r.u(1); size_t count = r.u(1);
            if (m.round >= 4 || m.parts < 1 || m.parts > 2 || m.part >= m.parts || !count || count > KartsPerPacket ||
                m.time < 0) return false;
            for (auto& p : m.standings) p = r.u(2);
            m.cupComplete = r.flag(); m.karts.clear(); unsigned participants = 0;
            for (size_t i = 0; i < count; ++i) {
                auto k = readKart(r); if (participants & (1u << k.participant)) return false;
                participants |= 1u << k.participant; m.karts.push_back(k);
            }
            break;
        }
        }
        if (r.offset != length) return false;
        result = std::move(m); return true;
    } catch (const std::runtime_error&) { return false; }
}
}
