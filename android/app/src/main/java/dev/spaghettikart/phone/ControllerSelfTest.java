package dev.spaghettikart.phone;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

// Opt-in app-local events, using the attached controller's Android device profile.
// No device-wide injection. The native test bridge observes bindings then clears gameplay input.
final class ControllerSelfTest {
    private final MainActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<Case> cases = new ArrayList<>();
    private final JSONArray results = new JSONArray();
    private InputDevice device;
    private int index, attempts;
    private final File folder;

    private static final class Case {
        final String name, field;
        final int key, axis;
        final float amount;
        final long buttons;
        final double expected;
        Case(String name, int key, int axis, float amount, long buttons, String field, double expected) {
            this.name = name; this.key = key; this.axis = axis; this.amount = amount;
            this.buttons = buttons; this.field = field; this.expected = expected;
        }
    }

    ControllerSelfTest(MainActivity activity) {
        this.activity = activity;
        folder = new File(activity.getExternalFilesDir(null), "kart-test");
    }

    private void button(String name, int key, long mask) {
        cases.add(new Case(name, key, -1, 0, mask, null, 0));
        neutral(name + "-release");
    }
    private void axis(String name, int axis, float amount, long mask, String field, double expected) {
        cases.add(new Case(name, 0, axis, amount, mask, field, expected));
        neutral(name + "-release");
    }
    private void neutral(String name) { cases.add(new Case(name, 0, -1, 0, 0, null, 0)); }

