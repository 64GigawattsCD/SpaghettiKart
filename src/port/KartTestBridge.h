#pragma once
#include <libultraship/libultra/controller.h>
#ifdef __cplusplus
namespace Fast { class Interpreter; }
namespace KartTest {
bool WantsStereo();
void Capture(Fast::Interpreter* renderer, int framebuffer, int width, int height, const char* eye);
void Publish();
}
extern "C" {
#endif
int kart_test_active(void);
// Cached at test startup. Place SK_TEST_ROOT/menu-boot before launch and remove
// it in the runner's teardown. No marker retains the existing quick-race boot.
int kart_test_menu_boot(void);
// Returns 1 only during a pointerUi step. Coordinates are relative to the
// rendered image (0..1 inside; outside coordinates permit drag-off tests).
int kart_test_pointer_ui(float* x, float* y, int* down);
// Test queues accept pointer: [nativeX, nativeY, 0|1]. Clicks need explicit
// press/release steps; omitted pointers, stop and heartbeat expiry cancel holds.
void kart_test_read_inputs(OSContPad* pads);
#ifdef __cplusplus
}
#endif
