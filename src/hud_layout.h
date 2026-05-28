#ifndef HUD_LAYOUT_H
#define HUD_LAYOUT_H

#include <stdbool.h>
#include <libultra/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUD_LAYOUT_INVALID_WIDGET (-1)
#define HUD_LAYOUT_MAX_WIDGETS 96
#define HUD_LAYOUT_MAX_SLOTS 160
#define HUD_LAYOUT_GRID_MAX_TRACKS 12

typedef s32 HudWidgetId;

typedef struct HudVec2 {
    f32 x;
    f32 y;
} HudVec2;

typedef struct HudRect {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
} HudRect;

typedef struct HudPadding {
    f32 left;
    f32 top;
    f32 right;
    f32 bottom;
} HudPadding;

typedef struct HudAnchor {
    HudVec2 min;
    HudVec2 max;
} HudAnchor;

typedef enum HudWidgetType {
    HUD_WIDGET_CANVAS,
    HUD_WIDGET_HORIZONTAL_BOX,
    HUD_WIDGET_VERTICAL_BOX,
    HUD_WIDGET_SCALE_BOX,
    HUD_WIDGET_SIZE_BOX,
    HUD_WIDGET_GRID_BOX,
    HUD_WIDGET_DRAW
} HudWidgetType;

typedef enum HudSlotType {
    HUD_SLOT_CANVAS,
    HUD_SLOT_BOX,
    HUD_SLOT_SINGLE_CHILD,
    HUD_SLOT_GRID
} HudSlotType;

typedef enum HudBoxSlotSizing {
    HUD_BOX_SLOT_AUTO,
    HUD_BOX_SLOT_FILL
} HudBoxSlotSizing;

typedef enum HudScaleStretch {
    HUD_SCALE_STRETCH_NONE,
    HUD_SCALE_STRETCH_FILL,
    HUD_SCALE_STRETCH_FIT,
    HUD_SCALE_STRETCH_FILL_CROP,
    HUD_SCALE_STRETCH_USER
} HudScaleStretch;

typedef enum HudScaleDirection {
    HUD_SCALE_DIRECTION_BOTH,
    HUD_SCALE_DIRECTION_DOWN_ONLY,
    HUD_SCALE_DIRECTION_UP_ONLY
} HudScaleDirection;

struct HudLayoutContext;

typedef void (*HudLayoutDrawFunc)(const struct HudLayoutContext* ctx, HudWidgetId widgetId, HudRect rect,
                                  void* userData);

typedef struct HudCanvasSlot {
    HudAnchor anchors;
    HudPadding offsets;
    HudVec2 alignment;
    HudRect alignmentBounds;
    bool useAlignmentBounds;
    bool autoSize;
    s32 zOrder;
} HudCanvasSlot;

typedef struct HudBoxSlot {
    HudBoxSlotSizing sizing;
    f32 fillWeight;
    HudPadding padding;
    HudVec2 alignment;
    bool fillCrossAxis;
} HudBoxSlot;

typedef struct HudSingleChildSlot {
    HudPadding padding;
    HudVec2 alignment;
    bool fillWidth;
    bool fillHeight;
} HudSingleChildSlot;

typedef struct HudGridSlot {
    s32 row;
    s32 column;
    s32 rowSpan;
    s32 columnSpan;
    s32 layer;
    HudVec2 nudge;
    HudPadding padding;
    HudVec2 alignment;
    bool fillWidth;
    bool fillHeight;
} HudGridSlot;

typedef struct HudLayoutSlot {
    HudWidgetId child;
    s32 nextSlot;
    HudSlotType type;
    union {
        HudCanvasSlot canvas;
        HudBoxSlot box;
        HudSingleChildSlot singleChild;
        HudGridSlot grid;
    } data;
} HudLayoutSlot;

typedef struct HudLayoutWidget {
    HudWidgetType type;
    bool visible;
    HudVec2 desiredSize;
    HudRect arrangedRect;
    s32 firstSlot;
    s32 lastSlot;
    s32 slotCount;
    union {
        struct {
            f32 spacing;
            HudPadding padding;
        } box;
        struct {
            HudScaleStretch stretch;
            HudScaleDirection direction;
            f32 userScale;
        } scaleBox;
        struct {
            f32 widthOverride;
            f32 heightOverride;
            f32 minWidth;
            f32 minHeight;
            f32 maxWidth;
            f32 maxHeight;
            HudPadding padding;
            HudVec2 alignment;
        } sizeBox;
        struct {
            f32 columnFill[HUD_LAYOUT_GRID_MAX_TRACKS];
            f32 rowFill[HUD_LAYOUT_GRID_MAX_TRACKS];
            HudPadding padding;
        } grid;
        struct {
            HudLayoutDrawFunc draw;
            void* userData;
        } draw;
    } data;
} HudLayoutWidget;