    void start() {
        try {
            JSONObject state = readState();
            if (state.getJSONObject("state").getInt("race_state") != 3) throw new Exception("Waiting for race");
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice candidate = InputDevice.getDevice(id);
                if (candidate != null && candidate.getVendorId() == 0x3537 && candidate.getProductId() == 0x0105) {
                    device = candidate;
                    break;
                }
            }
            if (device == null) throw new Exception("GameSir X2s is not attached");
            activity.controllerTestMode(true);
            for (int code : new int[] {KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B,
                    KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_L1,
                    KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_SELECT,
                    KeyEvent.KEYCODE_BUTTON_THUMBL, KeyEvent.KEYCODE_BUTTON_THUMBR}) key(code, KeyEvent.ACTION_UP);
            neutral("neutral");
            button("A-jump-confirm", KeyEvent.KEYCODE_BUTTON_A, 0x60);
            button("B-item-cancel", KeyEvent.KEYCODE_BUTTON_B, 0x2080);
            button("X-unassigned", KeyEvent.KEYCODE_BUTTON_X, 0);
            button("Y-look-behind", KeyEvent.KEYCODE_BUTTON_Y, 2);
            button("LB-drift", KeyEvent.KEYCODE_BUTTON_L1, 0x10);
            button("RB-item", KeyEvent.KEYCODE_BUTTON_R1, 0x2000);
            button("start-menu", KeyEvent.KEYCODE_BUTTON_START, 0x1040);
            button("select-camera", KeyEvent.KEYCODE_BUTTON_SELECT, 1);
            button("L3", KeyEvent.KEYCODE_BUTTON_THUMBL, 0);
            button("R3-look-behind", KeyEvent.KEYCODE_BUTTON_THUMBR, 2);
            axis("left-stick-left", MotionEvent.AXIS_X, -1, 0, "stick_x", -85);
            axis("left-stick-right", MotionEvent.AXIS_X, 1, 0, "stick_x", 85);
            axis("left-stick-up", MotionEvent.AXIS_Y, -1, 0, "stick_y", 85);
            axis("left-stick-down", MotionEvent.AXIS_Y, 1, 0, "stick_y", -85);
            axis("look-left", MotionEvent.AXIS_Z, -1, 0, "look_x", -85);
            axis("look-right", MotionEvent.AXIS_Z, 1, 0, "look_x", 85);
            axis("look-up", MotionEvent.AXIS_RZ, -1, 0, "look_y", 85);
            axis("look-down", MotionEvent.AXIS_RZ, 1, 0, "look_y", -85);
            axis("LT-half", MotionEvent.AXIS_LTRIGGER, .5f, 0x4000, "brake", .5);
            axis("LT-full", MotionEvent.AXIS_LTRIGGER, 1, 0x4000, "brake", 1);
            axis("RT-half", MotionEvent.AXIS_RTRIGGER, .5f, 0x8000, "throttle", .5);
            axis("RT-full", MotionEvent.AXIS_RTRIGGER, 1, 0x8000, "throttle", 1);
            axis("dpad-left", MotionEvent.AXIS_HAT_X, -1, 0x200, null, 0);
            axis("dpad-right", MotionEvent.AXIS_HAT_X, 1, 0x100, null, 0);
            axis("dpad-up", MotionEvent.AXIS_HAT_Y, -1, 0x800, null, 0);
            axis("dpad-down", MotionEvent.AXIS_HAT_Y, 1, 0x400, null, 0);
            next();
        } catch (Exception e) {
            if (++attempts < 90) handler.postDelayed(this::start, 1000);
            else finish(e.toString());
        }
    }

    private JSONObject readState() throws Exception {
        return new JSONObject(new String(Files.readAllBytes(new File(folder, "state.json").toPath()), StandardCharsets.UTF_8));
    }

    private void key(int code, int action) {
        long now = SystemClock.uptimeMillis();
        activity.dispatchControllerTestEvent(new KeyEvent(now, now, action, code, 0, 0, device.getId(), 0, 0, InputDevice.SOURCE_GAMEPAD));
    }

    private void motion(Case step) {
        MotionEvent.PointerProperties pointer = new MotionEvent.PointerProperties();
        pointer.id = 0;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        if (step.axis >= 0) {
            coords.setAxisValue(step.axis, step.amount);
            if (step.axis == MotionEvent.AXIS_LTRIGGER) coords.setAxisValue(MotionEvent.AXIS_BRAKE, step.amount);
            if (step.axis == MotionEvent.AXIS_RTRIGGER) coords.setAxisValue(MotionEvent.AXIS_GAS, step.amount);
        }
        long now = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(now, now, MotionEvent.ACTION_MOVE, 1,
            new MotionEvent.PointerProperties[] {pointer}, new MotionEvent.PointerCoords[] {coords},
            0, 0, 1, 1, device.getId(), 0, InputDevice.SOURCE_JOYSTICK, 0);
        activity.dispatchControllerTestEvent(event);
        event.recycle();
    }

    private void next() {
        if (index == cases.size()) { finish(null); return; }
        Case step = cases.get(index);
        motion(step);
        if (step.key != 0) key(step.key, KeyEvent.ACTION_DOWN);
        handler.postDelayed(() -> {
            try {
                JSONObject state = readState();
                JSONObject pad = state.getJSONObject("physical_pad");
                boolean pass = pad.getLong("button") == step.buttons;
                for (String field : new String[] {"stick_x", "stick_y", "look_x", "look_y", "throttle", "brake"}) {
                    double expected = field.equals(step.field) ? step.expected : 0;
                    double tolerance = field.contains("_") ? 3 : .015;
                    pass &= Math.abs(pad.getDouble(field) - expected) <= tolerance;
                }
                results.put(new JSONObject().put("name", step.name).put("pass", pass)
                    .put("expected_buttons", step.buttons).put("pad", pad).put("sdl", state.getJSONArray("sdl_controllers")));
                if (step.key != 0) key(step.key, KeyEvent.ACTION_UP);
                ++index;
                next();
            } catch (Exception e) {
                if (step.key != 0) key(step.key, KeyEvent.ACTION_UP);
                motion(new Case("release", 0, -1, 0, 0, null, 0));
                finish(e.toString());
            }
        }, 250);
    }

    private void finish(String error) {
        activity.controllerTestMode(false);
        try {
            JSONObject report = new JSONObject().put("checks", results).put("error", error == null ? "" : error)
                .put("method", "app-local Android events through SDL and ControlDeck; not physical button presses");
            if (device != null) report.put("device", device.getName()).put("vendor", device.getVendorId()).put("product", device.getProductId());
            File pending = new File(folder, "controller-selftest.pending");
            Files.write(pending.toPath(), report.toString(2).getBytes(StandardCharsets.UTF_8));
            Files.move(pending.toPath(), new File(folder, "controller-selftest.json").toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        } catch (Exception e) { android.util.Log.e("KartControllerTest", "Report failed", e); }
    }
}
