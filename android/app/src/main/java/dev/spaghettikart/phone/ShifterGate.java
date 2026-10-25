package dev.spaghettikart.phone;

/** Normalized gate geometry, independent of screen density and Android events. */
final class ShifterGate {
    static final float CORNER = .5f;
    float x, y;

    void select(int gear) {
        if (gear == -1) { x = 1.85f; y = 0; }
        else if (gear >= 1 && gear <= 6) {
            x = (gear - 1) / 2 - 1;
            y = (gear & 1) == 1 ? -1 : 1;
        } else { x = 0; y = 0; }
    }

    void move(float targetX, float targetY) {
        if (!Float.isFinite(targetX) || !Float.isFinite(targetY)) return;
        if (y != 0) {
            // A vertical slot can only exit through its neutral intersection.
            y = y * targetY <= 0 || Math.abs(targetY) <= CORNER ? 0 : Math.max(-1, Math.min(1, targetY));
            return;
        }
        if (Math.abs(targetX - x) > CORNER) {
            x = Math.max(-1, Math.min(1.85f, targetX));
            return;
        }
        int column = Math.round(x);
        if (column >= -1 && column <= 1 && Math.abs(x-column) <= CORNER && Math.abs(targetY) > CORNER) {
            x = column;
            y = Math.max(-1, Math.min(1, targetY));
        }
    }

    int gear() {
        if (y == 0 && x >= 1.65f) return -1;
        if (Math.abs(y) < .8f) return 0;
        return (Math.round(x)+1)*2 + (y < 0 ? 1 : 2);
    }

    void settle() {
        int gear = gear();
        if (gear == 0) y = 0;
        else select(gear);
    }

    int button() {
        int gear = gear();
        return gear == -1 ? 0x00800000 : gear == 0 ? 0x01000000 : 1 << (16+gear);
    }
}
