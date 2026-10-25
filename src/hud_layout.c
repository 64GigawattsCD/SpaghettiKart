#include "hud_layout.h"

#include <math.h>
#include <string.h>

static f32 hud_layout_maxf(f32 a, f32 b) {
    return (a > b) ? a : b;
}

static f32 hud_layout_minf(f32 a, f32 b) {
    return (a < b) ? a : b;
}

static f32 hud_layout_clampf(f32 value, f32 minValue, f32 maxValue) {
    if ((minValue >= 0.0f) && (value < minValue)) {
        value = minValue;
    }
    if ((maxValue >= 0.0f) && (value > maxValue)) {
        value = maxValue;
    }
    return value;
}

static HudRect hud_layout_inset_rect(HudRect rect, HudPadding padding) {
    rect.x += padding.left;
    rect.y += padding.top;
    rect.w -= padding.left + padding.right;
    rect.h -= padding.top + padding.bottom;
    if (rect.w < 0.0f) {
        rect.w = 0.0f;
    }
    if (rect.h < 0.0f) {
        rect.h = 0.0f;
    }
    return rect;
}

static HudPadding hud_layout_add_padding(HudPadding a, HudPadding b) {
    HudPadding result;

    result.left = a.left + b.left;
    result.top = a.top + b.top;
    result.right = a.right + b.right;
    result.bottom = a.bottom + b.bottom;
    return result;
}

static HudSingleChildSlot hud_layout_default_single_child_slot(void) {
    return hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                        hud_layout_vec2(0.5f, 0.5f), false, false);
}

static HudSingleChildSlot hud_layout_get_first_single_child_slot(HudLayoutContext* ctx, HudLayoutWidget* widget) {
    if ((widget->firstSlot >= 0) && (ctx->slots[widget->firstSlot].type == HUD_SLOT_SINGLE_CHILD)) {
        return ctx->slots[widget->firstSlot].data.singleChild;
    }
    return hud_layout_default_single_child_slot();
}

static HudLayoutWidget* hud_layout_get_widget(HudLayoutContext* ctx, HudWidgetId widgetId) {
    if ((widgetId < 0) || (widgetId >= ctx->widgetCount)) {
        return NULL;
    }
    return &ctx->widgets[widgetId];
}

static const HudLayoutWidget* hud_layout_get_widget_const(const HudLayoutContext* ctx, HudWidgetId widgetId) {
    if ((widgetId < 0) || (widgetId >= ctx->widgetCount)) {
        return NULL;
    }
    return &ctx->widgets[widgetId];
}

static HudWidgetId hud_layout_alloc_widget(HudLayoutContext* ctx, HudWidgetType type) {
    HudWidgetId id;
    HudLayoutWidget* widget;

    if (ctx->widgetCount >= HUD_LAYOUT_MAX_WIDGETS) {
        ctx->overflowed = true;
        return HUD_LAYOUT_INVALID_WIDGET;
    }

    id = ctx->widgetCount++;
    widget = &ctx->widgets[id];
    memset(widget, 0, sizeof(*widget));
    widget->type = type;
    widget->visible = true;
    widget->firstSlot = -1;
    widget->lastSlot = -1;
    widget->parent = HUD_LAYOUT_INVALID_WIDGET;
    return id;
}

static void hud_layout_add_slot(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudSlotType type,
                                const void* slotData) {
    HudLayoutWidget* parentWidget;
    HudLayoutSlot* slot;
    s32 slotIndex;

    if ((parent < 0) || (child < 0) || (ctx->slotCount >= HUD_LAYOUT_MAX_SLOTS)) {
        ctx->overflowed = true;
        return;
    }

    parentWidget = hud_layout_get_widget(ctx, parent);
    HudLayoutWidget* childWidget = hud_layout_get_widget(ctx, child);
    if (parentWidget == NULL || childWidget == NULL || child == ctx->rootId || childWidget->parent >= 0 ||
        (type == HUD_SLOT_CANVAS && parentWidget->type != HUD_WIDGET_CANVAS) ||
        (type == HUD_SLOT_BOX && parentWidget->type != HUD_WIDGET_HORIZONTAL_BOX && parentWidget->type != HUD_WIDGET_VERTICAL_BOX) ||
        (type == HUD_SLOT_GRID && parentWidget->type != HUD_WIDGET_GRID_BOX) ||
        (type == HUD_SLOT_SINGLE_CHILD &&
         ((parentWidget->type != HUD_WIDGET_SIZE_BOX && parentWidget->type != HUD_WIDGET_SCALE_BOX) || parentWidget->slotCount))) {
        ctx->overflowed = true;
        return;
    }
    for (HudWidgetId ancestor = parent; ancestor >= 0; ancestor = ctx->widgets[ancestor].parent) {
        if (ancestor == child) { ctx->overflowed = true; return; }
    }
    if (type == HUD_SLOT_GRID) {
        const HudGridSlot* grid = slotData;
        if (grid->row < 0 || grid->column < 0 || grid->row >= HUD_LAYOUT_GRID_MAX_TRACKS ||
            grid->column >= HUD_LAYOUT_GRID_MAX_TRACKS || grid->rowSpan < 1 || grid->columnSpan < 1 ||
            grid->rowSpan > HUD_LAYOUT_GRID_MAX_TRACKS - grid->row ||
            grid->columnSpan > HUD_LAYOUT_GRID_MAX_TRACKS - grid->column) {
            ctx->overflowed = true; return;
        }
    }
    childWidget->parent = parent;

    slotIndex = ctx->slotCount++;
    slot = &ctx->slots[slotIndex];
    memset(slot, 0, sizeof(*slot));
    slot->child = child;
    slot->nextSlot = -1;
    slot->type = type;

    switch (type) {
        case HUD_SLOT_CANVAS:
            slot->data.canvas = *(const HudCanvasSlot*) slotData;
            break;
        case HUD_SLOT_BOX:
            slot->data.box = *(const HudBoxSlot*) slotData;
            break;
        case HUD_SLOT_SINGLE_CHILD:
            slot->data.singleChild = *(const HudSingleChildSlot*) slotData;
            break;
        case HUD_SLOT_GRID:
            slot->data.grid = *(const HudGridSlot*) slotData;
            break;
    }

    if (parentWidget->lastSlot >= 0) {
        ctx->slots[parentWidget->lastSlot].nextSlot = slotIndex;
    } else {
        parentWidget->firstSlot = slotIndex;
    }
    parentWidget->lastSlot = slotIndex;
    parentWidget->slotCount++;
}

