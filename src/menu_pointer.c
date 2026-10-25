#include "menu_pointer.h"
#include <math.h>

typedef struct { int context, id; float x, y, w, h; } PointerTarget;
static PointerTarget targets[256];
static int count, pressed, armedContext = -1, armedId = -1;
static int pendingContext = -1, pendingId = -1;

void menu_pointer_begin(void) { count = 0; }
void menu_pointer_end(void) {
    pendingContext = pendingId = -1;
    for (int i=0; i<count; ++i)
        if (targets[i].context == armedContext && targets[i].id == armedId) return;
    armedContext = armedId = -1;
}
void menu_pointer_clear_targets(void) { count = 0; }
void menu_pointer_cancel(void) {
    pressed = 0;
    armedContext = armedId = pendingContext = pendingId = -1;
}
void menu_pointer_target(int context, int id, float x, float y, float w, float h) {
    if (count >= 256 || id < 0 || w <= 0 || h <= 0) return;
    targets[count++] = (PointerTarget){context, id, x, y, w, h};
}
int menu_pointer_count(void) { return count; }
int menu_pointer_get(int index, int* context, int* id, float* x, float* y, float* w, float* h) {
    if (index < 0 || index >= count) return 0;
    const PointerTarget* t = &targets[index];
    *context = t->context; *id = t->id; *x = t->x; *y = t->y; *w = t->w; *h = t->h;
    return 1;
}
void menu_pointer_inject(float x, float y, int down) {
    int context = -1, id = -1;
    if (isfinite(x) && isfinite(y)) {
        for (int i = count - 1; i >= 0; --i) {
            PointerTarget* t = &targets[i];
            if (x >= t->x && y >= t->y && x < t->x+t->w && y < t->y+t->h) {
                context = t->context; id = t->id; break;
            }
        }
    }
    if (down && !pressed) { armedContext = context; armedId = id; }
    if (!down && pressed) {
        if (id >= 0 && id == armedId && context == armedContext) {
            pendingContext = context; pendingId = id;
        }
        armedContext = armedId = -1;
    }
    pressed = !!down;
}
int menu_pointer_take(int context) {
    if (pendingContext != context) return -1;
    int id = pendingId;
    pendingContext = pendingId = -1;
    return id;
}
