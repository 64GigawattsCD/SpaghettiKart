package dev.spaghettikart.phone;

import android.os.Bundle;
import android.os.SystemClock;
import android.content.pm.ActivityInfo;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLControllerManager;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

public class MainActivity extends SDLActivity {
    private TouchControls controls;
    private boolean resumed;
    private boolean controllerSelfTest;
    private InputEvent controllerTestEvent;
    private Runnable touchTestRelease;
    private boolean dispatchingTouchTest;
    private JSONArray touchTestSurfaceEvents;
    static native void touchPad(int modeToken, int buttons, int x, int y, int lookX, int lookY);
    static native int touchMode();
    static native long touchSnapshot();
    static native int touchLayout();
    static native int touchTransmission();

    // Opt-in phone test mailbox. Coordinates are normalized to the SDL surface.
    // android-touch-command.json: {"id":"tap-1","x":0.5,"y":0.5,"hold_ms":120,"gameplay":false}
    private final Runnable touchDispatchPoll = new Runnable() {
        @Override public void run() {
            if (touchTestRelease == null && resumed && hasWindowFocus()) pollTouchDispatchTest();
            if (controls != null) controls.postDelayed(this, 50);
        }
    };

    private boolean dispatchTestTouch(int action, long downTime, float x, float y, int deviceId) {
        MotionEvent.PointerProperties pointer = new MotionEvent.PointerProperties();
        pointer.id = 0;
        pointer.toolType = MotionEvent.TOOL_TYPE_FINGER;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x; coords.y = y; coords.pressure = action == MotionEvent.ACTION_UP ? 0 : 1; coords.size = 1;
        MotionEvent event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, 1,
            new MotionEvent.PointerProperties[]{pointer}, new MotionEvent.PointerCoords[]{coords},
            0, 0, 1, 1, deviceId, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        dispatchingTouchTest = true;
        try { return dispatchTouchEvent(event); }
        finally { dispatchingTouchTest = false; event.recycle(); }
    }

    private void writeTouchDispatchReport(JSONObject report) {
        try {
            File root = new File(getExternalFilesDir(null), "kart-test");
            File pending = new File(root, "android-touch-result.pending");
            Files.write(pending.toPath(), report.toString(2).getBytes(StandardCharsets.UTF_8));
            Files.move(pending.toPath(), new File(root, "android-touch-result.json").toPath(), StandardCopyOption.REPLACE_EXISTING);
        } catch (Exception error) { android.util.Log.e("KartTouchDispatch", "Report failed", error); }
    }

