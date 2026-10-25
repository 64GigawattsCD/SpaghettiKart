#pragma once

namespace KartNet {
class Session;
// Session owns authenticated, epoch-filtered host World messages. Invoke capture only
// after authority simulation; invoke apply after client visual/HUD ticks, before draw.
// Mark the last frame before Finish so Session reliably sends/caches/replays the group.
void CaptureItemWorld(Session& session, bool finalFrame = false);
void ApplyItemWorld(Session& session);
void ResetItemWorld();
// Stock C item layouts are supported. Course scripts, custom actor-private state,
// mod-only objects/particles and the falling-rock private respawn timer are not
// part of this sidecar. Parent integration must suppress client item simulation,
// initialize eight distinct inventory objects, and update all inventories on host.
// Full frames cover <=256 active records and <=8192 indexed slots (the engine
// currently appends stock actors instead of reusing inactive indices). Capacity
// or course-layout mismatches stop the session with an error. No paid services
// were used for this module; tests/network/item_wire_test.cpp is engine-independent.
}
