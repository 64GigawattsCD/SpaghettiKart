#include "../../src/hud_rpm.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

void check(bool pass) { if (!pass) std::exit(1); }
int main() {
    check(hud_rpm_needle_rotation(0) > 56000);
    check(hud_rpm_needle_rotation(.491f) > 65000);
    check(hud_rpm_needle_rotation(.493f) < 100);
    check(hud_rpm_needle_rotation(1) > 9200 && hud_rpm_needle_rotation(1) < 9300);
    check(hud_rpm_needle_rotation(-1) == hud_rpm_needle_rotation(0));
    check(hud_rpm_needle_rotation(2) == hud_rpm_needle_rotation(1));
    check(hud_rpm_needle_rotation(std::numeric_limits<float>::quiet_NaN()) == hud_rpm_needle_rotation(0));
    int previous = hud_rpm_needle_rotation(0);
    for (int i = 1; i <= 1000; ++i) {
        int angle = hud_rpm_needle_rotation(i / 1000.0f);
        int delta = (angle - previous + 65536) % 65536;
        check(delta >= 17 && delta <= 19);
        previous = angle;
    }
    std::cout << "RPM angle sweep passed\n";
}
