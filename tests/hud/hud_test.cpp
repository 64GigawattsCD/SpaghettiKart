#include "hud_race_layout.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

std::vector<int> order;
void paint(const HudLayoutContext*, HudWidgetId id, HudRect, void*) { order.push_back(id); }

void check(bool pass, const char* message) { if (!pass) { std::cerr << message << '\n'; std::exit(1); } }
bool near(float a, float b) { return std::abs(a-b) < .002f; }
float edge(HudRect r) { return r.x+r.w; }
float bottom(HudRect r) { return r.y+r.h; }
bool inside(HudRect a, HudRect b) {
    return a.x >= b.x-.002f && a.y >= b.y-.002f && edge(a) <= edge(b)+.002f && bottom(a) <= bottom(b)+.002f;
}
int main() {
    HudLayoutContext ctx;
    auto zero = hud_layout_padding(0,0,0,0);
    hud_layout_begin(&ctx, hud_layout_rect(40,60,200,100));
    auto scale = hud_layout_scale_box(&ctx, HUD_SCALE_STRETCH_FIT, HUD_SCALE_DIRECTION_BOTH, 1);
    auto box = hud_layout_vertical_box(&ctx, 4, zero);
    auto a = hud_layout_draw(&ctx, hud_layout_vec2(100,20), nullptr, nullptr);
    auto b = hud_layout_draw(&ctx, hud_layout_vec2(100,20), nullptr, nullptr);
    hud_layout_box_add(&ctx,box,a,hud_layout_auto_slot(zero,hud_layout_vec2(1,0),false));
    hud_layout_box_add(&ctx,box,b,hud_layout_auto_slot(zero,hud_layout_vec2(1,0),false));
    hud_layout_single_child_add(&ctx,scale,box,hud_layout_single_child_slot(zero,hud_layout_vec2(1,0),false,false));
    hud_layout_canvas_add(&ctx,ctx.rootId,scale,hud_layout_canvas_slot(hud_layout_anchor(1,0,1,0),
        hud_layout_padding(-10,5,50,22),hud_layout_vec2(1,0),false,0));
    hud_layout_arrange(&ctx);
    check(near(ctx.widgets[a].arrangedRect.x,180) && near(ctx.widgets[a].arrangedRect.h,10),"nested ScaleBox geometry");
    check(near(ctx.widgets[b].arrangedRect.y,77),"scaled VBox spacing");
    hud_layout_arrange(&ctx);
    check(near(ctx.widgets[b].arrangedRect.y,77),"repeat arrange accumulates scale");
    ctx.widgets[b].visible=false;
    hud_layout_arrange(&ctx);
    check(near(ctx.widgets[box].desiredSize.y,20),"collapsed box slot retains padding/spacing");
    auto count=ctx.slotCount;
    hud_layout_box_add(&ctx,box,box,hud_layout_auto_slot(zero,hud_layout_vec2(0,0),false));
    check(ctx.overflowed && ctx.slotCount==count,"cycle accepted");
    hud_layout_canvas_add(&ctx,ctx.rootId,999,hud_layout_canvas_slot(hud_layout_anchor(0,0,0,0),zero,hud_layout_vec2(0,0),true,0));
    check(ctx.slotCount==count,"invalid child accepted");

    hud_layout_begin(&ctx,hud_layout_rect(0,0,200,100));
    auto grid=hud_layout_grid_box(&ctx,zero);
    a=hud_layout_draw(&ctx,hud_layout_vec2(100,30),paint,nullptr);
    b=hud_layout_draw(&ctx,hud_layout_vec2(30,10),paint,nullptr);
    hud_layout_grid_add(&ctx,grid,a,hud_layout_grid_slot(0,0,1,2,5,hud_layout_vec2(0,0),zero,hud_layout_vec2(0,0),true,true));
    hud_layout_grid_add(&ctx,grid,b,hud_layout_grid_slot(0,0,1,1,1,hud_layout_vec2(0,0),zero,hud_layout_vec2(0,0),false,false));
    hud_layout_canvas_add(&ctx,ctx.rootId,grid,hud_layout_canvas_slot(hud_layout_anchor(0,0,0,0),zero,hud_layout_vec2(0,0),true,0));
    hud_layout_arrange(&ctx);
    check(near(ctx.widgets[grid].desiredSize.x,100),"grid span measurement");
    check(near(ctx.widgets[a].arrangedRect.w,100),"grid span arrangement");
    hud_layout_draw_tree(&ctx);
    check(order==std::vector<int>{b,a},"grid layer order");
    auto c=hud_layout_draw(&ctx,hud_layout_vec2(10,10),nullptr,nullptr);
    count=ctx.slotCount;
    hud_layout_grid_add(&ctx,grid,c,hud_layout_grid_slot(-1,0,1,1,0,hud_layout_vec2(0,0),zero,hud_layout_vec2(0,0),false,false));
    check(ctx.overflowed && ctx.slotCount==count,"negative grid index accepted");

    hud_layout_begin_safe_zone(&ctx,hud_layout_rect(20,30,200,100),hud_layout_padding(10,10,10,10));
    auto canvas=hud_layout_canvas(&ctx);
    a=hud_layout_draw(&ctx,hud_layout_vec2(10,10),nullptr,nullptr);
    hud_layout_canvas_add(&ctx,ctx.rootId,canvas,hud_layout_canvas_slot(hud_layout_anchor(0,0,0,0),
        hud_layout_padding(20,20,60,40),hud_layout_vec2(0,0),false,0));
    hud_layout_canvas_add(&ctx,canvas,a,hud_layout_canvas_safe_slot(hud_layout_anchor(1,1,1,1),zero,hud_layout_vec2(1,1),true,0));
    hud_layout_arrange(&ctx);
    check(near(edge(ctx.widgets[a].arrangedRect),100) && near(bottom(ctx.widgets[a].arrangedRect),90),"nested safe anchor escaped parent");

    int cases=0;
    for (float aspect : {4.f/3,16.f/9,21.f/9,32.f/9,9.f/16}) {
        for (int mode=0; mode<4; ++mode) {
            int players=mode==0 ? 1 : mode==3 ? 4 : 2;
            for (int p=0;p<players;++p) for (float margin : {.25f,1.f,3.f}) {
                for (auto size : {hud_layout_vec2(64,96),hud_layout_vec2(64,160),hud_layout_vec2(300,32),hud_layout_vec2(500,900)}) {
                    auto view=hud_race_viewport(aspect,mode,p);
                    auto r=hud_race_layout(view,margin,size,true,true);
                    check(inside(r.column,view) && inside(r.laps,view) && inside(r.rpm,view) && inside(r.map,view),"viewport containment");
                    check(near(edge(r.laps),edge(r.column)) && near(edge(r.rpm),edge(r.column)),"common corner anchors");
                    check(near(r.laps.y,r.column.y) && near(bottom(r.rpm),bottom(r.column)),"top/bottom anchors");
                    check(bottom(r.laps) < r.map.y && bottom(r.map) < r.rpm.y,"map overlaps corner blocks");
                    check(near(r.map.w/size.x,r.map.h/size.y),"map aspect ratio");
                    check(near(r.map.y+r.map.h/2,r.mapSlot.y+r.mapSlot.h/2),"map slot center");
                    check(near(r.map.y+r.map.h/2,(bottom(r.laps)+r.rpm.y)/2),"map gap center");
                    float fitted=std::fmin(r.mapSlot.w/(size.x+12),r.mapSlot.h/(size.y+12));
                    check(near(r.mapScale,fitted),"map does not fill available gap");
                    check(near(edge(r.map)+6*r.mapScale,edge(r.column)),"map edge alignment");
                    ++cases;
                }
            }
        }
    }
    auto p1=hud_race_viewport(16.f/9,3,0),p4=hud_race_viewport(16.f/9,3,3);
    auto phone=hud_race_layout(hud_race_viewport(3088.f/1440,0,0),1,hud_layout_vec2(64,96),true,true);
    check(phone.map.h>88 && phone.map.h<89,"phone map size regression");
    for (bool laps : {false,true}) for (bool rpm : {false,true}) {
        auto r=hud_race_layout(hud_race_viewport(16.f/9,0,0),1,hud_layout_vec2(32,32),laps,rpm);
        check(near(r.mapScale,std::fmin(r.mapSlot.w/44,r.mapSlot.h/44)),"small map must upscale to fit");
        check(inside(r.map,r.column),"map with hidden corners escapes column");
    }
    check(near(p4.x,edge(p1)) && near(p4.y,bottom(p1)),"player viewport origins");
    std::cout << cases << " viewport/track/margin cases plus layout semantics passed\n";
}