static HudVec2 hud_layout_measure_widget(HudLayoutContext* ctx, HudWidgetId widgetId);
static void hud_layout_arrange_widget(HudLayoutContext* ctx, HudWidgetId widgetId, HudRect rect);

static HudVec2 hud_layout_measure_canvas(HudLayoutContext* ctx, HudLayoutWidget* widget) {
    s32 slotIndex;
    HudVec2 desired = hud_layout_vec2(0.0f, 0.0f);

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudVec2 childDesired = hud_layout_measure_widget(ctx, slot->child);
        HudCanvasSlot* canvasSlot = &slot->data.canvas;
        f32 childW = canvasSlot->autoSize ? childDesired.x : canvasSlot->offsets.right;
        f32 childH = canvasSlot->autoSize ? childDesired.y : canvasSlot->offsets.bottom;

        desired.x = hud_layout_maxf(desired.x, canvasSlot->offsets.left + childW);
        desired.y = hud_layout_maxf(desired.y, canvasSlot->offsets.top + childH);
    }

    return desired;
}

static HudVec2 hud_layout_measure_box(HudLayoutContext* ctx, HudLayoutWidget* widget, bool horizontal) {
    s32 slotIndex;
    s32 childCount = 0;
    f32 major = 0.0f;
    f32 cross = 0.0f;
    HudVec2 desired;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudVec2 childDesired = hud_layout_measure_widget(ctx, slot->child);
        HudPadding padding = slot->data.box.padding;
        if (!ctx->widgets[slot->child].visible) continue;
        f32 childMajor;
        f32 childCross;

        childMajor = horizontal ? childDesired.x + padding.left + padding.right
                                : childDesired.y + padding.top + padding.bottom;
        childCross = horizontal ? childDesired.y + padding.top + padding.bottom
                                : childDesired.x + padding.left + padding.right;

        major += childMajor;
        cross = hud_layout_maxf(cross, childCross);
        childCount++;
    }

    if (childCount > 1) {
        major += widget->data.box.spacing * (f32) (childCount - 1);
    }

    if (horizontal) {
        desired.x = widget->data.box.padding.left + major + widget->data.box.padding.right;
        desired.y = widget->data.box.padding.top + cross + widget->data.box.padding.bottom;
    } else {
        desired.x = widget->data.box.padding.left + cross + widget->data.box.padding.right;
        desired.y = widget->data.box.padding.top + major + widget->data.box.padding.bottom;
    }

    return desired;
}

static HudVec2 hud_layout_measure_single_child(HudLayoutContext* ctx, HudLayoutWidget* widget, HudPadding padding) {
    s32 slotIndex = widget->firstSlot;
    HudVec2 childDesired = hud_layout_vec2(0.0f, 0.0f);
    HudSingleChildSlot slotData = hud_layout_get_first_single_child_slot(ctx, widget);

    if (slotIndex >= 0) {
        childDesired = hud_layout_measure_widget(ctx, ctx->slots[slotIndex].child);
    }

    padding = hud_layout_add_padding(padding, slotData.padding);
    childDesired.x += padding.left + padding.right;
    childDesired.y += padding.top + padding.bottom;
    return childDesired;
}

static HudVec2 hud_layout_measure_size_box(HudLayoutContext* ctx, HudLayoutWidget* widget) {
    HudVec2 desired = hud_layout_measure_single_child(ctx, widget, widget->data.sizeBox.padding);

    if (widget->data.sizeBox.widthOverride >= 0.0f) {
        desired.x = widget->data.sizeBox.widthOverride;
    }
    if (widget->data.sizeBox.heightOverride >= 0.0f) {
        desired.y = widget->data.sizeBox.heightOverride;
    }

    desired.x = hud_layout_clampf(desired.x, widget->data.sizeBox.minWidth, widget->data.sizeBox.maxWidth);
    desired.y = hud_layout_clampf(desired.y, widget->data.sizeBox.minHeight, widget->data.sizeBox.maxHeight);
    return desired;
}

static void hud_layout_grid_min_tracks(HudLayoutContext* ctx, HudLayoutWidget* widget, f32* columns, f32* rows) {
    memset(columns, 0, sizeof(f32) * HUD_LAYOUT_GRID_MAX_TRACKS);
    memset(rows, 0, sizeof(f32) * HUD_LAYOUT_GRID_MAX_TRACKS);
    // Single-cell requirements first; spanning children then distribute only their deficit.
    for (s32 pass = 0; pass < 2; ++pass) {
        for (s32 index = widget->firstSlot; index >= 0; index = ctx->slots[index].nextSlot) {
            HudLayoutSlot* slot = &ctx->slots[index];
            HudGridSlot* grid = &slot->data.grid;
            if (!ctx->widgets[slot->child].visible) continue;
            HudVec2 size = hud_layout_get_desired_size(ctx, slot->child);
            for (s32 axis = 0; axis < 2; ++axis) {
                s32 start = axis ? grid->row : grid->column;
                s32 span = axis ? grid->rowSpan : grid->columnSpan;
                f32* tracks = axis ? rows : columns;
                if ((span > 1) != (pass == 1)) continue;
                f32 desired = axis ? size.y + grid->padding.top + grid->padding.bottom :
                                     size.x + grid->padding.left + grid->padding.right;
                f32 allocated = 0;
                for (s32 i = start; i < start + span; ++i) allocated += tracks[i];
                f32 deficit = hud_layout_maxf(0, desired - allocated) / span;
                for (s32 i = start; i < start + span; ++i) tracks[i] += deficit;
            }
        }
    }
}