    private void pollTouchDispatchTest() {
        File command = new File(getExternalFilesDir(null), "kart-test/android-touch-command.json");
        if (!command.isFile()) return;
        JSONObject report = new JSONObject();
        try {
            File claimed = new File(command.getParentFile(), "android-touch-command.processing");
            Files.move(command.toPath(), claimed.toPath(), StandardCopyOption.REPLACE_EXISTING);
            if (claimed.length() > 4096) throw new IllegalArgumentException("Command too large");
            JSONObject request = new JSONObject(new String(Files.readAllBytes(claimed.toPath()), StandardCharsets.UTF_8));
            Files.delete(claimed.toPath());
            report.put("id", request.getString("id"));
            if (request.optString("op").equals("status")) {
                report.put("overlay", controls.captureDiagnostics(command.getParentFile(), "overlay-status.png"));
                android.graphics.Bitmap windowPixels = android.graphics.Bitmap.createBitmap(
                    getWindow().getDecorView().getWidth(), getWindow().getDecorView().getHeight(), android.graphics.Bitmap.Config.ARGB_8888);
                android.view.PixelCopy.request(getWindow(), windowPixels, result -> {
                    try {
                        report.put("window_copy_result", result);
                        if (result == android.view.PixelCopy.SUCCESS) {
                            try (java.io.FileOutputStream out = new java.io.FileOutputStream(new File(command.getParentFile(), "window-status.png"))) {
                                windowPixels.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out);
                            }
                            report.put("window_capture", "window-status.png");
                        }
                    } catch (Exception error) {
                        try { report.put("error", error.toString()); } catch (Exception ignored) {}
                    } finally { windowPixels.recycle(); writeTouchDispatchReport(report); }
                }, new android.os.Handler(getMainLooper()));
                return;
            }
            double nx = request.getDouble("x"), ny = request.getDouble("y");
            int hold = request.optInt("hold_ms", 120);
            if (!Double.isFinite(nx) || !Double.isFinite(ny) || nx < 0 || nx > 1 || ny < 0 || ny > 1 || hold < 32 || hold > 2000)
                throw new IllegalArgumentException("Expected x/y in [0,1] and hold_ms in [32,2000]");
            boolean gameplay = controls.syncTouchMode();
            if (request.optBoolean("gameplay", false) != gameplay)
                throw new IllegalStateException("Unexpected touch mode");
            if (mSurface == null || mSurface.getWidth() <= 0 || mSurface.getHeight() <= 0)
                throw new IllegalStateException("SDL surface not ready");
            int deviceId = 0;
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice device = InputDevice.getDevice(id);
                if (device != null && device.supportsSource(InputDevice.SOURCE_TOUCHSCREEN)) { deviceId = id; break; }
            }
            final int touchDevice = deviceId;
            int[] location = new int[2];
            mSurface.getLocationInWindow(location);
            float x = location[0] + (float)nx * (mSurface.getWidth()-1);
            float y = location[1] + (float)ny * (mSurface.getHeight()-1);
            long downTime = SystemClock.uptimeMillis();
            touchTestSurfaceEvents = new JSONArray();
            // SDL reinstalls its listener on resume; observe only this test gesture.
            mSurface.setOnTouchListener((view, event) -> {
                if (dispatchingTouchTest && touchTestSurfaceEvents != null)
                    touchTestSurfaceEvents.put(event.getActionMasked());
                return mSurface.onTouch(view, event);
            });
            report.put("method", "Activity dispatchTouchEvent -> SDL surface onTouch -> SDL native touch; app-local touchscreen events")
                .put("mode_before", touchMode()).put("overlay_before", controls.getVisibility())
                .put("x", nx).put("y", ny).put("device_id", touchDevice);
            // Arm cleanup before dispatch, so every down has an up or lifecycle cancel.
            touchTestRelease = () -> {
                try {
                    int action = resumed && hasWindowFocus() ? MotionEvent.ACTION_UP : MotionEvent.ACTION_CANCEL;
                    report.put("release_handled", dispatchTestTouch(action, downTime, x, y, touchDevice))
                        .put("release_action", action).put("mode_after", touchMode())
                        .put("overlay_after", controls.getVisibility()).put("touch_pad_after", touchSnapshot())
                        .put("sdl_surface_events", touchTestSurfaceEvents).put("dispatched", true);
                } catch (Exception error) {
                    try { report.put("error", error.toString()); } catch (Exception ignored) {}
                } finally {
                    touchTestRelease = null;
                    writeTouchDispatchReport(report);
                }
            };
            report.put("down_handled", dispatchTestTouch(MotionEvent.ACTION_DOWN, downTime, x, y, touchDevice));
            controls.postDelayed(touchTestRelease, hold);
        } catch (Exception error) {
            if (touchTestRelease != null) { controls.removeCallbacks(touchTestRelease); touchTestRelease.run(); }
            try { report.put("error", error.toString()).put("dispatched", false); } catch (Exception ignored) {}
            writeTouchDispatchReport(report);
        }
    }

    @Override public boolean dispatchTouchEvent(MotionEvent event) {
        // Refresh before ViewGroup hit testing, including the first tap after a menu opens.
        if (controls != null) controls.syncTouchMode();
        return super.dispatchTouchEvent(event);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        // A focused Android overlay must not take buttons away from SDL/ControlDeck.
        if (!mBrokenLibraries && SDLControllerManager.isDeviceSDLJoystick(event.getDeviceId())) {
            if (controllerSelfTest && event != controllerTestEvent) return true;
            int result = -1;
            if (event.getAction() == KeyEvent.ACTION_DOWN)
                result = SDLControllerManager.onNativePadDown(event.getDeviceId(), event.getKeyCode());
            else if (event.getAction() == KeyEvent.ACTION_UP)
                result = SDLControllerManager.onNativePadUp(event.getDeviceId(), event.getKeyCode());
            if (result == 0) return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (!mBrokenLibraries && event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
            if (controllerSelfTest && event != controllerTestEvent) return true;
            return SDLControllerManager.handleJoystickMotionEvent(event);
        }
        return super.dispatchGenericMotionEvent(event);
    }

    void controllerTestMode(boolean enabled) {
        controllerSelfTest = enabled && getIntent().getBooleanExtra("kart_test", false)
            && getIntent().getBooleanExtra("controller_test", false);
        // Exercise routing with focus on the overlay instead of the SDL surface.
        controls.setFocusableInTouchMode(controllerSelfTest);
        controls.setFocusable(controllerSelfTest);
        if (controllerSelfTest) controls.requestFocus();
        else if (mSurface != null) mSurface.requestFocus();
    }

    void dispatchControllerTestEvent(InputEvent event) {
        if (!controllerSelfTest) return;
        controllerTestEvent = event;
        try {
            if (event instanceof KeyEvent) dispatchKeyEvent((KeyEvent) event);
            else dispatchGenericMotionEvent((MotionEvent) event);
        } finally { controllerTestEvent = null; }
    }

    @Override protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    @Override protected String[] getArguments() {
        String[] arguments = gameArguments();
        if (getIntent().getBooleanExtra("kart_test", false) && getIntent().getBooleanExtra("touch_dispatch_test", false)) {
            arguments = java.util.Arrays.copyOf(arguments, arguments.length+1);
            arguments[arguments.length-1] = "--kart-touch-dispatch-test";
        }
        return arguments;
    }

    private String[] gameArguments() {
        String host = getIntent().getStringExtra("net_join");
        if (host != null && host.matches("[0-9.]{7,15}"))
            return getIntent().getBooleanExtra("kart_test", false)
                ? new String[] { "--novr", "--kart-test", "--net-join="+host }
                : new String[] { "--novr", "--net-join="+host };
        if (getIntent().getBooleanExtra("kart_test", false) && getIntent().getBooleanExtra("audio_test", false))
            return new String[] { "--novr", "--kart-test", "--kart-audio-test" };
        return getIntent().getBooleanExtra("kart_test", false)
            ? new String[] { "--novr", "--kart-test" } : new String[] { "--novr" };
    }

    @Override public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mLayout == null) return;
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        controls = new TouchControls(this);
        mLayout.addView(controls, new RelativeLayout.LayoutParams(-1, -1));
        if (getIntent().getBooleanExtra("kart_test", false)) {
            new java.io.File(getExternalFilesDir(null), "kart-test").mkdirs();
            if (getIntent().getBooleanExtra("touch_dispatch_test", false)) {
                // Observe the actual SDL listener reached by normal Android view dispatch.
                mSurface.setOnTouchListener((view, event) -> {
                    if (dispatchingTouchTest && touchTestSurfaceEvents != null)
                        touchTestSurfaceEvents.put(event.getActionMasked());
                    return mSurface.onTouch(view, event);
                });
                controls.post(touchDispatchPoll);
            } else controls.postDelayed(() -> controls.selfTest(getExternalFilesDir(null)), 1000);
            if (getIntent().getBooleanExtra("controller_test", false)) {
                controls.postDelayed(() -> new ControllerSelfTest(this).start(), 3000);
            }
        }
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        if (controls != null) controls.setHostActive(hasWindowFocus());
    }

    @Override protected void onPause() {
        resumed = false;
        if (touchTestRelease != null) { controls.removeCallbacks(touchTestRelease); touchTestRelease.run(); }
        if (controls != null) controls.setHostActive(false);
        super.onPause();
    }

    @Override public void onWindowFocusChanged(boolean focused) {
        if (controls != null) controls.setHostActive(resumed && focused);
        if (!focused && touchTestRelease != null) { controls.removeCallbacks(touchTestRelease); touchTestRelease.run(); }
        super.onWindowFocusChanged(focused);
    }

    @Override protected void onDestroy() {
        if (controls != null) controls.removeCallbacks(touchDispatchPoll);
        super.onDestroy();
    }
}
