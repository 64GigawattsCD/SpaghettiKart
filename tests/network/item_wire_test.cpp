#include "../../src/port/net/ItemWire.h"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace KartNet::ItemWire;
namespace {
unsigned checks = 0;
void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
Frame Sample(size_t count = 40, uint32_t generation = 1) {
    Frame f; f.generation = generation; f.slots = uint16_t(count + 50); f.active = uint16_t(count); f.permanent = 12;
    for (size_t n = 0; n < Racers; ++n) {
        auto& i = f.inventories[n]; i.participant = uint8_t(n + 1); i.present = 1;
        i.item = uint8_t(n + 1); i.icon = uint8_t(15 - n); i.copiedItem = i.item;
        i.nextItem = i.item; i.state = 8; i.timer = -1; i.goldenTimer = 600;
        i.balloons = int8_t(int(n % 4) - 1); i.balloonStatus = {0, 1, 2};
    }
    for (size_t n = 0; n < count; ++n) {
        Actor a; a.index = uint16_t(n); a.flags = 0xc000; a.state = 2;
        a.rotation = {int16_t(n), -32768, 32767}; a.position = {float(n), -25.5f, 8000};
        a.velocity = {1, -2, 3}; a.auxiliary = -12; a.radius = 5; f.actors.push_back(a);
    }
    return f;
}
template<class F> void Invalid(F edit, const char* label) {
    auto f = Sample(); edit(f); Check(Encode(f).empty(), label);
}
std::vector<std::vector<uint8_t>> RawChunks(const std::vector<uint8_t>& bytes, uint32_t gen) {
    std::vector<std::vector<uint8_t>> result;
    const auto parts = (bytes.size() + ChunkData - 1) / ChunkData;
    for (size_t part = 0; part < parts; ++part) {
        Writer w; w.U32(0x31435749); w.U32(gen); w.U16(uint16_t(bytes.size()));
        w.U8(uint8_t(part)); w.U8(uint8_t(parts)); w.U32(Digest(bytes));
        w.bytes.insert(w.bytes.end(), bytes.begin() + part * ChunkData,
            bytes.begin() + std::min(bytes.size(), (part + 1) * ChunkData));
        result.push_back(std::move(w.bytes));
    }
    return result;
}
void Codec() {
    auto f = Sample(); auto bytes = Encode(f); Frame result;
    Check(!bytes.empty() && Decode(bytes.data(), bytes.size(), result), "Complete frame round trip");
    Check(Encode(result) == bytes, "All explicit fields retained");
    for (size_t n = 0; n < bytes.size(); ++n) {
        result.generation = 876;
        Check(!Decode(bytes.data(), n, result) && result.generation == 876, "Truncated frames have no partial output");
    }
    auto trailing = bytes; trailing.push_back(0);
    Check(!Decode(trailing.data(), trailing.size(), result), "Reject trailing bytes");
    Check(!Decode(nullptr, bytes.size(), result), "Reject null input");
    for (size_t offset : {size_t(0), size_t(8), size_t(10), size_t(12), size_t(14), size_t(16), size_t(18)}) {
        auto bad = bytes; bad[offset] = 255; bad[offset + 1] = 255;
        Check(!Decode(bad.data(), bad.size(), result), "Reject malformed magic/count/index/type");
    }
    Invalid([](Frame& f) { f.slots = MaxSlots + 1; }, "Slot cap");
    Invalid([](Frame& f) { f.active = f.slots + 1; }, "Active count cap");
    Invalid([](Frame& f) { f.active = 0; }, "Active records must fit count");
    Invalid([](Frame& f) { f.permanent = f.slots + 1; }, "Permanent count cap");
    Invalid([](Frame& f) { f.actors[0].index = f.slots; }, "Index bounds");
    Invalid([](Frame& f) { f.actors[1].index = f.actors[0].index; }, "Duplicate actor");
    Invalid([](Frame& f) { f.actors[0].flags = 0; }, "Inactive record");
    Invalid([](Frame& f) { f.actors[0].type = 39; }, "Pointer-bearing crossing excluded");
    Invalid([](Frame& f) { f.actors[0].type = 255; }, "Unknown actor type");
    Invalid([](Frame& f) { f.actors[0].owner = 9; }, "Owner bounds");
    Invalid([](Frame& f) { f.actors[0].target = 9; }, "Target bounds");
    Invalid([](Frame& f) { f.actors[0].parent = -2; }, "Parent negative bound");
    Invalid([](Frame& f) { f.actors[0].links[4] = f.slots; }, "Linked actor bound");
    Invalid([](Frame& f) { f.actors[0].position[0] = std::numeric_limits<float>::quiet_NaN(); }, "NaN position");
    Invalid([](Frame& f) { f.actors[0].velocity[1] = std::numeric_limits<float>::infinity(); }, "Infinite velocity");
    Invalid([](Frame& f) { f.actors[0].radius = -1; }, "Negative radius");
    Invalid([](Frame& f) { f.actors[0].auxiliary = 1000001; }, "Auxiliary magnitude cap");
    Invalid([](Frame& f) { auto& a = f.actors[0]; a.type = Banana; a.state = 2; a.owner = 1; }, "Bunched banana parent required");
    Invalid([](Frame& f) { auto& a = f.actors[0]; a.type = Banana; a.state = 2; a.owner = 1; a.parent = 0; a.param = 5; }, "Banana id cap");
    Invalid([](Frame& f) { auto& a = f.actors[0]; a.type = Kiwano; a.owner = 1; }, "Kiwano animation bounds");
    Invalid([](Frame& f) { f.inventories[0].participant = 0; }, "Participant zero");
    Invalid([](Frame& f) { f.inventories[0].participant = 9; }, "Participant cap");
    Invalid([](Frame& f) { f.inventories[0].participant = 2; }, "Duplicate inventory participant");
    Invalid([](Frame& f) { f.inventories[0].item = 16; }, "Inventory item cap");
    Invalid([](Frame& f) { f.inventories[0].icon = 255; }, "Inventory icon cap");
    Invalid([](Frame& f) { f.inventories[0].copiedItem = -1; }, "Copied item bound");
    Invalid([](Frame& f) { f.inventories[0].state = 51; }, "Inventory state bound");
    Invalid([](Frame& f) { f.inventories[0].timer = -2; }, "Inventory timer bound");
    Invalid([](Frame& f) { f.inventories[0].mode = 4; }, "Inventory mode bound");
    Invalid([](Frame& f) { f.inventories[0].balloons = 3; }, "Three balloon slots only");
    Invalid([](Frame& f) { f.inventories[0].balloonStatus[1] = 4; }, "Balloon bits");
    for (uint8_t type : {Banana, GreenShell, RedShell, ItemBox, FakeBox, BananaBunch, TripleGreen, TripleRed, BlueShell, BalloonBox}) {
        for (int state = 0; state <= 10; ++state) {
            auto sample = Sample(1); auto& a = sample.actors[0];
            a.type = type; a.state = int16_t(state); a.owner = 8; a.target = 5; a.parent = 0; a.auxiliary = 0;
            const bool expected = Shell(type) ? state <= 9 : type == Banana ? state <= 5 :
                type == FakeBox ? state <= 2 : Triple(type) ? state <= 4 : state <= 6;
            Check(!Encode(sample).empty() == expected, "Per-type stock state ranges");
        }
    }
    for (uint8_t type : {GreenShell, RedShell, BlueShell, FakeBox, TripleGreen, TripleRed, BananaBunch}) {
        auto sample = Sample(1); auto& a = sample.actors[0]; a.type = type; a.state = 0; a.auxiliary = 0;
        Check(Encode(sample).empty(), "Item owner required");
    }
    auto maximum = Sample(MaxActors); Check(!Chunk(maximum).empty(), "Maximum active actors fit queue");
    maximum.actors.push_back(maximum.actors[0]); Check(Encode(maximum).empty(), "Active actor cap");
}
void Chunks() {
    auto f = Sample(MaxActors); auto chunks = Chunk(f); Frame out; Reassembler receiver;
    Check(chunks.size() > 1 && chunks.size() <= MaxParts, "Bounded multipart frame");
    for (const auto& c : chunks) Check(c.size() <= MaxOpaque, "Transport payload bound");
    for (const auto& c : chunks) Check(!receiver.Push(c, false, out), "Client-origin world rejected");
    for (size_t n = 0; n < chunks.size(); ++n)
        Check(receiver.Push(chunks[n], true, out) == (n + 1 == chunks.size()), "Commit only full frame");
    Check(Encode(out) == Encode(f), "Chunk reassembly exact");
    for (const auto& c : chunks) Check(!receiver.Push(c, true, out), "Completed frame replay rejected");
    receiver.Reset();
    for (size_t n = chunks.size(); n-- > 0;)
        Check(receiver.Push(chunks[n], true, out) == (n == 0), "Reorder reassembly");
    for (size_t missing = 0; missing < chunks.size(); ++missing) {
        receiver.Reset(); out.generation = 234;
        for (size_t n = 0; n < chunks.size(); ++n) if (n != missing)
            Check(!receiver.Push(chunks[n], true, out) && out.generation == 234, "Loss retains previous committed frame");
        auto next = Sample(MaxActors, 2); auto nextChunks = Chunk(next);
        for (size_t n = 0; n < nextChunks.size(); ++n)
            Check(receiver.Push(nextChunks[n], true, out) == (n + 1 == nextChunks.size()), "Recover on next complete frame");
        Check(out.generation == 2, "New generation replaces incomplete frame");
        for (const auto& c : chunks) Check(!receiver.Push(c, true, out), "Old incomplete generation rejected");
    }
    receiver.Reset();
    Check(!receiver.Push(chunks[0], true, out), "First chunk pending");
    for (size_t n = 1; n < chunks.size(); ++n) Check(!receiver.Push(chunks[n], false, out), "Unauthorized chunks do not advance pending frame");
    Check(!receiver.Push(chunks[0], true, out), "Identical duplicate ignored");
    for (size_t n = 1; n < chunks.size(); ++n)
        Check(receiver.Push(chunks[n], true, out) == (n + 1 == chunks.size()), "Duplicates do not count twice");
    for (size_t field : {size_t(0), size_t(8), size_t(9), size_t(10), size_t(11), size_t(12), size_t(16)}) {
        receiver.Reset(); auto bad = chunks; bad[0][field] ^= 0xff;
        out.generation = 432;
        for (const auto& c : bad) Check(!receiver.Push(c, true, out), "Malformed chunk never commits");
        Check(out.generation == 432, "Malformed frame output untouched");
    }
    for (size_t length = 0; length < chunks[0].size(); ++length) {
        receiver.Reset(); auto c = chunks[0]; c.resize(length);
        Check(!receiver.Push(c, true, out), "Chunk truncations rejected");
    }
    receiver.Reset(); auto oversized = chunks[0]; oversized.resize(MaxOpaque + 1);
    Check(!receiver.Push(oversized, true, out), "Oversized chunk rejected");
    auto bytes = Encode(f); bytes[18] = 255; // Correct checksum does not excuse an invalid actor type.
    auto invalid = RawChunks(bytes, f.generation); receiver.Reset();
    for (const auto& c : invalid) Check(!receiver.Push(c, true, out), "Semantic frame validation after assembly");
    receiver.Reset();
    auto beforeWrap = Chunk(Sample(40, 0xffffffffu)); auto afterWrap = Chunk(Sample(40, 0));
    for (const auto& c : beforeWrap) receiver.Push(c, true, out);
    Check(out.generation == 0xffffffffu, "Initial large generation");
    for (const auto& c : afterWrap) receiver.Push(c, true, out);
    Check(out.generation == 0, "Sequence wrap accepted");
    for (const auto& c : beforeWrap) Check(!receiver.Push(c, true, out), "Pre-wrap replay rejected");
    receiver.Reset(); out.generation = 123;
    Check(!receiver.Push(chunks[0], true, out), "Conflict test first chunk");
    auto conflict = chunks[0]; conflict.back() ^= 1;
    Check(!receiver.Push(conflict, true, out), "Conflicting duplicate rejected");
    for (size_t n = 1; n < chunks.size(); ++n) Check(!receiver.Push(chunks[n], true, out), "Conflict never commits partial frame");
    Check(out.generation == 123, "Conflict leaves output untouched");
}
void Fuzz() {
    std::mt19937 rng(0x17e8);
    Frame out; Reassembler receiver;
    for (unsigned n = 0; n < 12000; ++n) {
        std::vector<uint8_t> bytes(rng() % (MaxOpaque + 40));
        for (auto& b : bytes) b = uint8_t(rng());
        Check(!Decode(bytes.data(), bytes.size(), out), "Random malformed frame rejected");
        Check(!receiver.Push(bytes, true, out), "Random malformed chunk rejected");
    }
}
}
int main() {
    try { Codec(); Chunks(); Fuzz(); std::cout << checks << " item wire checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "Check " << checks << ": " << e.what() << '\n'; return 1; }
}