static HudVec2 hud_layout_measure_grid(HudLayoutContext* ctx, HudLayoutWidget* widget) {
    f32 columns[HUD_LAYOUT_GRID_MAX_TRACKS];
    f32 rows[HUD_LAYOUT_GRID_MAX_TRACKS];
    s32 slotIndex;
    s32 i;
    HudVec2 desired;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        hud_layout_measure_widget(ctx, ctx->slots[slotIndex].child);
    }
    hud_layout_grid_min_tracks(ctx, widget, columns, rows);

    desired = hud_layout_vec2(widget->data.grid.padding.left + widget->data.grid.padding.right,
                              widget->data.grid.padding.top + widget->data.grid.padding.bottom);
    for (i = 0; i < HUD_LAYOUT_GRID_MAX_TRACKS; i++) {
        desired.x += columns[i];
        desired.y += rows[i];
    }
    return desired;
}

static HudVec2 hud_layout_measure_widget(HudLayoutContext* ctx, HudWidgetId widgetId) {
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, widgetId);
    HudVec2 desired = hud_layout_vec2(0.0f, 0.0f);

    if (widget == NULL) {
        return desired;
    }
    if (!widget->visible) { widget->desiredSize = desired; return desired; }

    switch (widget->type) {
        case HUD_WIDGET_CANVAS:
            desired = hud_layout_measure_canvas(ctx, widget);
            break;
        case HUD_WIDGET_HORIZONTAL_BOX:
            desired = hud_layout_measure_box(ctx, widget, true);
            break;
        case HUD_WIDGET_VERTICAL_BOX:
            desired = hud_layout_measure_box(ctx, widget, false);
            break;
        case HUD_WIDGET_SCALE_BOX:
            desired = hud_layout_measure_single_child(ctx, widget, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));
            if (widget->data.scaleBox.stretch == HUD_SCALE_STRETCH_USER) {
                desired.x *= widget->data.scaleBox.userScale;
                desired.y *= widget->data.scaleBox.userScale;
            }
            break;
        case HUD_WIDGET_SIZE_BOX:
            desired = hud_layout_measure_size_box(ctx, widget);
            break;
        case HUD_WIDGET_GRID_BOX:
            desired = hud_layout_measure_grid(ctx, widget);
            break;
        case HUD_WIDGET_DRAW:
            desired = widget->desiredSize;
            break;
    }

    widget->desiredSize = desired;
    return desired;
}

static void hud_layout_arrange_canvas(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect rect) {
    s32 slotIndex;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudCanvasSlot* canvasSlot = &slot->data.canvas;
        HudVec2 childDesired = hud_layout_get_desired_size(ctx, slot->child);
        HudRect anchorRect = canvasSlot->useSafeZone ? ctx->safeRect : rect;
        if (canvasSlot->useSafeZone) {
            f32 right = hud_layout_minf(rect.x + rect.w, anchorRect.x + anchorRect.w);
            f32 bottom = hud_layout_minf(rect.y + rect.h, anchorRect.y + anchorRect.h);
            anchorRect.x = hud_layout_maxf(rect.x, anchorRect.x);
            anchorRect.y = hud_layout_maxf(rect.y, anchorRect.y);
            anchorRect.w = hud_layout_maxf(0, right - anchorRect.x);
            anchorRect.h = hud_layout_maxf(0, bottom - anchorRect.y);
        }
        f32 anchorLeft = anchorRect.x + anchorRect.w * canvasSlot->anchors.min.x;
        f32 anchorTop = anchorRect.y + anchorRect.h * canvasSlot->anchors.min.y;
        f32 anchorRight = anchorRect.x + anchorRect.w * canvasSlot->anchors.max.x;
        f32 anchorBottom = anchorRect.y + anchorRect.h * canvasSlot->anchors.max.y;
        bool stretchX = fabsf(canvasSlot->anchors.max.x - canvasSlot->anchors.min.x) > 0.0001f;
        bool stretchY = fabsf(canvasSlot->anchors.max.y - canvasSlot->anchors.min.y) > 0.0001f;
        HudRect childRect;

        if (stretchX) {
            childRect.x = anchorLeft + canvasSlot->offsets.left;
            childRect.w = (anchorRight - anchorLeft) - canvasSlot->offsets.left - canvasSlot->offsets.right;
        } else {
            childRect.w = canvasSlot->autoSize ? childDesired.x : canvasSlot->offsets.right;
            childRect.x = anchorLeft + canvasSlot->offsets.left - childRect.w * canvasSlot->alignment.x;
        }

        if (stretchY) {
            childRect.y = anchorTop + canvasSlot->offsets.top;
            childRect.h = (anchorBottom - anchorTop) - canvasSlot->offsets.top - canvasSlot->offsets.bottom;
        } else {
            childRect.h = canvasSlot->autoSize ? childDesired.y : canvasSlot->offsets.bottom;
            childRect.y = anchorTop + canvasSlot->offsets.top - childRect.h * canvasSlot->alignment.y;
        }

        if (canvasSlot->autoSize && stretchX) {
            childRect.w = childDesired.x;
            childRect.x -= childRect.w * canvasSlot->alignment.x;
        }
        if (canvasSlot->autoSize && stretchY) {
            childRect.h = childDesired.y;
            childRect.y -= childRect.h * canvasSlot->alignment.y;
        }

        if (canvasSlot->useAlignmentBounds) {
            f32 scaleX = (childDesired.x > 0.0f) ? (childRect.w / childDesired.x) : 1.0f;
            f32 scaleY = (childDesired.y > 0.0f) ? (childRect.h / childDesired.y) : 1.0f;
            f32 boundsX = canvasSlot->alignmentBounds.x * scaleX;
            f32 boundsY = canvasSlot->alignmentBounds.y * scaleY;
            f32 boundsW = canvasSlot->alignmentBounds.w * scaleX;
            f32 boundsH = canvasSlot->alignmentBounds.h * scaleY;

            if (!stretchX) {
                childRect.x = anchorLeft + canvasSlot->offsets.left -
                              (boundsX + (boundsW * canvasSlot->alignment.x));
            }
            if (!stretchY) {
                childRect.y = anchorTop + canvasSlot->offsets.top -
                              (boundsY + (boundsH * canvasSlot->alignment.y));
            }
        }

        if (childRect.w < 0.0f) {
            childRect.w = 0.0f;
        }
        if (childRect.h < 0.0f) {
            childRect.h = 0.0f;
        }

        hud_layout_arrange_widget(ctx, slot->child, childRect);
    }
}

