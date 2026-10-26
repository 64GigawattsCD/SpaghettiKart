#include "hit_radio_model.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

extern "C" int radio_model_c_test(void);
static int checks;
static void check(bool pass, const char* message) {
    ++checks;
    if (!pass) { std::cerr << message << '\n'; std::exit(1); }
}
static bool near(float a, float b) { return std::abs(a - b) < .002f; }
static float bottom(HudRect r) { return r.y + r.h; }
static bool inside(HudRect a, HudRect b) {
    return a.x >= b.x - .002f && a.y >= b.y - .002f &&
        a.x + a.w <= b.x + b.w + .002f && bottom(a) <= bottom(b) + .002f;
}

static void timing() {
    HitRadioModel model;
    hit_radio_model_reset(&model);
    check(radio_model_c_test() == 1, "C header/API compatibility");
    for (int i = 0; i < 4; ++i)
        check(!model.slots[i].active && model.slots[i].character == -1, "reset slots");
    for (int count : {-1, 0, 1, 2, 3, 4, 5}) {
        for (int slot = -2; slot < 6; ++slot) {
            for (int character = -2; character < 10; ++character) {
                hit_radio_model_reset(&model);
                bool valid = count > 0 && count <= 4 && slot >= 0 && slot < count && character >= 0 && character < 8;
                check(hit_radio_model_show(&model, count, slot, character) == valid, "input validation");
            }
        }
    }
    hit_radio_model_reset(&model);
    check(hit_radio_model_show(&model, 4, 2, 5) == 1, "first event");
    check(!model.slots[0].active && !model.slots[1].active && !model.slots[3].active, "victim only");
    check(near(hit_radio_model_reveal(&model.slots[2]), 0), "initial hidden pose");
    for (float dt : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        hit_radio_model_tick(&model, dt);
    check(near(model.slots[2].age, 0) && near(model.slots[2].cooldown, .75f), "invalid/paused ticks");
    hit_radio_model_tick(&model, .075f);
    check(near(hit_radio_model_reveal(&model.slots[2]), .5f), "slide midpoint");
    hit_radio_model_tick(&model, .075f);
    check(near(hit_radio_model_reveal(&model.slots[2]), 1), "150ms entry");
    hit_radio_model_tick(&model, .599f);
    for (int i = 0; i < 100; ++i) check(!hit_radio_model_show(&model, 4, 2, i % 8), "cooldown spam");
    check(model.slots[2].character == 5 && near(model.slots[2].age, .749f), "rejected events leave active portrait");
    hit_radio_model_tick(&model, .002f);
    check(hit_radio_model_show(&model, 4, 2, 7), "replacement after 750ms");
    check(model.slots[2].character == 7 && near(model.slots[2].age, 0), "replacement does not stack");
    check(hit_radio_model_show(&model, 4, 1, 0), "independent slot cooldown");
    hit_radio_model_tick(&model, 1.65f);
    check(near(hit_radio_model_reveal(&model.slots[2]), 1), "1.5 second hold after entry");
    hit_radio_model_tick(&model, .075f);
    check(near(hit_radio_model_reveal(&model.slots[2]), .5f), "exit midpoint");
    hit_radio_model_tick(&model, .076f);
    check(!model.slots[2].active && !model.slots[1].active, "1800ms expiry");
    hit_radio_model_tick(&model, 100.f);
    check(!model.slots[2].active, "no queued portrait");
    check(hit_radio_model_show(&model, 4, 2, 0), "expired slot accepts");
    hit_radio_model_reset(&model);
    check(!model.slots[2].active && hit_radio_model_show(&model, 4, 2, 1), "race reset clears cooldown");
    hit_radio_model_tick(&model, std::numeric_limits<float>::max());
    check(!model.slots[2].active && model.slots[2].cooldown == 0, "large finite tick saturates");
    for (float step : {1.f / 30, 1.f / 60, 1.f / 144}) {
        hit_radio_model_reset(&model);
        hit_radio_model_show(&model, 1, 0, 0);
        for (int i = 0; i < int(1.9f / step); ++i) hit_radio_model_tick(&model, step);
        check(!model.slots[0].active, "rate independent expiry");
    }
}

static void geometry() {
    for (float aspect : {1.f, 4.f / 3, 16.f / 9, 2.17f, 21.f / 9, 32.f / 9}) {
        for (int mode = 0; mode < 4; ++mode) {
            int count = mode == 0 ? 1 : mode == 3 ? 4 : 2;
            for (int player = 0; player < count; ++player) {
                auto view = hud_race_viewport(aspect, mode, player);
                auto hud = hud_race_layout(view, 1, hud_layout_vec2(64, 64), true, true);
                for (int phone = 0; phone < 2; ++phone) {
                    for (int frame = 0; frame <= 120; ++frame) {
                        float reveal = frame / 120.f;
                        auto layout = hit_radio_model_layout(hud, reveal, phone, aspect);
                        auto visible = hit_radio_clip(layout.panel, layout.clip);
                        if (visible.w && visible.h) check(inside(visible, view), "animated panel crosses viewport");
                        check(near(layout.panel.x + layout.panel.w / 2, view.x + view.w / 2), "viewport center");
                        if (frame != 120) continue;
                        check(inside(layout.panel, view), "held panel bounds");
                        check(near(bottom(layout.panel), bottom(view) - 4), "four unit bottom inset");
                        check(layout.panel.x > view.x + view.w * .3f &&
                              layout.panel.x + layout.panel.w < view.x + view.w * .7f, "central control corridor");
                        if (phone) {
                            check(layout.panel.h <= 28.002f, "phone height cap");
                            for (int touchMode = 0; touchMode < 2; ++touchMode) {
                                float w = 240 * aspect, unit = std::min(240.f / 7.5f, w / 14);
                                float column = std::min(1.f, w / 320) * 83.2f;
                                float controlEdge = touchMode == 0 ? w - column : w;
                                float pedalX = controlEdge - 1.85f * unit, pedalY = 240 - 1.85f * unit;
                                float gx = pedalX - 4 * unit, gateUnit = .8f * unit;
                                // Java x is left-origin; native wide HUD x is centered at 160.
                                HudRect gate = hud_layout_rect(160 - w / 2 + gx - gateUnit,
                                    pedalY - gateUnit, 2.85f * gateUnit, 2 * gateUnit);
                                bool overlapX = layout.panel.x < gate.x + gate.w &&
                                    layout.panel.x + layout.panel.w > gate.x;
                                check(!overlapX || bottom(layout.panel) <= gate.y ||
                                    layout.panel.y >= bottom(gate) + 1.99f, "manual shifter overlap");
                                if (bottom(view) > 200)
                                    check(layout.panel.y >= bottom(gate) + 1.99f, "phone panel below manual gate");
                            }
                        }
                    }
                }
            }
        }
    }
    auto vr = hud_race_layout(hud_race_viewport(4.f / 3, 0, 0), 1, hud_layout_vec2(64, 64), true, true);
    check(near(hit_radio_model_layout(vr, 1, 0, 4.f / 3).panel.h, 48), "desktop/VR reference size");
}

int main() {
    timing();
    geometry();
    std::cout << checks << " radio HUD checks passed\n";
}
