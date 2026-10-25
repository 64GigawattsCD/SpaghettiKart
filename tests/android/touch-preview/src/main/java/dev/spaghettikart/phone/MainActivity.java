package dev.spaghettikart.phone;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;
import org.json.JSONArray;
import org.json.JSONObject;

/** Emulator-only renderer/gesture fixture; no game JNI, audio or ROM data. */
public final class MainActivity extends Activity {
    private static long pad;
    private static int transmission, layout, modeToken;
    static int touchMode() { return modeToken; }
    private static void mode(int next) {
        if ((modeToken & TouchControls.MODE_MASK)==next) return;
        pad=0;
        modeToken=((modeToken+4)&0x7ffffffc)|next;
    }
    static void touchPad(int token, int buttons, int x, int y, int lookX, int lookY) {
        if (token!=modeToken || (token & TouchControls.MODE_MASK)!=TouchControls.MODE_GAMEPLAY) return;
        pad=(buttons & 0xffffffffL) | ((x & 255L)<<32) | ((y & 255L)<<40)
            | ((lookX & 255L)<<48) | ((lookY & 255L)<<56);
    }
    static long touchSnapshot() { return pad; }
    static int touchLayout() { return layout; }
    static int touchTransmission() { return transmission; }

    private static void field(TouchControls view, String name, Object value) throws Exception {
        Field field=TouchControls.class.getDeclaredField(name);
        field.setAccessible(true);
        field.set(view,value);
    }

    private static float number(TouchControls view, String name) throws Exception {
        Field field=TouchControls.class.getDeclaredField(name);
        field.setAccessible(true);
        return field.getFloat(view);
    }