static s32 hud_layout_count_box_children(HudLayoutContext* ctx, HudLayoutWidget* widget) {
    s32 slotIndex;
    s32 count = 0;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        if (ctx->widgets[ctx->slots[slotIndex].child].visible) count++;
    }
    return count;
}

static void hud_layout_arrange_box(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect rect, bool horizontal) {
    HudRect inner = hud_layout_inset_rect(rect, widget->data.box.padding);
    s32 childCount = hud_layout_count_box_children(ctx, widget);
    s32 slotIndex;
    f32 fixedMajor = 0.0f;
    f32 fillWeight = 0.0f;
    f32 availableMajor = horizontal ? inner.w : inner.h;
    f32 cursor;
    f32 remainingMajor;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudBoxSlot* boxSlot = &slot->data.box;
        HudVec2 childDesired = hud_layout_get_desired_size(ctx, slot->child);
        if (!ctx->widgets[slot->child].visible) continue;
        f32 childMajor = horizontal ? childDesired.x + boxSlot->padding.left + boxSlot->padding.right
                                    : childDesired.y + boxSlot->padding.top + boxSlot->padding.bottom;

        if (boxSlot->sizing == HUD_BOX_SLOT_FILL) {
            fillWeight += (boxSlot->fillWeight > 0.0f) ? boxSlot->fillWeight : 1.0f;
        } else {
            fixedMajor += childMajor;
        }
    }

    if (childCount > 1) {
        availableMajor -= widget->data.box.spacing * (f32) (childCount - 1);
    }
    remainingMajor = availableMajor - fixedMajor;
    if (remainingMajor < 0.0f) {
        remainingMajor = 0.0f;
    }

    cursor = horizontal ? inner.x : inner.y;
    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudBoxSlot* boxSlot = &slot->data.box;
        HudVec2 childDesired = hud_layout_get_desired_size(ctx, slot->child);
        if (!ctx->widgets[slot->child].visible) continue;
        f32 slotMajor;
        HudRect slotRect;
        HudRect paddedRect;
        HudRect childRect;

        if (boxSlot->sizing == HUD_BOX_SLOT_FILL) {
            f32 weight = (boxSlot->fillWeight > 0.0f) ? boxSlot->fillWeight : 1.0f;
            slotMajor = (fillWeight > 0.0f) ? remainingMajor * (weight / fillWeight) : 0.0f;
        } else {
            slotMajor = horizontal ? childDesired.x + boxSlot->padding.left + boxSlot->padding.right
                                   : childDesired.y + boxSlot->padding.top + boxSlot->padding.bottom;
        }

        if (horizontal) {
            slotRect = hud_layout_rect(cursor, inner.y, slotMajor, inner.h);
        } else {
            slotRect = hud_layout_rect(inner.x, cursor, inner.w, slotMajor);
        }

        paddedRect = hud_layout_inset_rect(slotRect, boxSlot->padding);
        if (horizontal) {
            childRect.w = (boxSlot->sizing == HUD_BOX_SLOT_FILL) ? paddedRect.w : childDesired.x;
            childRect.h = boxSlot->fillCrossAxis ? paddedRect.h : childDesired.y;
        } else {
            childRect.w = boxSlot->fillCrossAxis ? paddedRect.w : childDesired.x;
            childRect.h = (boxSlot->sizing == HUD_BOX_SLOT_FILL) ? paddedRect.h : childDesired.y;
        }

        childRect.w = hud_layout_minf(childRect.w, paddedRect.w);
        childRect.h = hud_layout_minf(childRect.h, paddedRect.h);
        childRect.x = paddedRect.x + (paddedRect.w - childRect.w) * boxSlot->alignment.x;
        childRect.y = paddedRect.y + (paddedRect.h - childRect.h) * boxSlot->alignment.y;
        hud_layout_arrange_widget(ctx, slot->child, childRect);

        cursor += slotMajor + widget->data.box.spacing;
    }
}

