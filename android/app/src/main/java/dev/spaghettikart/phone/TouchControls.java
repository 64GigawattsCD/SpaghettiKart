package dev.spaghettikart.phone;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.LinearGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.os.SystemClock;
import java.io.File;
import java.io.FileWriter;
import java.io.FileOutputStream;
import org.json.JSONObject;
import java.util.HashMap;
import java.util.Map;
import java.util.HashSet;
import java.util.Set;

final class TouchControls extends View {
    // Low two bits match AndroidPhoneTouchMode; upper bits identify the input epoch.
    static final int MODE_GAMEPLAY = 1, MODE_MASK = 3;
    private int modeToken = -1;
    private boolean hostActive, gameplayActive, gestureActive;
    private int drawCount;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Map<Integer, Integer> pointers = new HashMap<>();
    private final Set<Integer> radialPointers = new HashSet<>();
    private final Set<Integer> gasPointers = new HashSet<>();
    private final Map<Integer, Integer> radialActions = new HashMap<>();
    private float pedalX, pedalY;
    private static final float PEDAL_RADIUS = .66f, RING_INNER = .84f, RING_OUTER = 1.65f;
    private static final int[] RING_ACTIONS = {7, 2, 1, 3};
    private final Path ringSegment = new Path();
    private final RectF ringBounds = new RectF();
    private int stickId = -1;
    private float sx, sy;
    private float unit, cx, cy;
    private int lookId = -1;
    private float lookX, lookY, lookCx, lookCy, lookRadius;
    private int layoutMode;
    private boolean manual;
    private final ShifterGate gate = new ShifterGate();
    private int shifterId = -1, shiftButton;
    private float gx, gy, gateUnit, grabX, grabY;
    private final Runnable layoutPoll = new Runnable() {
        @Override public void run() {
            boolean wasActive = gameplayActive;
            syncTouchMode();
            int next = MainActivity.touchLayout();
            int transmission = MainActivity.touchTransmission();
            boolean nextManual = (transmission & 0x100) != 0;
            boolean layoutChanged = next != layoutMode || nextManual != manual;
            if (layoutChanged) {
                layoutMode = next;
                manual = nextManual;
                gate.select((transmission & 255)-1);
                onSizeChanged(getWidth(), getHeight(), getWidth(), getHeight());
                invalidate();
            }
            if (gameplayActive && manual && shifterId == -1) {
                int actual = (transmission & 255)-1;
                if (shiftButton != 0 && actual == gate.gear()) { shiftButton=0; publish(); }
                if (shiftButton == 0) { gate.select(actual); invalidate(); }
            }
            MainActivity activity = (MainActivity)getContext();
            if (gameplayActive && (!wasActive || layoutChanged) && activity.getIntent().getBooleanExtra("kart_test", false)
                && !activity.getIntent().getBooleanExtra("touch_dispatch_test", false))
                selfTest(activity.getExternalFilesDir(null));
            postDelayed(this, 16);
        }
    };

    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); post(layoutPoll); }
    @Override protected void onDetachedFromWindow() { removeCallbacks(layoutPoll); setHostActive(false); super.onDetachedFromWindow(); }

    void setHostActive(boolean active) {
        hostActive = active;
        syncTouchMode();
    }

    boolean syncTouchMode() {
        int next = MainActivity.touchMode();
        boolean active = hostActive && (next & MODE_MASK) == MODE_GAMEPLAY;
        if (next != modeToken || active != gameplayActive) {
            modeToken = next;
            gameplayActive = active;
            release();
            setTooltipText(null);
            setVisibility(active ? VISIBLE : INVISIBLE);
            // INVISIBLE/VISIBLE does not relayout. Refresh the SurfaceView hole so
            // SurfaceFlinger does not keep discarding this now-visible overlay.
            if (getParent() != null) getParent().requestTransparentRegion(this);
            android.util.Log.i("KartTouch", "overlay active="+active+" host="+hostActive+" token="+next+
                " size="+getWidth()+"x"+getHeight()+" shown="+isShown());
        }
        return gameplayActive;
    }

    JSONObject captureDiagnostics(File root, String name) throws Exception {
        JSONObject status = new JSONObject().put("host_active", hostActive).put("gameplay_active", gameplayActive)
            .put("mode_token", modeToken).put("visibility", getVisibility()).put("shown", isShown())
            .put("width", getWidth()).put("height", getHeight()).put("draw_count", drawCount);
        if (getWidth() > 0 && getHeight() > 0) {
            Bitmap bitmap = Bitmap.createBitmap(getWidth(), getHeight(), Bitmap.Config.ARGB_8888);
            draw(new Canvas(bitmap));
            try (FileOutputStream out = new FileOutputStream(new File(root, name))) {
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
            } finally { bitmap.recycle(); }
            status.put("capture", name);
        }
        return status;
    }
    private final float[][] buttons = new float[8][3];
    private final String[] actions = { "Accelerate", "Brake", "Drift", "Use Item", "Pause", "Switch View", "Look Behind", "Jump" };
    private final Drawable[] icons = new Drawable[8];
    private final Drawable[] iconShadows = new Drawable[8];
    private final Bitmap[] rims = new Bitmap[8];
    private final Drawable lookIcon;
    private final Bitmap shiftKnob, steeringWheel;
    private final RectF spriteRect = new RectF();
    private final Path gateTrack = new Path();
    private Shader gearIdle, gearSelected;
    private static final int[] TRACK_COLORS = {0xD0080B13,0xC0A3B6D2,0xC0172132};
    private static final float[] TRACK_WIDTHS = {.66f,.6f,.48f};
    private final int[] accents = { 0xFF9CF541, 0xFFFF6559, 0xFF62D8FF,
        0xFFFFDE49, 0xFFD5A3FF, 0xFFE1EDFF, 0xFF62D8FF, 0xFFD5A3FF };
    private final int[] masks = { 0x8040, 0x4080, 0x10, 0x2000, 0x1000, 0x1, 0x2, 0x20 };

    TouchControls(Context context) {
        super(context);
        setFocusable(false);
        setVisibility(INVISIBLE);
        setContentDescription("Kart touch controls");
        int[] resources = { R.drawable.touch_accelerator, R.drawable.touch_brake, R.drawable.ic_drift,
            R.drawable.ic_item, R.drawable.ic_pause, R.drawable.ic_camera, R.drawable.ic_rear_view, R.drawable.ic_jump };
        int[] rimResources = { R.drawable.touch_rim_green, R.drawable.touch_rim_red,
            R.drawable.touch_rim_blue, R.drawable.touch_rim_gold,
            R.drawable.touch_rim_violet, R.drawable.touch_rim_silver, R.drawable.touch_rim_blue, R.drawable.touch_rim_violet };
        for (int i=0; i<icons.length; ++i) {
            icons[i] = context.getDrawable(resources[i]).mutate();
            iconShadows[i] = context.getDrawable(resources[i]).mutate();
            iconShadows[i].setTint(0xFF080B13);
            rims[i] = BitmapFactory.decodeResource(getResources(), rimResources[i]);
        }
        shiftKnob = BitmapFactory.decodeResource(getResources(), R.drawable.touch_shift_knob);
        steeringWheel = BitmapFactory.decodeResource(getResources(), R.drawable.touch_steering);
        lookIcon = context.getDrawable(R.drawable.ic_look).mutate();
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (w <= 0 || h <= 0) return;
        unit = Math.min(h / 7.5f, w / 14.0f);
        gearIdle = new LinearGradient(0,-unit*.29f,0,0,
            new int[]{0xFFFFFFFF,0xFFE9DDFF,0xFFA77CEB},new float[]{0,.5f,1},Shader.TileMode.CLAMP);
        gearSelected = new LinearGradient(0,-unit*.29f,0,0,
            new int[]{0xFFFFFFBD,0xFFFFE525,0xFFFF8D15},new float[]{0,.5f,1},Shader.TileMode.CLAMP);
        cx = 1.15f * unit; cy = h - 1.15f * unit;
        // Match the native HUD's 96-unit column plus its 8-unit edge inset at scale .8.
        float hudColumn = Math.min(h / 240.0f, w / 320.0f) * 104.0f * .8f;
        float controlEdge = layoutMode == 0 ? w - hudColumn : w;
        lookCx = controlEdge-.85f*unit; lookCy = h*.42f; lookRadius=.65f*unit;
        pedalX = controlEdge-1.85f*unit; pedalY = h-1.85f*unit;
        gx = pedalX-4.0f*unit; gy = pedalY;
        gateUnit = unit*.8f;
        gateTrack.rewind();
        gateTrack.moveTo(gx-gateUnit,gy);
        gateTrack.lineTo(gx+1.85f*gateUnit,gy);
        for (int col=-1; col<=1; ++col) {
            gateTrack.moveTo(gx+col*gateUnit,gy-gateUnit);
            gateTrack.lineTo(gx+col*gateUnit,gy+gateUnit);
        }
        buttons[0] = new float[] {pedalX,pedalY,PEDAL_RADIUS};
        for (int sector=0; sector<4; ++sector) {
            double angle = Math.toRadians(-90+90*sector);
            float radius = (RING_INNER+RING_OUTER)*.5f*unit;
            buttons[RING_ACTIONS[sector]] = new float[] {
                pedalX+(float)Math.cos(angle)*radius, pedalY+(float)Math.sin(angle)*radius, .43f};
        }
        buttons[4] = new float[] {.65f*unit,2.6f*unit,.38f};
        buttons[5] = new float[] {.65f*unit,3.9f*unit,.48f};
        buttons[6] = new float[] {lookCx,lookCy-1.4f*unit,.4f};
        release();
    }

    private void sprite(Canvas c, Bitmap bitmap, float x, float y, float radius, int alpha) {
        spriteRect.set(x-radius, y-radius, x+radius, y+radius);
        paint.setColor(0xFFFFFFFF);
        paint.setAlpha(alpha);
        paint.setFilterBitmap(true);
        c.drawBitmap(bitmap, null, spriteRect, paint);
        paint.setAlpha(255);
    }

    private void button(Canvas c, float x, float y, float r, int art, boolean held) {
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(held ? (accents[art] & 0xFFFFFF) | 0xB0000000 : 0x65080B13);
        c.drawCircle(x,y,r*.88f,paint);
        sprite(c,rims[art],x,y,r,held ? 255 : 215);
        if (held) {
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(unit*.025f);
            paint.setColor(0xFFF9FFFF);
            c.drawCircle(x,y,r*.72f,paint);
        }
        paint.setStyle(Paint.Style.FILL);
    }

    @Override protected void onDraw(Canvas c) {
        if (!syncTouchMode()) return;
        if (drawCount++ == 0) android.util.Log.i("KartTouch", "overlay first draw size="+getWidth()+"x"+getHeight());
        c.save();
        c.rotate(steeringAngle(),cx,cy);
        sprite(c,steeringWheel,cx,cy,unit,255);
        c.restore();
        button(c,cx+sx*unit*.65f,cy-sy*unit*.65f,unit*.44f,5,stickId!=-1);
        button(c,lookCx,lookCy,lookRadius,2,lookId!=-1);
        float knobX=lookCx+lookX*lookRadius*.65f, knobY=lookCy-lookY*lookRadius*.65f;
        button(c,knobX,knobY,lookRadius*.44f,5,lookId!=-1);
        float eye=lookRadius*.24f;
        lookIcon.setBounds(Math.round(knobX-eye),Math.round(knobY-eye),Math.round(knobX+eye),Math.round(knobY+eye));
        lookIcon.draw(c);
        drawPedalRing(c);
        for (int i=0; i<buttons.length; i++) {
            boolean held=isHeld(i);
            if (!isRingAction(i)) button(c,buttons[i][0],buttons[i][1],buttons[i][2]*unit,i,held);
            float half = buttons[i][2]*unit*(i<2 ? .65f : .54f);
            float y=buttons[i][1]+(held ? unit*.025f : 0);
            float outline=unit*.025f;
            iconShadows[i].setBounds(Math.round(buttons[i][0]-half-outline), Math.round(y-half),
                Math.round(buttons[i][0]+half+outline), Math.round(y+half+2*outline));
            if (i>=2) {
                iconShadows[i].draw(c);
                icons[i].setTint(held ? 0xFFFFFFFF : 0xFFF8F5DA);
            }
            icons[i].setBounds(Math.round(buttons[i][0]-half), Math.round(y-half-outline),
                Math.round(buttons[i][0]+half), Math.round(y+half-outline));
            icons[i].draw(c);
        }
        if (manual) drawShifter(c);
    }

    private float steeringAngle() { return sx*50.0f; }

    private boolean isRingAction(int action) { return action==1 || action==2 || action==3 || action==7; }

    private boolean isHeld(int action) {
        return action==0 ? !gasPointers.isEmpty() : pointers.containsValue(action) || radialActions.containsValue(action);
    }

    private int ringActionAt(float x, float y) {
        float dx=x-pedalX, dy=y-pedalY;
        double radius=Math.hypot(dx,dy)/unit;
        if (radius<RING_INNER || radius>RING_OUTER) return -1;
        double angle=(Math.toDegrees(Math.atan2(dy,dx))+450)%360;
        int sector=(int)Math.floor((angle+45)/90)%4;
        // The visible separator is also a release area.
        double fromCenter=Math.abs(((angle-sector*90+540)%360)-180);
        return fromCenter<=42 ? RING_ACTIONS[sector] : -1;
    }

    private void drawPedalRing(Canvas c) {
        for (int sector=0; sector<4; ++sector) {
            int action=RING_ACTIONS[sector];
            float start=-132+90*sector;
            float outer=RING_OUTER*unit, inner=RING_INNER*unit;
            ringSegment.rewind();
            ringBounds.set(pedalX-outer,pedalY-outer,pedalX+outer,pedalY+outer);
            ringSegment.arcTo(ringBounds,start,84,true);
            ringBounds.set(pedalX-inner,pedalY-inner,pedalX+inner,pedalY+inner);
            ringSegment.arcTo(ringBounds,start+84,-84,false);
            ringSegment.close();
            paint.setStyle(Paint.Style.FILL);
            paint.setColor((accents[action]&0xFFFFFF) | (isHeld(action) ? 0xDF000000 : 0x80000000));
            c.drawPath(ringSegment,paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeJoin(Paint.Join.ROUND);
            paint.setStrokeWidth(unit*.065f);
            paint.setColor(0xFF10151E);
            c.drawPath(ringSegment,paint);
            paint.setStrokeWidth(unit*.025f);
            paint.setColor(isHeld(action) ? 0xFFFFFFFF : accents[action]);
            c.drawPath(ringSegment,paint);
        }
        paint.setStyle(Paint.Style.FILL);
    }

    private void drawShifter(Canvas c) {
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStyle(Paint.Style.STROKE);
        // Layered bevels follow the existing wide lanes, without changing the gate.
        for (int pass=0; pass<3; ++pass) {
            paint.setColor(TRACK_COLORS[pass]);
            paint.setStrokeWidth(gateUnit*TRACK_WIDTHS[pass]);
            c.drawPath(gateTrack,paint);
        }
        paint.setColor(shifterId!=-1 ? 0xFFFFDC47 : 0xCCDBE6F8);
        paint.setStrokeWidth(gateUnit*.045f);
        c.drawPath(gateTrack,paint);
        paint.setStyle(Paint.Style.FILL);
        for (int gear=1; gear<=6; ++gear) {
            float x = gx+((gear-1)/2-1)*gateUnit;
            float y = gy+((gear & 1) == 1 ? -1.5f : 1.8f)*gateUnit;
            gearLabel(c,Integer.toString(gear),x,y,gear == gate.gear());
        }
        gearLabel(c,"R",gx+1.85f*gateUnit,gy-.55f*gateUnit,gate.gear() == -1);
        sprite(c,shiftKnob,gx+gate.x*gateUnit,gy+gate.y*gateUnit,.34f*unit,255);
    }

    private void gearLabel(Canvas c, String text, float x, float y, boolean selected) {
        c.save();
        c.translate(x,y);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTypeface(android.graphics.Typeface.create("sans-serif-condensed",android.graphics.Typeface.BOLD));
        paint.setTextSize(unit*.37f);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setStrokeWidth(unit*.055f);
        paint.setColor(0xFF080B13);
        c.drawText(text,unit*.015f,unit*.025f,paint);
        c.drawText(text,0,0,paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xFFFFFFFF);
        paint.setShader(selected ? gearSelected : gearIdle);
        c.drawText(text,0,0,paint);
        paint.setShader(null);
        c.restore();
    }

    @Override public boolean onHoverEvent(MotionEvent event) {
        if (!syncTouchMode()) return false;
        String action = null;
        if (event.getActionMasked()!=MotionEvent.ACTION_HOVER_EXIT) {
            if (Math.hypot(event.getX()-lookCx,event.getY()-lookCy)<lookRadius) action="Look Around";
            for (int i=0; i<buttons.length; ++i)
                if (Math.hypot(event.getX()-buttons[i][0],event.getY()-buttons[i][1])<buttons[i][2]*unit)
                    action = actions[i];
            int ringAction=ringActionAt(event.getX(),event.getY());
            if (ringAction>=0) action=actions[ringAction];
        }
        setTooltipText(action);
        return super.onHoverEvent(event);
    }

    private void publish() {
        if (!gameplayActive) {
            MainActivity.touchPad(modeToken, 0, 0, 0, 0, 0);
            invalidate();
            return;
        }
        int mask=manual ? shiftButton | (shifterId != -1 ? 0x00010000 : 0) : 0;
        for (int i : pointers.values()) mask |= masks[i];
        if (!gasPointers.isEmpty()) mask |= masks[0];
        for (int i : radialActions.values()) mask |= masks[i];
        MainActivity.touchPad(modeToken, mask, Math.round(sx*80), Math.round(sy*80), Math.round(lookX*80), Math.round(lookY*80));
        invalidate();
    }

    void release() {
        gestureActive=false;
        radialPointers.clear(); gasPointers.clear(); radialActions.clear();
        pointers.clear(); stickId=shifterId=lookId=-1; shiftButton=0; sx=sy=lookX=lookY=0; gate.settle(); publish();
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        if (!syncTouchMode()) return false;
        int action=e.getActionMasked(), index=e.getActionIndex(), id=e.getPointerId(index);
        // A gesture that began before hiding must not attach itself to the resumed controls.
        if (action!=MotionEvent.ACTION_DOWN && !gestureActive) return false;
        if (action==MotionEvent.ACTION_CANCEL) { release(); return true; }
        if (action==MotionEvent.ACTION_DOWN && gestureActive) release();
        if (action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            float x=e.getX(index), y=e.getY(index);
            if (stickId==-1 && Math.hypot(x-cx,y-cy)<unit*1.35f) stickId=id;
            else if (lookId==-1 && Math.hypot(x-lookCx,y-lookCy)<lookRadius*1.2f) lookId=id;
            else if (manual && shifterId==-1 && Math.hypot(x-gx-gate.x*gateUnit,y-gy-gate.y*gateUnit)<unit*.6f) {
                shifterId=id; grabX=x-gx-gate.x*gateUnit; grabY=y-gy-gate.y*gateUnit;
                shiftButton=gate.button();
            }
            else if (Math.hypot(x-pedalX,y-pedalY)<=RING_OUTER*unit) {
                radialPointers.add(id);
                if (Math.hypot(x-pedalX,y-pedalY)<=PEDAL_RADIUS*unit) gasPointers.add(id);
            }
            else for (int i=4;i<=6;i++) {
                if (Math.hypot(x-buttons[i][0],y-buttons[i][1])<buttons[i][2]*unit*1.2f) {
                    pointers.put(id,i); break;
                }
            }
            // Free gameplay fingers may slide into the ring; only a direct pedal
            // down latches gas. Steering, camera, shifter and utility fingers stay owned.
            if (id!=stickId && id!=shifterId && id!=lookId && !pointers.containsKey(id)) radialPointers.add(id);
            gestureActive=true;
        }
        int shifterIndex=e.findPointerIndex(shifterId);
        if (shifterIndex>=0 && (action==MotionEvent.ACTION_MOVE ||
            (id==shifterId && (action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP)))) {
            for (int history=0; history<e.getHistorySize(); ++history)
                gate.move((e.getHistoricalX(shifterIndex,history)-gx-grabX)/gateUnit,
                          (e.getHistoricalY(shifterIndex,history)-gy-grabY)/gateUnit);
            gate.move((e.getX(shifterIndex)-gx-grabX)/gateUnit,(e.getY(shifterIndex)-gy-grabY)/gateUnit);
            shiftButton=gate.button();
        }
        if (action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
            if (action==MotionEvent.ACTION_UP) gestureActive=false;
            pointers.remove(id);
            radialPointers.remove(id); gasPointers.remove(id); radialActions.remove(id);
            if (id==stickId) { stickId=-1; sx=sy=0; }
            if (id==lookId) { lookId=-1; lookX=lookY=0; }
            if (id==shifterId) { gate.settle(); shiftButton=gate.button(); shifterId=-1; }
        }
        int stickIndex=e.findPointerIndex(stickId);
        if (stickIndex>=0) {
            sx=(e.getX(stickIndex)-cx)/unit; sy=(cy-e.getY(stickIndex))/unit;
            float length=(float)Math.hypot(sx,sy);
            if (length<.12f) sx=sy=0;
            else { float scale=(Math.min(length,1)-.12f)/(.88f*length); sx*=scale; sy*=scale; }
        }
        int lookIndex=e.findPointerIndex(lookId);
        if (lookIndex>=0) {
            lookX=(e.getX(lookIndex)-lookCx)/lookRadius;
            lookY=(lookCy-e.getY(lookIndex))/lookRadius;
            float length=(float)Math.hypot(lookX,lookY);
            if (length<.12f) lookX=lookY=0;
            else { float scale=(Math.min(length,1)-.12f)/(.88f*length); lookX*=scale; lookY*=scale; }
        }
        for (int pointer : radialPointers) {
            int pointerIndex=e.findPointerIndex(pointer);
            int ringAction=pointerIndex<0 ? -1 : ringActionAt(e.getX(pointerIndex),e.getY(pointerIndex));
            if (ringAction<0) radialActions.remove(pointer);
            else radialActions.put(pointer,ringAction);
        }
        publish(); return true;
    }

    private void event(int action, float[][] points) {
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[points.length];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[points.length];
        for (int i=0; i<points.length; ++i) {
            props[i] = new MotionEvent.PointerProperties(); props[i].id=i;
            props[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x=points[i][0]; coords[i].y=points[i][1]; coords[i].pressure=1; coords[i].size=1;
        }
        long now=SystemClock.uptimeMillis();
        MotionEvent e=MotionEvent.obtain(now,now,action,points.length,props,coords,0,0,1,1,0,0,0,0);
        onTouchEvent(e); e.recycle();
    }

    // App-owned view events only: no system input injection or desktop control.
    void selfTest(File folder) {
        if (!syncTouchMode() || getWidth()<=0 || getHeight()<=0) return;
        JSONObject checks=new JSONObject();
        try {
            checks.put("layout",getWidth()>0 && getHeight()>0 && unit>0);
            float hudLeft = getWidth() - Math.min(getHeight()/240.0f,getWidth()/320.0f)*104.0f*.8f;
            boolean hudClear=true;
            for (int i=0;i<4;++i) hudClear &= buttons[i][0]+buttons[i][2]*unit < hudLeft;
            checks.put("hud_column_clear",layoutMode == 1 || hudClear);
            checks.put("layout_mode_valid",layoutMode == 0 || layoutMode == 1);
            checks.put("camera_screen_fit",lookCx-lookRadius*1.2f>cx+unit*1.35f
                && lookCy+lookRadius<getHeight() && lookCy-lookRadius>0);
            checks.put("camera_hud_clear",layoutMode==1 || lookCx+lookRadius<hudLeft);
            boolean cameraClear=true;
            for (float[] b:buttons) cameraClear &= Math.hypot(b[0]-lookCx,b[1]-lookCy)>
                lookRadius*1.2f+b[2]*unit*1.2f;
            checks.put("camera_button_clearance",cameraClear);
            float[] a={buttons[0][0],buttons[0][1]}, stick={cx+unit,cy}, r={buttons[2][0],buttons[2][1]};
            for (int ringAction : RING_ACTIONS) {
                float[] destination={buttons[ringAction][0],buttons[ringAction][1]};
                event(MotionEvent.ACTION_DOWN,new float[][]{a});
                event(MotionEvent.ACTION_MOVE,new float[][]{destination});
                checks.put("gas_drag_"+actions[ringAction],(MainActivity.touchSnapshot()&0xffff)==(masks[0]|masks[ringAction]));
                event(MotionEvent.ACTION_MOVE,new float[][]{{pedalX-2*unit,pedalY}});
                checks.put("ring_leave_"+actions[ringAction],(MainActivity.touchSnapshot()&0xffff)==masks[0]);
                event(MotionEvent.ACTION_UP,new float[][]{destination});
                checks.put("gas_lift_"+actions[ringAction],MainActivity.touchSnapshot()==0);
                event(MotionEvent.ACTION_DOWN,new float[][]{destination});
                event(MotionEvent.ACTION_MOVE,new float[][]{a});
                checks.put("ring_to_center_no_gas_"+actions[ringAction],MainActivity.touchSnapshot()==0);
                release();
            }
            event(MotionEvent.ACTION_DOWN,new float[][]{a});
            checks.put("throttle_confirm",(MainActivity.touchSnapshot() & 0xffff)==0x8040);
            event(MotionEvent.ACTION_POINTER_DOWN | (1<<8),new float[][]{a,stick});
            event(MotionEvent.ACTION_POINTER_DOWN | (2<<8),new float[][]{a,stick,r});
            long held=MainActivity.touchSnapshot();
            checks.put("steer_throttle_drift",(held & 0xffff)==0x8050 && ((held>>32)&255)==80);
            event(MotionEvent.ACTION_POINTER_UP | (2<<8),new float[][]{a,stick,r});
            checks.put("independent_release",(MainActivity.touchSnapshot() & 0xffff)==0x8040);
            event(MotionEvent.ACTION_CANCEL,new float[][]{a,stick});
            checks.put("cancel_releases",MainActivity.touchSnapshot()==0);
            for (int i=1;i<buttons.length;++i) {
                event(MotionEvent.ACTION_DOWN,new float[][]{{buttons[i][0],buttons[i][1]}});
                checks.put("action_"+actions[i],(MainActivity.touchSnapshot() & 0xffff)==masks[i]);
                release();
            }
            // This is the exact release hook called by Activity pause/focus loss.
            event(MotionEvent.ACTION_DOWN,new float[][]{a}); release();
            checks.put("pause_release_hook",MainActivity.touchSnapshot()==0);
            float[] look={lookCx+lookRadius,lookCy};
            float[] mirror={buttons[6][0],buttons[6][1]};
            event(MotionEvent.ACTION_DOWN,new float[][]{look});
            checks.put("look_right",((MainActivity.touchSnapshot()>>>48)&255)==80
                && ((MainActivity.touchSnapshot()>>>32)&65535)==0);
            event(MotionEvent.ACTION_POINTER_DOWN | (1<<8),new float[][]{look,stick});
            event(MotionEvent.ACTION_POINTER_DOWN | (2<<8),new float[][]{look,stick,a});
            checks.put("look_steer_throttle",((MainActivity.touchSnapshot()>>>48)&255)==80
                && ((MainActivity.touchSnapshot()>>>32)&255)==80 && (MainActivity.touchSnapshot()&0xffff)==0x8040);
            event(MotionEvent.ACTION_POINTER_DOWN | (3<<8),new float[][]{look,stick,a,mirror});
            checks.put("mirror_with_look",(MainActivity.touchSnapshot()&2)!=0
                && ((MainActivity.touchSnapshot()>>>48)&255)==80);
            event(MotionEvent.ACTION_POINTER_UP | (3<<8),new float[][]{look,stick,a,mirror});
            checks.put("mirror_release_restores_look",(MainActivity.touchSnapshot()&2)==0
                && ((MainActivity.touchSnapshot()>>>48)&255)==80);
            event(MotionEvent.ACTION_POINTER_UP,new float[][]{look,stick,a});
            checks.put("look_independent_release",(MainActivity.touchSnapshot()>>>48)==0
                && ((MainActivity.touchSnapshot()>>>32)&255)==80 && (MainActivity.touchSnapshot()&0xffff)==0x8040);
            release();
            event(MotionEvent.ACTION_DOWN,new float[][]{{lookCx,lookCy-lookRadius}});
            checks.put("look_up",((MainActivity.touchSnapshot()>>>56)&255)==80);
            event(MotionEvent.ACTION_MOVE,new float[][]{{lookCx,lookCy+lookRadius}});
            checks.put("look_down",((MainActivity.touchSnapshot()>>>56)&255)==176);
            event(MotionEvent.ACTION_MOVE,new float[][]{{lookCx-lookRadius,lookCy}});
            checks.put("look_left",((MainActivity.touchSnapshot()>>>48)&255)==176);
            event(MotionEvent.ACTION_MOVE,new float[][]{{lookCx,lookCy}});
            checks.put("look_deadzone",(MainActivity.touchSnapshot()>>>48)==0);
            event(MotionEvent.ACTION_MOVE,new float[][]{{lookCx+10*lookRadius,lookCy-10*lookRadius}});
            long diagonal=MainActivity.touchSnapshot();
            checks.put("look_clamped",Math.hypot((diagonal>>>48)&255,(diagonal>>>56)&255)<=81);
            event(MotionEvent.ACTION_CANCEL,new float[][]{look});
            checks.put("look_cancel_releases",MainActivity.touchSnapshot()==0 && lookId==-1);
            if (manual) {
                gate.select(1);
                float[] knob={gx-gateUnit,gy-gateUnit};
                event(MotionEvent.ACTION_DOWN,new float[][]{knob});
                checks.put("shifter_clutch_grab",(MainActivity.touchSnapshot() & 0x00010000)!=0);
                event(MotionEvent.ACTION_MOVE,new float[][]{{gx+gateUnit,gy-gateUnit}});
                checks.put("shifter_slot_wall",gate.x == -1 && gate.gear() == 1);
                event(MotionEvent.ACTION_MOVE,new float[][]{{gx-gateUnit,gy}});
                checks.put("shifter_neutral",gate.gear() == 0);
                event(MotionEvent.ACTION_MOVE,new float[][]{{gx,gy}});
                event(MotionEvent.ACTION_MOVE,new float[][]{{gx,gy-gateUnit}});
                checks.put("shifter_third",gate.gear() == 3 && (MainActivity.touchSnapshot() & 0x00080000)!=0);
                event(MotionEvent.ACTION_POINTER_DOWN | (1<<8),new float[][]{{gx,gy-gateUnit},stick});
                checks.put("shifter_with_steering",((MainActivity.touchSnapshot()>>32)&255)==80);
                event(MotionEvent.ACTION_POINTER_UP,new float[][]{{gx,gy-gateUnit},stick});
                checks.put("shifter_release_engages",shifterId == -1 && gate.gear() == 3
                    && (MainActivity.touchSnapshot() & 0x00010000)==0
                    && ((MainActivity.touchSnapshot()>>32)&255)==80);
                release();
                checks.put("shifter_cancel_releases",MainActivity.touchSnapshot()==0);
                gate.select((MainActivity.touchTransmission() & 255)-1);
                checks.put("shifter_hud_clear",layoutMode == 1 || gx+2.19f*unit < hudLeft);
                checks.put("shifter_screen_fit",gx-1.6f*unit>cx+unit*1.35f
                    && gy+1.5f*unit<getHeight() && gy-1.7f*unit>0);
            }
            File root=new File(folder,"kart-test"); root.mkdirs();
            Bitmap bitmap=Bitmap.createBitmap(getWidth(),getHeight(),Bitmap.Config.ARGB_8888);
            draw(new Canvas(bitmap));
            for (int i=0; i<icons.length; ++i) {
                android.graphics.Rect bounds = icons[i].getBounds();
                // Inspect the icon alone: silver pedal faces are not pure white,
                // and a bright button rim must not conceal a missing symbol.
                Bitmap symbol=Bitmap.createBitmap(bounds.width(),bounds.height(),Bitmap.Config.ARGB_8888);
                Canvas iconCanvas=new Canvas(symbol);
                iconCanvas.translate(-bounds.left,-bounds.top);
                icons[i].draw(iconCanvas);
                int ink = 0, dark = 0;
                for (int y=0; y<symbol.getHeight(); ++y) for (int x=0; x<symbol.getWidth(); ++x) {
                    int pixel = symbol.getPixel(x,y);
                    int light=Math.min(android.graphics.Color.red(pixel),Math.min(android.graphics.Color.green(pixel),android.graphics.Color.blue(pixel)));
                    if (android.graphics.Color.alpha(pixel)>230 && light>160) ++ink;
                    if (android.graphics.Color.alpha(pixel)>230 && light<80) ++dark;
                }
                symbol.recycle();
                checks.put("icon_"+actions[i],ink>20 && (i>=2 || dark>20) && bounds.left>=0 && bounds.top>=0
                    && bounds.right<=getWidth() && bounds.bottom<=getHeight());
            }
            try (FileOutputStream image=new FileOutputStream(new File(root,"touch-layout.png"))) {
                bitmap.compress(Bitmap.CompressFormat.PNG,100,image);
            }
            try (FileOutputStream image=new FileOutputStream(new File(root,"touch-layout-"+layoutMode+".png"))) {
                bitmap.compress(Bitmap.CompressFormat.PNG,100,image);
            }
            if (manual) try (FileOutputStream image=new FileOutputStream(new File(root,"touch-shifter.png"))) {
                bitmap.compress(Bitmap.CompressFormat.PNG,100,image);
            }
            for (int direction : new int[]{-1,1}) {
                event(MotionEvent.ACTION_DOWN,new float[][]{{cx+direction*unit,cy}});
                String side=direction<0 ? "left" : "right";
                checks.put("steering_"+side,Math.abs(steeringAngle()-direction*50.0f)<.01f
                    && ((MainActivity.touchSnapshot()>>32)&255)==(direction*80&255));
                bitmap.eraseColor(0);
                draw(new Canvas(bitmap));
                try (FileOutputStream image=new FileOutputStream(new File(root,"touch-steer-"+side+".png"))) {
                    bitmap.compress(Bitmap.CompressFormat.PNG,100,image);
                }
                release();
            }
            checks.put("steering_recenters",steeringAngle()==0 && sx==0 && sy==0 && MainActivity.touchSnapshot()==0);
            event(MotionEvent.ACTION_DOWN,new float[][]{{lookCx+lookRadius,lookCy}});
            bitmap.eraseColor(0); draw(new Canvas(bitmap));
            try (FileOutputStream image=new FileOutputStream(new File(root,"touch-camera.png"))) {
                bitmap.compress(Bitmap.CompressFormat.PNG,100,image);
            }
            release();
            bitmap.recycle();
            File temp=new File(root,"touch-selftest.pending");
            try (FileWriter writer=new FileWriter(temp)) { writer.write(checks.toString(2)); }
            temp.renameTo(new File(root,"touch-selftest.json"));
        } catch (Exception ex) {
            android.util.Log.e("KartTouchTest","Input self-test failed",ex);
        } finally { release(); }
    }
}
