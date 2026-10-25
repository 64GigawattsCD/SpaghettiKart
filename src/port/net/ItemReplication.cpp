#include "ItemReplication.h"
#include "ItemWire.h"
#include "Session.h"
#include "engine/World.h"
#include "engine/vehicles/Boat.h"
#include "engine/vehicles/Bus.h"
#include "engine/vehicles/Car.h"
#include "engine/vehicles/TankerTruck.h"
#include "engine/vehicles/Train.h"
#include "engine/vehicles/Truck.h"

extern "C" {
#include "actor_types.h"
#include "code_800029B0.h"
#include "code_80057C60.h"
#include "defines.h"
#include "main.h"
#include "objects.h"
#include "racing/actors.h"
#include "racing/collision.h"
#include "update_objects.h"
}

namespace KartNet {
namespace {
static_assert(ACTOR_BANANA == ItemWire::Banana && ACTOR_BLUE_SPINY_SHELL == ItemWire::BlueShell &&
              ACTOR_HOT_AIR_BALLOON_ITEM_BOX == ItemWire::BalloonBox && ACTOR_KIWANO_FRUIT == ItemWire::Kiwano &&
              ACTOR_BOX_TRUCK == ItemWire::Truck && ACTOR_PADDLE_BOAT == ItemWire::Boat &&
              ACTOR_SCHOOL_BUS == ItemWire::Bus && ACTOR_TANKER_TRUCK == ItemWire::Tanker && ACTOR_CAR == ItemWire::Car);
uint32_t generation = 0;
ItemWire::Reassembler assembler;
ItemWire::Frame lastFrame;
bool haveFrame = false;

void Fail(Session& s, const char* text) { s.error = text; s.phase = Phase::Error; }
uint16_t Owner(const Session& s, int slot) { return slot >= 0 && slot < 8 ? s.ParticipantForSlot(slot) : 0; }
int16_t Slot(const Session& s, uint16_t participant) { return participant ? int16_t(s.EngineSlot(participant)) : -1; }
bool IsHuman(const Session& s, uint16_t participant) {
    return std::any_of(s.roster.begin(), s.roster.end(), [participant](const auto& p) { return p.participant == participant; });
}
int16_t Link(int index, int type) {
    if (index < 0 || size_t(index) >= CM_GetActorSize()) return -1;
    const auto* a = CM_GetActor(size_t(index));
    return a->flags && a->type == type ? int16_t(index) : -1;
}
ItemWire::Actor CaptureActor(const Session& s, size_t index, const ::Actor& actor) {
    ItemWire::Actor a; a.index = uint16_t(index); a.type = uint8_t(actor.type);
    a.flags = uint16_t(actor.flags); a.state = actor.state;
    if (a.type == ItemWire::BananaBunch) {
        const auto& b = reinterpret_cast<const BananaBunchParent&>(actor);
        a.owner = Owner(s, b.playerId); a.timer = b.unk_04; a.param = b.bananasAvailable;
        for (int n = 0; n < 5; ++n) a.links[n] = Link(b.bananaIndices[n], ACTOR_BANANA);
        return a; // This layout overlays the position with banana indices.
    }
    a.timer = actor.unk_04; a.auxiliary = actor.unk_08; a.radius = actor.boundingBoxSize;
    std::copy_n(actor.rot, 3, a.rotation.begin()); std::copy_n(actor.pos, 3, a.position.begin());
    std::copy_n(actor.velocity, 3, a.velocity.begin());
    if (ItemWire::Shell(a.type)) {
        const auto& b = reinterpret_cast<const ShellActor&>(actor);
        a.owner = Owner(s, b.playerId); a.rotation[2] = 0;
        if (b.state == TRIPLE_GREEN_SHELL || b.state == TRIPLE_RED_SHELL) {
            a.parent = b.parentIndex; a.timer = 0;
        } else if (a.type != ItemWire::GreenShell &&
                   (b.state == RED_SHELL_LOCK_ON || b.state == BLUE_SHELL_LOCK_ON || b.state == BLUE_SHELL_TARGET_ELIMINATED)) {
            a.target = Owner(s, b.targetPlayer); a.timer = 0;
        }
    } else if (a.type == ItemWire::Banana) {
        const auto& b = reinterpret_cast<const BananaActor&>(actor);
        a.auxiliary = 0; // The float aliases two shorts, not a floating-point quantity.
        if (b.state != DESTROYED_BANANA) {
            a.owner = Owner(s, b.playerId); a.rotation = {};
            if (b.state == FIRST_BANANA_BUNCH_BANANA || b.state == BANANA_BUNCH_BANANA) {
                a.parent = b.parentIndex; a.param = b.bananaId;
                a.links[0] = Link(b.elderIndex, ACTOR_BANANA); a.links[1] = Link(b.youngerIndex, ACTOR_BANANA);
            }
        }
    } else if (a.type == ItemWire::FakeBox) {
        const auto& b = reinterpret_cast<const FakeItemBox&>(actor);
        a.owner = std::isfinite(b.playerId) && b.playerId >= 0 && b.playerId < 8 && std::floor(b.playerId) == b.playerId
            ? Owner(s, int(b.playerId)) : 0;
        a.velocity[0] = 0; // Owner travels as participant, never as a host engine slot.
    } else if (ItemWire::Triple(a.type)) {
        const auto& b = reinterpret_cast<const TripleShellParent&>(actor);
        a.owner = Owner(s, b.playerId); a.rotation[2] = 0; a.param = b.shellsAvailable;
        a.timer = 0; a.velocity = {};
        for (int n = 0; n < 3; ++n) {
            const float link = b.shellIndices[n];
            if (std::isfinite(link) && link >= 0 && link < ItemWire::MaxSlots && std::floor(link) == link)
                a.links[n] = Link(int(link), a.type == ItemWire::TripleGreen ? ACTOR_GREEN_SHELL : ACTOR_RED_SHELL);
        }
    } else if (a.type == ACTOR_KIWANO_FRUIT) {
        a.owner = Owner(s, actor.unk_04); a.timer = 0;
    }
    return a;
}

bool CaptureInventories(const Session& s, ItemWire::Frame& f) {
    std::array<bool, OBJECT_LIST_SIZE> used{};
    for (int slot = 0; slot < 8; ++slot) {
        auto& i = f.inventories[slot]; i.participant = uint8_t(s.ParticipantForSlot(slot));
        i.copiedItem = gPlayers[slot].currentItemCopy;
        if (gPlayerBalloonCount[slot] < -1 || gPlayerBalloonCount[slot] > 2) return false;
        i.balloons = int8_t(gPlayerBalloonCount[slot]);
        std::copy_n(gPlayerBalloonStatus[slot], 3, i.balloonStatus.begin());
        const int id = gItemWindowObjectByPlayerId[slot];
        if (id < 0 || id >= OBJECT_LIST_SIZE || used[id]) {
            if (IsHuman(s, i.participant)) return false;
            continue;
        }
        used[id] = true;
        const auto& o = gObjectList[id];
        // The state machine's shorts must be checked before narrowing to wire bytes.
        if (o.type < 0 || o.type >= ITEM_MAX || o.textureListIndex < 0 || o.textureListIndex >= ITEM_MAX ||
            o.unk_0A2 < 0 || o.unk_0A2 >= ITEM_MAX || o.unk_0D6 > 3) return false;
        i.present = 1; i.item = uint8_t(o.type); i.icon = uint8_t(o.textureListIndex);
        i.nextItem = uint8_t(o.unk_0A2); i.mode = o.unk_0D6; i.state = o.state;
        i.goldenTimer = o.animationTimer; i.timer = o.unk_04C; i.status = uint32_t(o.status);
    }
    return true;
}

void InitializePlaceholder(::Actor& a) {
    a.type = 0; a.flags = 0; a.unk_04 = 0; a.state = 0; a.unk_08 = 0; a.boundingBoxSize = 0;
    std::fill_n(a.rot, 3, int16_t(0)); a.unk_16 = 0;
    std::fill_n(a.pos, 3, 0.0f); std::fill_n(a.velocity, 3, 0.0f);
    a.model = nullptr; func_802AAAAC(&a.unk30);
}
bool Compatible(const Session& s, const ItemWire::Frame& f) {
    const auto* w = GetWorld();
    if (!w || w->Actors.size() > ItemWire::MaxSlots) return false;
    std::array<bool, OBJECT_LIST_SIZE> used{};
    for (const auto& i : f.inventories) {
        const int slot = s.EngineSlot(i.participant);
        if (slot < 0 || slot >= 8) return false;
        if (!i.present) continue;
        const int id = gItemWindowObjectByPlayerId[slot];
        if (id < 0 || id >= OBJECT_LIST_SIZE || used[id]) return false;
        used[id] = true;
    }
    for (const auto& a : f.actors) {
        if (a.owner && Slot(s, a.owner) < 0) return false;
        if (a.target && Slot(s, a.target) < 0) return false;
        if (a.index >= w->Actors.size()) continue;
        auto* local = w->Actors[a.index].get();
        if (!local || local->IsMod()) return false;
        // A local track object at this index means the course layouts disagree.
        if (local->Flags && local->Type != a.type && !ItemWire::Item(uint8_t(local->Type))) return false;
    }
    return true;
}
void ApplyActor(const Session& s, const ItemWire::Actor& a, ::Actor& actor) {
    actor.type = a.type; actor.flags = int16_t(a.flags); actor.state = a.state; actor.unk_04 = a.timer;
    actor.unk_08 = a.auxiliary; actor.boundingBoxSize = a.radius;
    std::copy(a.rotation.begin(), a.rotation.end(), actor.rot);
    std::copy(a.position.begin(), a.position.end(), actor.pos);
    std::copy(a.velocity.begin(), a.velocity.end(), actor.velocity);
    if (a.type == ItemWire::BananaBunch) {
        auto& b = reinterpret_cast<BananaBunchParent&>(actor);
        b.playerId = Slot(s, a.owner); b.bananasAvailable = a.param;
        std::copy(a.links.begin(), a.links.end(), b.bananaIndices);
    } else if (ItemWire::Shell(a.type)) {
        auto& b = reinterpret_cast<ShellActor&>(actor);
        b.playerId = Slot(s, a.owner);
        if (b.state == TRIPLE_GREEN_SHELL || b.state == TRIPLE_RED_SHELL) b.parentIndex = a.parent;
        else if (a.target) b.targetPlayer = Slot(s, a.target);
    } else if (a.type == ItemWire::Banana) {
        auto& b = reinterpret_cast<BananaActor&>(actor);
        b.parentIndex = a.parent; b.bananaId = a.param;
        if (b.state != DESTROYED_BANANA) {
            b.playerId = Slot(s, a.owner); b.elderIndex = a.links[0]; b.youngerIndex = a.links[1];
        }
    } else if (a.type == ItemWire::FakeBox) {
        reinterpret_cast<FakeItemBox&>(actor).playerId = float(Slot(s, a.owner));
    } else if (ItemWire::Triple(a.type)) {
        auto& b = reinterpret_cast<TripleShellParent&>(actor);
        b.playerId = Slot(s, a.owner); b.shellsAvailable = a.param;
        for (int n = 0; n < 3; ++n) b.shellIndices[n] = float(a.links[n]);
    } else if (a.type == ACTOR_KIWANO_FRUIT) actor.unk_04 = Slot(s, a.owner);
    // Re-query only static geography for shadows. No player/item collision or item update runs here.
    func_802AAAAC(&actor.unk30);
    if (ItemWire::Shell(a.type))
        check_bounding_collision(&actor.unk30, actor.boundingBoxSize + 1.0f, actor.pos[0], actor.pos[1], actor.pos[2]);
}

// Vehicle controllers carry their own transforms in addition to the C render actor.
// Copy into those fields without casting an arbitrary IsMod actor to the stock layout.
template<class T> void SyncRoadVehicle(T& vehicle) {
    if (vehicle.ActorIndex < 0 || size_t(vehicle.ActorIndex) >= CM_GetActorSize()) return;
    const auto& a = *CM_GetActor(size_t(vehicle.ActorIndex));
    std::copy_n(a.pos, 3, vehicle.Position); std::copy_n(a.velocity, 3, vehicle.Velocity);
    std::copy_n(a.rot, 3, vehicle.Rotation);
    if (gIsMirrorMode) vehicle.Rotation[1] = int16_t(-int32_t(vehicle.Rotation[1]));
}
void SyncModVehicles() {
    for (auto& ptr : GetWorld()->Actors) {
        if (!ptr->IsMod()) continue;
        if (auto* v = dynamic_cast<ACar*>(ptr.get())) SyncRoadVehicle(*v);
        else if (auto* v = dynamic_cast<ABus*>(ptr.get())) SyncRoadVehicle(*v);
        else if (auto* v = dynamic_cast<ATruck*>(ptr.get())) SyncRoadVehicle(*v);
        else if (auto* v = dynamic_cast<ATankerTruck*>(ptr.get())) SyncRoadVehicle(*v);
        else if (auto* v = dynamic_cast<ABoat*>(ptr.get())) {
            if (v->ActorIndex < 0 || size_t(v->ActorIndex) >= CM_GetActorSize()) continue;
            const auto& a = *CM_GetActor(size_t(v->ActorIndex));
            std::copy_n(a.pos, 3, v->Position); std::copy_n(a.velocity, 3, v->Velocity);
            v->RotY = gIsMirrorMode ? int16_t(-int32_t(a.rot[1])) : a.rot[1];
        } else if (auto* v = dynamic_cast<ATrain*>(ptr.get())) {
            const auto sync = [](TrainCarStuff& car) {
                if (car.actorIndex < 0 || size_t(car.actorIndex) >= CM_GetActorSize()) return;
                const auto& a = *CM_GetActor(size_t(car.actorIndex));
                std::copy_n(a.pos, 3, car.position); std::copy_n(a.velocity, 3, car.velocity);
            };
            sync(v->Locomotive);
            if (v->Tender.isActive) sync(v->Tender);
            for (auto& car : v->PassengerCars) if (car.isActive) sync(car);
        }
    }
}
void ApplyFrame(const Session& s, const ItemWire::Frame& f) {
    auto* w = GetWorld();
    while (w->Actors.size() < f.slots) InitializePlaceholder(*w->AddBaseActor());
    std::array<bool, ItemWire::MaxSlots> seen{};
    for (const auto& a : f.actors) seen[a.index] = true;
    for (size_t n = 0; n < w->Actors.size(); ++n) {
        auto* p = w->Actors[n].get();
        if (!p->IsMod() && ItemWire::Supported(uint8_t(p->Type)) && !seen[n]) {
            p->Flags = 0; p->Type = 0;
        }
    }
    for (const auto& a : f.actors) {
        auto* actor = CM_GetActor(a.index);
        ApplyActor(s, a, *actor);
        if (a.type == ItemWire::BalloonBox) gActorHotAirBalloonItemBox = actor;
    }
    gNumActors = f.active; gNumPermanentActors = f.permanent;
    for (const auto& i : f.inventories) {
        const int slot = s.EngineSlot(i.participant);
        gPlayers[slot].currentItemCopy = i.copiedItem;
        gPlayerBalloonCount[slot] = i.balloons;
        std::copy(i.balloonStatus.begin(), i.balloonStatus.end(), gPlayerBalloonStatus[slot]);
        if (!i.present) continue;
        auto& o = gObjectList[gItemWindowObjectByPlayerId[slot]];
        o.type = i.item; o.state = i.state; o.textureListIndex = int8_t(i.icon);
        o.unk_0A2 = i.nextItem; o.unk_0D6 = i.mode; o.animationTimer = i.goldenTimer;
        o.unk_04C = i.timer; o.status = int32_t(i.status);
        o.activeTexture = gItemWindowTextures[i.icon]; o.activeTLUT = gItemWindowTLUTs[i.icon];
        // HUD slide offsets belong to this machine's layout, not the host's viewport count.
        if (slot < s.LocalCount()) {
            const bool visible = (i.state >= 2 && i.state <= 10) || i.state >= 20;
            const auto step = [](int16_t current, int target, int amount) {
                return int16_t(current + std::clamp(target - int(current), -amount, amount));
            };
            const int targetY = gActiveScreenMode == 0 && visible ? 64 : 0;
            const int targetX = gActiveScreenMode == 0 || !visible ? 0 :
                (gActiveScreenMode == 3 && (slot == 1 || slot == 3) ? -128 : 128);
            playerHUD[slot].slideItemBoxY = step(playerHUD[slot].slideItemBoxY, targetY, 4);
            playerHUD[slot].slideItemBoxX = step(playerHUD[slot].slideItemBoxX, targetX, 8);
        }
    }
    SyncModVehicles();
}
}

void ResetItemWorld() { generation = 0; assembler.Reset(); lastFrame = {}; haveFrame = false; }
void CaptureItemWorld(Session& s, bool finalFrame) {
    if (!s.Active() || !s.IsHost() || s.LocalOnly() || (s.phase != Phase::Racing && s.phase != Phase::Finished)) return;
    const auto* w = GetWorld();
    if (!w || w->Actors.size() > ItemWire::MaxSlots) { Fail(s, "Item world exceeds indexed actor capacity"); return; }
    ItemWire::Frame f; f.generation = ++generation; f.slots = uint16_t(w->Actors.size());
    f.active = gNumActors; f.permanent = gNumPermanentActors;
    for (size_t n = 0; n < w->Actors.size(); ++n) {
        auto* p = w->Actors[n].get();
        if (!p || p->IsMod()) continue;
        const auto* actor = CM_GetActor(n);
        if (!actor->flags || actor->type < 0 || actor->type > 255 || !ItemWire::Supported(uint8_t(actor->type))) continue;
        if (f.actors.size() == ItemWire::MaxActors) { Fail(s, "Item world exceeds active actor capacity"); return; }
        f.actors.push_back(CaptureActor(s, n, *actor));
    }
    if (!CaptureInventories(s, f)) { Fail(s, "Eight distinct initialized item inventories are required"); return; }
    auto chunks = ItemWire::Chunk(f);
    if (chunks.empty()) { Fail(s, "Item world contains unsupported stock state"); return; }
    s.PublishWorldFrame(chunks, finalFrame);
}
void ApplyItemWorld(Session& s) {
    if (!s.Active() || s.IsHost() || s.LocalOnly() || (s.phase != Phase::Racing && s.phase != Phase::Finished)) return;
    for (const auto& bytes : s.TakeWorld()) {
        ItemWire::Frame f;
        if (!assembler.Push(bytes, true, f)) continue; // Session authenticated host and current race epoch.
        if (!Compatible(s, f)) { Fail(s, "Host item actors or inventories do not match the loaded course"); return; }
        lastFrame = std::move(f); haveFrame = true;
    }
    if (haveFrame) {
        if (!Compatible(s, lastFrame)) { Fail(s, "Client item layout changed during the race"); return; }
        ApplyFrame(s, lastFrame);
    }
}
}