static void hud_layout_arrange_size_box(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect rect) {
    s32 slotIndex = widget->firstSlot;
    HudSingleChildSlot slotData = ((widget->firstSlot >= 0) && (ctx->slots[widget->firstSlot].type == HUD_SLOT_SINGLE_CHILD))
                                      ? ctx->slots[widget->firstSlot].data.singleChild
                                      : hud_layout_single_child_slot(hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f),
                                                                     widget->data.sizeBox.alignment, false, false);
    HudPadding padding = hud_layout_add_padding(widget->data.sizeBox.padding, slotData.padding);
    HudRect inner = hud_layout_inset_rect(rect, padding);
    HudVec2 childDesired;
    HudRect childRect;

    if (slotIndex < 0) {
        return;
    }

    childDesired = hud_layout_get_desired_size(ctx, ctx->slots[slotIndex].child);
    childRect.w = slotData.fillWidth ? inner.w
                                     : ((widget->data.sizeBox.widthOverride >= 0.0f) ? widget->data.sizeBox.widthOverride
                                                                                     : childDesired.x);
    childRect.h = slotData.fillHeight ? inner.h
                                      : ((widget->data.sizeBox.heightOverride >= 0.0f) ? widget->data.sizeBox.heightOverride
                                                                                       : childDesired.y);
    childRect.w = hud_layout_clampf(childRect.w, widget->data.sizeBox.minWidth, widget->data.sizeBox.maxWidth);
    childRect.h = hud_layout_clampf(childRect.h, widget->data.sizeBox.minHeight, widget->data.sizeBox.maxHeight);
    childRect.w = hud_layout_minf(childRect.w, inner.w);
    childRect.h = hud_layout_minf(childRect.h, inner.h);
    childRect.x = inner.x + (inner.w - childRect.w) * slotData.alignment.x;
    childRect.y = inner.y + (inner.h - childRect.h) * slotData.alignment.y;
    hud_layout_arrange_widget(ctx, ctx->slots[slotIndex].child, childRect);
}

static void hud_layout_transform_tree(HudLayoutContext* ctx, HudWidgetId id, HudRect target, HudVec2 scale) {
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);
    s32 slotIndex;
    if (widget == NULL) return;
    widget->arrangedRect.x = target.x + widget->arrangedRect.x * scale.x;
    widget->arrangedRect.y = target.y + widget->arrangedRect.y * scale.y;
    widget->arrangedRect.w *= scale.x;
    widget->arrangedRect.h *= scale.y;
    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        hud_layout_transform_tree(ctx, ctx->slots[slotIndex].child, target, scale);
    }
}

static void hud_layout_arrange_scale_box(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect rect) {
    s32 slotIndex = widget->firstSlot;
    HudSingleChildSlot slotData = hud_layout_get_first_single_child_slot(ctx, widget);
    HudRect inner = hud_layout_inset_rect(rect, slotData.padding);
    HudVec2 childDesired;
    HudRect childRect = inner;
    f32 scaleX = 1.0f;
    f32 scaleY = 1.0f;
    f32 scale;

    if (slotIndex < 0) {
        return;
    }

    childDesired = hud_layout_get_desired_size(ctx, ctx->slots[slotIndex].child);
    if ((childDesired.x <= 0.0f) || (childDesired.y <= 0.0f)) {
        hud_layout_arrange_widget(ctx, ctx->slots[slotIndex].child, inner);
        return;
    }

    scaleX = inner.w / childDesired.x;
    scaleY = inner.h / childDesired.y;
    switch (widget->data.scaleBox.stretch) {
        case HUD_SCALE_STRETCH_NONE:
            scaleX = 1.0f;
            scaleY = 1.0f;
            break;
        case HUD_SCALE_STRETCH_FILL:
            break;
        case HUD_SCALE_STRETCH_FIT:
            scale = hud_layout_minf(scaleX, scaleY);
            scaleX = scale;
            scaleY = scale;
            break;
        case HUD_SCALE_STRETCH_FILL_CROP:
            scale = hud_layout_maxf(scaleX, scaleY);
            scaleX = scale;
            scaleY = scale;
            break;
        case HUD_SCALE_STRETCH_USER:
            scaleX = widget->data.scaleBox.userScale;
            scaleY = widget->data.scaleBox.userScale;
            break;
    }

    if (widget->data.scaleBox.direction == HUD_SCALE_DIRECTION_DOWN_ONLY) {
        scaleX = hud_layout_minf(scaleX, 1.0f);
        scaleY = hud_layout_minf(scaleY, 1.0f);
    } else if (widget->data.scaleBox.direction == HUD_SCALE_DIRECTION_UP_ONLY) {
        scaleX = hud_layout_maxf(scaleX, 1.0f);
        scaleY = hud_layout_maxf(scaleY, 1.0f);
    }

    childRect.w = childDesired.x * scaleX;
    childRect.h = childDesired.y * scaleY;
    childRect.x = inner.x + (inner.w - childRect.w) * slotData.alignment.x;
    childRect.y = inner.y + (inner.h - childRect.h) * slotData.alignment.y;
    // Measure/arrange in the child's local units, then scale the complete subtree.
    // Passing the smaller rect to a box clips its Auto slots instead of scaling them.
    HudRect savedSafeRect = ctx->safeRect;
    ctx->safeRect = hud_layout_rect(scaleX > 0 ? (savedSafeRect.x - childRect.x) / scaleX : 0,
                                    scaleY > 0 ? (savedSafeRect.y - childRect.y) / scaleY : 0,
                                    scaleX > 0 ? savedSafeRect.w / scaleX : childDesired.x,
                                    scaleY > 0 ? savedSafeRect.h / scaleY : childDesired.y);
    hud_layout_arrange_widget(ctx, ctx->slots[slotIndex].child,
                              hud_layout_rect(0.0f, 0.0f, childDesired.x, childDesired.y));
    ctx->safeRect = savedSafeRect;
    hud_layout_transform_tree(ctx, ctx->slots[slotIndex].child, childRect, hud_layout_vec2(scaleX, scaleY));
}