    private static Bitmap capture(TouchControls view, File path) throws Exception {
        Bitmap bitmap=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);
        view.draw(new Canvas(bitmap));
        try (FileOutputStream stream=new FileOutputStream(path)) {
            bitmap.compress(Bitmap.CompressFormat.PNG,100,stream);
        }
        return bitmap;
    }

    private static void gesture(TouchControls view, int action, float[][] points) throws Exception {
        java.lang.reflect.Method event=TouchControls.class.getDeclaredMethod("event",int.class,float[][].class);
        event.setAccessible(true);
        event.invoke(view,action,points);
    }

    private static boolean tap(TouchControls view, int action, float x, float y) {
        MotionEvent event=MotionEvent.obtain(0,0,action,x,y,0);
        try { return view.onTouchEvent(event); }
        finally { event.recycle(); }
    }

    private static void modeChecks(TouchControls view, JSONObject checks, boolean manual) throws Exception {
        float[] stick={number(view,"cx")+number(view,"unit"),number(view,"cy")};
        float[] look={number(view,"lookCx")+number(view,"lookRadius"),number(view,"lookCy")};
        Field buttonsField=TouchControls.class.getDeclaredField("buttons");
        buttonsField.setAccessible(true);
        float[] throttle=((float[][])buttonsField.get(view))[0];
        for (int hidden:new int[]{0,2}) {
            gesture(view,MotionEvent.ACTION_DOWN,new float[][]{stick});
            gesture(view,MotionEvent.ACTION_POINTER_DOWN|(1<<8),new float[][]{stick,look});
            gesture(view,MotionEvent.ACTION_POINTER_DOWN|(2<<8),new float[][]{stick,look,throttle});
            if (manual) {
                float[] knob={number(view,"gx")-number(view,"gateUnit"),number(view,"gy")-number(view,"gateUnit")};
                gesture(view,MotionEvent.ACTION_POINTER_DOWN|(3<<8),new float[][]{stick,look,throttle,knob});
            }
            checks.put("mode_held_"+hidden,((pad>>>32)&255)==80 && ((pad>>>48)&255)==80
                && (pad&0x8040)==0x8040 && (!manual || (pad&0x10000)!=0));
            mode(hidden);
            checks.put("mode_hidden_"+hidden,!view.syncTouchMode() && view.getVisibility()==View.INVISIBLE && pad==0);
            boolean bypass=true;
            for (int action:new int[]{MotionEvent.ACTION_DOWN,MotionEvent.ACTION_MOVE,MotionEvent.ACTION_UP,MotionEvent.ACTION_CANCEL})
                bypass &= !tap(view,action,throttle[0],throttle[1]);
            checks.put("mode_bypass_"+hidden,bypass && pad==0);
            Bitmap hiddenArt=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);
            view.draw(new Canvas(hiddenArt));
            boolean blank=true;
            for (int y=0;y<hiddenArt.getHeight();y+=4) for (int x=0;x<hiddenArt.getWidth();x+=4)
                blank &= hiddenArt.getPixel(x,y)==0;
            hiddenArt.recycle();
            checks.put("mode_blank_"+hidden,blank);
            mode(TouchControls.MODE_GAMEPLAY);
            checks.put("mode_resume_"+hidden,view.syncTouchMode() && view.getVisibility()==View.VISIBLE && pad==0
                && number(view,"sx")==0 && number(view,"sy")==0 && number(view,"lookX")==0 && number(view,"lookY")==0);
            checks.put("mode_old_gesture_"+hidden,!tap(view,MotionEvent.ACTION_MOVE,throttle[0],throttle[1]) && pad==0);
            checks.put("mode_new_gesture_"+hidden,tap(view,MotionEvent.ACTION_DOWN,throttle[0],throttle[1]) && (pad&0xffff)==0x8040);
            view.release();
        }
        tap(view,MotionEvent.ACTION_DOWN,throttle[0],throttle[1]);
        mode(0); mode(TouchControls.MODE_GAMEPLAY);
        checks.put("mode_epoch_release",view.syncTouchMode() && pad==0
            && !tap(view,MotionEvent.ACTION_MOVE,throttle[0],throttle[1]));
        tap(view,MotionEvent.ACTION_DOWN,throttle[0],throttle[1]);
        view.setHostActive(false);
        checks.put("mode_host_inactive",view.getVisibility()==View.INVISIBLE && pad==0
            && !tap(view,MotionEvent.ACTION_DOWN,throttle[0],throttle[1]));
        view.setHostActive(true);
        checks.put("mode_host_resume",view.getVisibility()==View.VISIBLE && pad==0);
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        File root=new File(getExternalFilesDir(null),"kart-test");
        root.mkdirs();
        new File(root,"art-report.json").delete();
        JSONObject report=new JSONObject();
        try {
            JSONArray results=new JSONArray();
            int[][] sizes={{3088,1440},{2400,1080},{1920,1080},{1280,720},{1024,768}};
            for (int[] size:sizes) for (boolean manual:new boolean[]{false,true}) for (layout=0;layout<2;layout++) {
                transmission=manual ? 0x102 : 0;
                TouchControls view=new TouchControls(this);
                if (view.getVisibility()!=View.INVISIBLE) throw new AssertionError("Startup overlay visible");
                field(view,"manual",manual);
                field(view,"layoutMode",layout);
                view.layout(0,0,size[0],size[1]);
                mode(TouchControls.MODE_GAMEPLAY);
                view.setHostActive(true);
                String name=size[0]+"x"+size[1]+(manual ? "-manual" : "-auto")+"-"+layout;
                view.selfTest(getExternalFilesDir(null));
                JSONObject checks=new JSONObject(new String(Files.readAllBytes(
                    new File(root,"touch-selftest.json").toPath()),StandardCharsets.UTF_8));
                modeChecks(view,checks,manual);
                for (Iterator<String> keys=checks.keys();keys.hasNext();) {
                    String key=keys.next();
                    if (!checks.getBoolean(key)) throw new AssertionError(name+": "+key);
                }
                Bitmap idle=capture(view,new File(root,name+".png"));
                Field buttonsField=TouchControls.class.getDeclaredField("buttons");
                buttonsField.setAccessible(true);
                float[][] buttons=(float[][])buttonsField.get(view);
                MotionEvent down=MotionEvent.obtain(0,0,MotionEvent.ACTION_DOWN,buttons[0][0],buttons[0][1],0);
                view.onTouchEvent(down); down.recycle();
                Bitmap held=capture(view,new File(root,name+"-held.png"));
                int changed=0;
                for (int y=0;y<size[1];y+=2) for (int x=0;x<size[0];x+=2)
                    if (idle.getPixel(x,y)!=held.getPixel(x,y)) changed++;
                if (changed<100) throw new AssertionError("Pressed art unchanged: "+name);
                view.release();
                if (manual) {
                    float gateUnit=number(view,"gateUnit");
                    MotionEvent grab=MotionEvent.obtain(0,0,MotionEvent.ACTION_DOWN,
                        number(view,"gx")-gateUnit,number(view,"gy")-gateUnit,0);
                    view.onTouchEvent(grab); grab.recycle();
                    Bitmap clutch=capture(view,new File(root,name+"-clutch.png"));
                    int idleTrack=idle.getPixel(Math.round(number(view,"gx")),Math.round(number(view,"gy")));
                    int heldTrack=clutch.getPixel(Math.round(number(view,"gx")),Math.round(number(view,"gy")));
                    if (idleTrack==heldTrack || (touchSnapshot() & 0x10000)==0)
                        throw new AssertionError("Clutch art/state unchanged: "+name);
                    clutch.recycle();
                    checks.put("clutch_art",true);
                }
                // Empty driving space remains transparent, not covered by a panel.
                if ((idle.getPixel(size[0]/2,size[1]/3)>>>24)!=0) throw new AssertionError("Center covered");
                idle.recycle(); held.recycle(); view.release();
                results.put(new JSONObject().put("case",name).put("checks",checks).put("pressed_pixels",changed));
            }
            report.put("passed",true).put("cases",results);
        } catch (Throwable error) {
            android.util.Log.e("KartTouchArt", "Art tests failed",error);
            try { report.put("passed",false).put("error",error.toString()); } catch (Exception ignored) {}
        }
        try (FileWriter writer=new FileWriter(new File(root,"art-report.json"))) {
            writer.write(report.toString(2));
        } catch (Exception error) { throw new RuntimeException(error); }
        finish();
    }
}
