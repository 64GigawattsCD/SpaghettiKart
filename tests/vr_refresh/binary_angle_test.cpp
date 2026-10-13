#include "../../src/port/interpolation/BinaryAngle.h"
#include <cstdio>
#include <cstdlib>

using FrameInterpolation::InterpolateBinaryAngle;

static void Check(uint16_t oldAngle, uint16_t newAngle, float step, uint16_t expected) {
    const auto actual = static_cast<uint16_t>(InterpolateBinaryAngle(
        static_cast<int16_t>(oldAngle), static_cast<int16_t>(newAngle), step));
    if (actual != expected) {
        std::fprintf(stderr, "%04x -> %04x at %.2f: expected %04x, got %04x\n",
                     oldAngle, newAngle, step, expected, actual);
        std::exit(1);
    }
}

int main() {
    Check(0x1000, 0x2000, 0.5f, 0x1800);
    Check(0x2000, 0x1000, 0.5f, 0x1800);
    Check(0x7f00, 0x8100, 0.5f, 0x8000);
    Check(0x8100, 0x7f00, 0.5f, 0x8000);
    Check(0xff00, 0x0100, 0.5f, 0x0000);
    Check(0x0100, 0xff00, 0.5f, 0x0000);
    Check(0xff00, 0x0100, 0.25f, 0xff80);
    Check(0xff00, 0x0100, 0.75f, 0x0080);
    Check(0x0100, 0xff00, 0.25f, 0x0080);
    Check(0x0100, 0xff00, 0.75f, 0xff80);
    Check(0x0000, 0x4000, 0.5f, 0x2000);
    Check(0x0000, 0x4001, 0.5f, 0x4001);
    Check(0x0000, 0x8000, 0.5f, 0x8000);
    Check(0x1000, 0xc000, 0.5f, 0xc000);
    for (uint32_t angle = 0; angle < 65536; ++angle) {
        Check(angle, angle, 0.5f, angle);
        const auto next = static_cast<uint16_t>(angle + 256);
        Check(angle, next, 0.0f, angle);
        Check(angle, next, 0.5f, static_cast<uint16_t>(angle + 128));
        Check(angle, next, 1.0f, next);
    }
    std::puts("Binary-angle regression checks passed (262158 cases).");
}