static void hud_layout_compute_grid_tracks(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect inner, f32* columns,
                                           f32* rows) {
    s32 i;
    f32 desiredWidth = 0.0f;
    f32 desiredHeight = 0.0f;
    f32 columnFill = 0.0f;
    f32 rowFill = 0.0f;
    f32 extraWidth;
    f32 extraHeight;

    hud_layout_grid_min_tracks(ctx, widget, columns, rows);

    for (i = 0; i < HUD_LAYOUT_GRID_MAX_TRACKS; i++) {
        desiredWidth += columns[i];
        desiredHeight += rows[i];
        columnFill += widget->data.grid.columnFill[i];
        rowFill += widget->data.grid.rowFill[i];
    }

    extraWidth = inner.w - desiredWidth;
    extraHeight = inner.h - desiredHeight;
    if (extraWidth < 0.0f) {
        extraWidth = 0.0f;
    }
    if (extraHeight < 0.0f) {
        extraHeight = 0.0f;
    }

    for (i = 0; i < HUD_LAYOUT_GRID_MAX_TRACKS; i++) {
        if ((columnFill > 0.0f) && (widget->data.grid.columnFill[i] > 0.0f)) {
            columns[i] += extraWidth * (widget->data.grid.columnFill[i] / columnFill);
        }
        if ((rowFill > 0.0f) && (widget->data.grid.rowFill[i] > 0.0f)) {
            rows[i] += extraHeight * (widget->data.grid.rowFill[i] / rowFill);
        }
    }
}

static void hud_layout_arrange_grid(HudLayoutContext* ctx, HudLayoutWidget* widget, HudRect rect) {
    f32 columns[HUD_LAYOUT_GRID_MAX_TRACKS];
    f32 rows[HUD_LAYOUT_GRID_MAX_TRACKS];
    HudRect inner = hud_layout_inset_rect(rect, widget->data.grid.padding);
    s32 slotIndex;

    hud_layout_compute_grid_tracks(ctx, widget, inner, columns, rows);

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        HudLayoutSlot* slot = &ctx->slots[slotIndex];
        HudGridSlot* gridSlot = &slot->data.grid;
        HudVec2 childDesired = hud_layout_get_desired_size(ctx, slot->child);
        HudRect cellRect;
        HudRect paddedRect;
        HudRect childRect;
        s32 columnSpan = (gridSlot->columnSpan > 0) ? gridSlot->columnSpan : 1;
        s32 rowSpan = (gridSlot->rowSpan > 0) ? gridSlot->rowSpan : 1;
        s32 i;

        cellRect = hud_layout_rect(inner.x, inner.y, 0.0f, 0.0f);
        for (i = 0; (i < gridSlot->column) && (i < HUD_LAYOUT_GRID_MAX_TRACKS); i++) {
            cellRect.x += columns[i];
        }
        for (i = 0; (i < gridSlot->row) && (i < HUD_LAYOUT_GRID_MAX_TRACKS); i++) {
            cellRect.y += rows[i];
        }
        for (i = gridSlot->column; (i < gridSlot->column + columnSpan) && (i < HUD_LAYOUT_GRID_MAX_TRACKS); i++) {
            cellRect.w += columns[i];
        }
        for (i = gridSlot->row; (i < gridSlot->row + rowSpan) && (i < HUD_LAYOUT_GRID_MAX_TRACKS); i++) {
            cellRect.h += rows[i];
        }

        cellRect.x += gridSlot->nudge.x;
        cellRect.y += gridSlot->nudge.y;
        paddedRect = hud_layout_inset_rect(cellRect, gridSlot->padding);
        childRect.w = gridSlot->fillWidth ? paddedRect.w : hud_layout_minf(childDesired.x, paddedRect.w);
        childRect.h = gridSlot->fillHeight ? paddedRect.h : hud_layout_minf(childDesired.y, paddedRect.h);
        childRect.x = paddedRect.x + (paddedRect.w - childRect.w) * gridSlot->alignment.x;
        childRect.y = paddedRect.y + (paddedRect.h - childRect.h) * gridSlot->alignment.y;
        hud_layout_arrange_widget(ctx, slot->child, childRect);
    }
}

static void hud_layout_arrange_widget(HudLayoutContext* ctx, HudWidgetId widgetId, HudRect rect) {
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, widgetId);

    if ((widget == NULL) || !widget->visible) {
        return;
    }

    widget->arrangedRect = rect;
    switch (widget->type) {
        case HUD_WIDGET_CANVAS:
            hud_layout_arrange_canvas(ctx, widget, rect);
            break;
        case HUD_WIDGET_HORIZONTAL_BOX:
            hud_layout_arrange_box(ctx, widget, rect, true);
            break;
        case HUD_WIDGET_VERTICAL_BOX:
            hud_layout_arrange_box(ctx, widget, rect, false);
            break;
        case HUD_WIDGET_SCALE_BOX:
            hud_layout_arrange_scale_box(ctx, widget, rect);
            break;
        case HUD_WIDGET_SIZE_BOX:
            hud_layout_arrange_size_box(ctx, widget, rect);
            break;
        case HUD_WIDGET_GRID_BOX:
            hud_layout_arrange_grid(ctx, widget, rect);
            break;
        case HUD_WIDGET_DRAW:
            break;
    }
}

static void hud_layout_draw_widget(const HudLayoutContext* ctx, HudWidgetId widgetId);

static void hud_layout_draw_canvas_children(const HudLayoutContext* ctx, const HudLayoutWidget* widget) {
    s32 orderedSlots[HUD_LAYOUT_MAX_SLOTS];
    s32 count = 0;
    s32 slotIndex;
    s32 i;

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        if (count < HUD_LAYOUT_MAX_SLOTS) {
            orderedSlots[count++] = slotIndex;
        }
    }

    for (i = 1; i < count; i++) {
        s32 current = orderedSlots[i];
        s32 currentZ = widget->type == HUD_WIDGET_GRID_BOX ? ctx->slots[current].data.grid.layer :
                                                           ctx->slots[current].data.canvas.zOrder;
        s32 j = i - 1;

        while (j >= 0) {
            s32 previousZ = widget->type == HUD_WIDGET_GRID_BOX ? ctx->slots[orderedSlots[j]].data.grid.layer :
                                                                 ctx->slots[orderedSlots[j]].data.canvas.zOrder;
            if (previousZ <= currentZ) break;
            orderedSlots[j + 1] = orderedSlots[j];
            j--;
        }
        orderedSlots[j + 1] = current;
    }

    for (i = 0; i < count; i++) {
        hud_layout_draw_widget(ctx, ctx->slots[orderedSlots[i]].child);
    }
}