typedef struct HudLayoutContext {
    HudRect rootRect;
    HudWidgetId rootId;
    s32 widgetCount;
    s32 slotCount;
    bool overflowed;
    HudLayoutWidget widgets[HUD_LAYOUT_MAX_WIDGETS];
    HudLayoutSlot slots[HUD_LAYOUT_MAX_SLOTS];
} HudLayoutContext;

static inline HudVec2 hud_layout_vec2(f32 x, f32 y) {
    HudVec2 result = { x, y };
    return result;
}

static inline HudRect hud_layout_rect(f32 x, f32 y, f32 w, f32 h) {
    HudRect result = { x, y, w, h };
    return result;
}

static inline HudPadding hud_layout_padding(f32 left, f32 top, f32 right, f32 bottom) {
    HudPadding result = { left, top, right, bottom };
    return result;
}

static inline HudAnchor hud_layout_anchor(f32 minX, f32 minY, f32 maxX, f32 maxY) {
    HudAnchor result = { { minX, minY }, { maxX, maxY } };
    return result;
}

void hud_layout_begin(HudLayoutContext* ctx, HudRect rootRect);
HudWidgetId hud_layout_root(const HudLayoutContext* ctx);

HudWidgetId hud_layout_canvas(HudLayoutContext* ctx);
HudWidgetId hud_layout_horizontal_box(HudLayoutContext* ctx, f32 spacing, HudPadding padding);
HudWidgetId hud_layout_vertical_box(HudLayoutContext* ctx, f32 spacing, HudPadding padding);
HudWidgetId hud_layout_scale_box(HudLayoutContext* ctx, HudScaleStretch stretch, HudScaleDirection direction,
                                 f32 userScale);
HudWidgetId hud_layout_size_box(HudLayoutContext* ctx, f32 widthOverride, f32 heightOverride, f32 minWidth,
                                f32 minHeight, f32 maxWidth, f32 maxHeight, HudPadding padding,
                                HudVec2 alignment);
HudWidgetId hud_layout_grid_box(HudLayoutContext* ctx, HudPadding padding);
HudWidgetId hud_layout_draw(HudLayoutContext* ctx, HudVec2 desiredSize, HudLayoutDrawFunc draw, void* userData);

void hud_layout_canvas_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudCanvasSlot slot);
void hud_layout_box_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudBoxSlot slot);
void hud_layout_single_child_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child,
                                 HudSingleChildSlot slot);
void hud_layout_grid_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudGridSlot slot);
void hud_layout_grid_set_column_fill(HudLayoutContext* ctx, HudWidgetId grid, s32 column, f32 fillWeight);
void hud_layout_grid_set_row_fill(HudLayoutContext* ctx, HudWidgetId grid, s32 row, f32 fillWeight);

void hud_layout_arrange(HudLayoutContext* ctx);
void hud_layout_draw_tree(const HudLayoutContext* ctx);
HudRect hud_layout_get_rect(const HudLayoutContext* ctx, HudWidgetId widgetId);
HudVec2 hud_layout_get_desired_size(const HudLayoutContext* ctx, HudWidgetId widgetId);

HudCanvasSlot hud_layout_canvas_slot(HudAnchor anchors, HudPadding offsets, HudVec2 alignment, bool autoSize,
                                     s32 zOrder);
HudCanvasSlot hud_layout_canvas_bounds_slot(HudAnchor anchors, HudPadding offsets, HudRect alignmentBounds,
                                            HudVec2 alignment, bool autoSize, s32 zOrder);
HudBoxSlot hud_layout_auto_slot(HudPadding padding, HudVec2 alignment, bool fillCrossAxis);
HudBoxSlot hud_layout_fill_slot(f32 fillWeight, HudPadding padding, HudVec2 alignment, bool fillCrossAxis);
HudSingleChildSlot hud_layout_single_child_slot(HudPadding padding, HudVec2 alignment, bool fillWidth,
                                                bool fillHeight);
HudGridSlot hud_layout_grid_slot(s32 row, s32 column, s32 rowSpan, s32 columnSpan, s32 layer, HudVec2 nudge,
                                 HudPadding padding, HudVec2 alignment, bool fillWidth, bool fillHeight);

#ifdef __cplusplus
}
#endif

#endif
