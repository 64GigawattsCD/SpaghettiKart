package dev.spaghettikart.phone;

public final class ShifterGateTest {
    private static void check(boolean result, String message) {
        if (!result) throw new AssertionError(message);
    }
    public static void main(String[] args) {
        ShifterGate gate = new ShifterGate();
        for (int gear=-1; gear<=6; ++gear) {
            gate.select(gear);
            check(gate.gear() == gear, "All detents");
            int mask=gear==-1 ? 0x800000 : gear==0 ? 0x1000000 : 1<<(16+gear);
            check(gate.button() == mask, "Native command masks");
        }
        gate.select(1);
        gate.move(1,-1);
        check(gate.x == -1 && gate.gear() == 1, "No lateral motion inside a slot");
        gate.move(1,1);
        check(gate.x == -1 && gate.y == 0, "Fast diagonal stops at neutral");
        gate.move(1,1);
        check(gate.x == 1 && gate.y == 0, "Crossbar motion");
        gate.move(1,1);
        check(gate.gear() == 6, "Sixth via neutral");
        gate.move(1,0);
        gate.move(1.85f,0);
        check(gate.gear() == -1, "Reverse side branch");
        gate.move(1.85f,-1);
        check(gate.y == 0, "No invented reverse vertical slot");
        gate.select(3); gate.move(0,-.4f); gate.settle();
        check(gate.gear() == 0 && gate.y == 0, "Partial shift releases to neutral");
        gate.select(1); gate.move(-.8f,-.35f);
        check(gate.y == 0, "Wide neutral corner tolerance");
        gate.move(.35f,0); gate.move(.35f,-.9f);
        check(gate.gear() == 3, "Off-center middle slot entry");
        gate.select(0);
        gate.move(Float.NaN,Float.POSITIVE_INFINITY);
        check(gate.x == 0 && gate.y == 0, "Nonfinite input ignored");
        java.util.Random random = new java.util.Random(42);
        for (int i=0; i<100000; ++i) {
            gate.move(random.nextFloat()*8-4,random.nextFloat()*8-4);
            check(gate.x>=-1 && gate.x<=1.85f && gate.y>=-1 && gate.y<=1, "Gate extents");
            check(gate.y==0 || gate.x==-1 || gate.x==0 || gate.x==1, "Every point on gate");
            check(gate.gear()>=-1 && gate.gear()<=6, "Gear range");
        }
        System.out.println("Gate detents, walls, neutral routing, reverse, release, masks and 100k movements passed");
    }
}