static void hud_layout_draw_widget(const HudLayoutContext* ctx, HudWidgetId widgetId) {
    const HudLayoutWidget* widget = hud_layout_get_widget_const(ctx, widgetId);
    s32 slotIndex;

    if ((widget == NULL) || !widget->visible) {
        return;
    }

    if (widget->type == HUD_WIDGET_DRAW) {
        if (widget->data.draw.draw != NULL) {
            widget->data.draw.draw(ctx, widgetId, widget->arrangedRect, widget->data.draw.userData);
        }
        return;
    }

    if (widget->type == HUD_WIDGET_CANVAS || widget->type == HUD_WIDGET_GRID_BOX) {
        hud_layout_draw_canvas_children(ctx, widget);
        return;
    }

    for (slotIndex = widget->firstSlot; slotIndex >= 0; slotIndex = ctx->slots[slotIndex].nextSlot) {
        hud_layout_draw_widget(ctx, ctx->slots[slotIndex].child);
    }
}

void hud_layout_begin(HudLayoutContext* ctx, HudRect rootRect) {
    hud_layout_begin_safe_zone(ctx, rootRect, hud_layout_padding(0.0f, 0.0f, 0.0f, 0.0f));
}

void hud_layout_begin_safe_zone(HudLayoutContext* ctx, HudRect rootRect, HudPadding safeZonePadding) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->rootRect = rootRect;
    ctx->safeRect = hud_layout_inset_rect(rootRect, safeZonePadding);
    ctx->rootId = hud_layout_canvas(ctx);
    if (ctx->rootId >= 0) {
        ctx->widgets[ctx->rootId].arrangedRect = rootRect;
    }
}

HudWidgetId hud_layout_root(const HudLayoutContext* ctx) {
    return ctx->rootId;
}

HudWidgetId hud_layout_canvas(HudLayoutContext* ctx) {
    return hud_layout_alloc_widget(ctx, HUD_WIDGET_CANVAS);
}

HudWidgetId hud_layout_horizontal_box(HudLayoutContext* ctx, f32 spacing, HudPadding padding) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_HORIZONTAL_BOX);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->data.box.spacing = spacing;
        widget->data.box.padding = padding;
    }
    return id;
}

HudWidgetId hud_layout_vertical_box(HudLayoutContext* ctx, f32 spacing, HudPadding padding) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_VERTICAL_BOX);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->data.box.spacing = spacing;
        widget->data.box.padding = padding;
    }
    return id;
}

HudWidgetId hud_layout_scale_box(HudLayoutContext* ctx, HudScaleStretch stretch, HudScaleDirection direction,
                                 f32 userScale) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_SCALE_BOX);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->data.scaleBox.stretch = stretch;
        widget->data.scaleBox.direction = direction;
        widget->data.scaleBox.userScale = userScale;
    }
    return id;
}

HudWidgetId hud_layout_size_box(HudLayoutContext* ctx, f32 widthOverride, f32 heightOverride, f32 minWidth,
                                f32 minHeight, f32 maxWidth, f32 maxHeight, HudPadding padding,
                                HudVec2 alignment) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_SIZE_BOX);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->data.sizeBox.widthOverride = widthOverride;
        widget->data.sizeBox.heightOverride = heightOverride;
        widget->data.sizeBox.minWidth = minWidth;
        widget->data.sizeBox.minHeight = minHeight;
        widget->data.sizeBox.maxWidth = maxWidth;
        widget->data.sizeBox.maxHeight = maxHeight;
        widget->data.sizeBox.padding = padding;
        widget->data.sizeBox.alignment = alignment;
    }
    return id;
}

HudWidgetId hud_layout_grid_box(HudLayoutContext* ctx, HudPadding padding) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_GRID_BOX);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->data.grid.padding = padding;
    }
    return id;
}

HudWidgetId hud_layout_draw(HudLayoutContext* ctx, HudVec2 desiredSize, HudLayoutDrawFunc draw, void* userData) {
    HudWidgetId id = hud_layout_alloc_widget(ctx, HUD_WIDGET_DRAW);
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, id);

    if (widget != NULL) {
        widget->desiredSize = desiredSize;
        widget->data.draw.draw = draw;
        widget->data.draw.userData = userData;
    }
    return id;
}

void hud_layout_canvas_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudCanvasSlot slot) {
    hud_layout_add_slot(ctx, parent, child, HUD_SLOT_CANVAS, &slot);
}

void hud_layout_box_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudBoxSlot slot) {
    hud_layout_add_slot(ctx, parent, child, HUD_SLOT_BOX, &slot);
}

void hud_layout_single_child_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child,
                                 HudSingleChildSlot slot) {
    hud_layout_add_slot(ctx, parent, child, HUD_SLOT_SINGLE_CHILD, &slot);
}

void hud_layout_grid_add(HudLayoutContext* ctx, HudWidgetId parent, HudWidgetId child, HudGridSlot slot) {
    hud_layout_add_slot(ctx, parent, child, HUD_SLOT_GRID, &slot);
}

void hud_layout_grid_set_column_fill(HudLayoutContext* ctx, HudWidgetId grid, s32 column, f32 fillWeight) {
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, grid);

    if ((widget != NULL) && (widget->type == HUD_WIDGET_GRID_BOX) && (column >= 0) &&
        (column < HUD_LAYOUT_GRID_MAX_TRACKS)) {
        widget->data.grid.columnFill[column] = fillWeight;
    }
}

