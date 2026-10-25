#include "../../src/menu_pointer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static void frame(int context) {
    menu_pointer_begin();
    menu_pointer_target(context, 1, 20,30,60,25);
    menu_pointer_target(context, 2, 80,30,60,25);
}
int main(void) {
    frame(100);
    menu_pointer_inject(40,40,1);
    CHECK(menu_pointer_take(100)==-1);
    for(int i=0;i<100;++i) { frame(100); menu_pointer_end(); menu_pointer_inject(40,40,1); }
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(100)==1);
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,1); menu_pointer_inject(90,40,0);
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,1); menu_pointer_inject(-100,40,0);
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,1); frame(101); menu_pointer_end();
    frame(100); menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,1); menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(101)==-1);
    menu_pointer_end(); CHECK(menu_pointer_take(100)==-1);
    menu_pointer_inject(40,40,1); menu_pointer_cancel(); menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(100)==-1);
    menu_pointer_clear_targets(); menu_pointer_target(200,3,20,30,60,25);
    menu_pointer_inject(40,40,1); menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(100)==-1); CHECK(menu_pointer_take(200)==3);
    menu_pointer_inject(NAN,INFINITY,1); menu_pointer_inject(40,40,0);
    CHECK(menu_pointer_take(200)==-1);
    frame(100); menu_pointer_inject(80,40,1); menu_pointer_inject(80,40,0);
    CHECK(menu_pointer_take(100)==2);
    puts("pointer release, drag-off, held, epoch, expiry, modal, nonfinite and edge tests passed");
    return 0;
}