void hud_layout_grid_set_row_fill(HudLayoutContext* ctx, HudWidgetId grid, s32 row, f32 fillWeight) {
    HudLayoutWidget* widget = hud_layout_get_widget(ctx, grid);

    if ((widget != NULL) && (widget->type == HUD_WIDGET_GRID_BOX) && (row >= 0) &&
        (row < HUD_LAYOUT_GRID_MAX_TRACKS)) {
        widget->data.grid.rowFill[row] = fillWeight;
    }
}

void hud_layout_arrange(HudLayoutContext* ctx) {
    if (ctx->rootId < 0) {
        return;
    }
    for (s32 i = 0; i < ctx->widgetCount; ++i) ctx->widgets[i].arrangedRect = hud_layout_rect(0, 0, 0, 0);
    hud_layout_measure_widget(ctx, ctx->rootId);
    ctx->widgets[ctx->rootId].desiredSize = hud_layout_vec2(ctx->rootRect.w, ctx->rootRect.h);
    hud_layout_arrange_widget(ctx, ctx->rootId, ctx->rootRect);
}

void hud_layout_draw_tree(const HudLayoutContext* ctx) {
    if (ctx->rootId >= 0) {
        hud_layout_draw_widget(ctx, ctx->rootId);
    }
}

HudRect hud_layout_get_rect(const HudLayoutContext* ctx, HudWidgetId widgetId) {
    const HudLayoutWidget* widget = hud_layout_get_widget_const(ctx, widgetId);

    if (widget == NULL) {
        return hud_layout_rect(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return widget->arrangedRect;
}

HudVec2 hud_layout_get_desired_size(const HudLayoutContext* ctx, HudWidgetId widgetId) {
    const HudLayoutWidget* widget = hud_layout_get_widget_const(ctx, widgetId);

    if (widget == NULL) {
        return hud_layout_vec2(0.0f, 0.0f);
    }
    return widget->desiredSize;
}

HudCanvasSlot hud_layout_canvas_slot(HudAnchor anchors, HudPadding offsets, HudVec2 alignment, bool autoSize,
                                     s32 zOrder) {
    HudCanvasSlot slot;

    slot.anchors = anchors;
    slot.offsets = offsets;
    slot.alignment = alignment;
    slot.alignmentBounds = hud_layout_rect(0.0f, 0.0f, 0.0f, 0.0f);
    slot.useAlignmentBounds = false;
    slot.useSafeZone = false;
    slot.autoSize = autoSize;
    slot.zOrder = zOrder;
    return slot;
}

HudCanvasSlot hud_layout_canvas_bounds_slot(HudAnchor anchors, HudPadding offsets, HudRect alignmentBounds,
                                            HudVec2 alignment, bool autoSize, s32 zOrder) {
    HudCanvasSlot slot = hud_layout_canvas_slot(anchors, offsets, alignment, autoSize, zOrder);

    slot.alignmentBounds = alignmentBounds;
    slot.useAlignmentBounds = true;
    return slot;
}

HudCanvasSlot hud_layout_canvas_safe_slot(HudAnchor anchors, HudPadding offsets, HudVec2 alignment, bool autoSize,
                                          s32 zOrder) {
    HudCanvasSlot slot = hud_layout_canvas_slot(anchors, offsets, alignment, autoSize, zOrder);

    slot.useSafeZone = true;
    return slot;
}

HudCanvasSlot hud_layout_canvas_safe_bounds_slot(HudAnchor anchors, HudPadding offsets, HudRect alignmentBounds,
                                                 HudVec2 alignment, bool autoSize, s32 zOrder) {
    HudCanvasSlot slot = hud_layout_canvas_bounds_slot(anchors, offsets, alignmentBounds, alignment, autoSize, zOrder);

    slot.useSafeZone = true;
    return slot;
}

HudBoxSlot hud_layout_auto_slot(HudPadding padding, HudVec2 alignment, bool fillCrossAxis) {
    HudBoxSlot slot;

    slot.sizing = HUD_BOX_SLOT_AUTO;
    slot.fillWeight = 0.0f;
    slot.padding = padding;
    slot.alignment = alignment;
    slot.fillCrossAxis = fillCrossAxis;
    return slot;
}

HudBoxSlot hud_layout_fill_slot(f32 fillWeight, HudPadding padding, HudVec2 alignment, bool fillCrossAxis) {
    HudBoxSlot slot;

    slot.sizing = HUD_BOX_SLOT_FILL;
    slot.fillWeight = fillWeight;
    slot.padding = padding;
    slot.alignment = alignment;
    slot.fillCrossAxis = fillCrossAxis;
    return slot;
}

HudSingleChildSlot hud_layout_single_child_slot(HudPadding padding, HudVec2 alignment, bool fillWidth,
                                                bool fillHeight) {
    HudSingleChildSlot slot;

    slot.padding = padding;
    slot.alignment = alignment;
    slot.fillWidth = fillWidth;
    slot.fillHeight = fillHeight;
    return slot;
}

HudGridSlot hud_layout_grid_slot(s32 row, s32 column, s32 rowSpan, s32 columnSpan, s32 layer, HudVec2 nudge,
                                 HudPadding padding, HudVec2 alignment, bool fillWidth, bool fillHeight) {
    HudGridSlot slot;

    slot.row = row;
    slot.column = column;
    slot.rowSpan = rowSpan;
    slot.columnSpan = columnSpan;
    slot.layer = layer;
    slot.nudge = nudge;
    slot.padding = padding;
    slot.alignment = alignment;
    slot.fillWidth = fillWidth;
    slot.fillHeight = fillHeight;
    return slot;
}
