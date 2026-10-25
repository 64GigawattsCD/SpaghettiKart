#include "../../src/port/LocalPartyDevices.h"
#include <cstdlib>
#include <iostream>

static int checks = 0;
static void check(bool condition) {
    ++checks;
    if (!condition) { std::cerr << "Party-device check failed: " << checks << '\n'; std::exit(1); }
}
int main() {
    using KartNet::PartyDevices::Assignments;
    Assignments slots;
    check(slots.Find(-1) == -1);
    check(!slots.Claim(-1, 10, "a", "A"));
    check(!slots.Claim(4, 10, "a", "A"));
    check(!slots.Claim(0, -1, "a", "A"));
    for (int i = 0; i < 4; ++i) {
        check(slots.Claim(i, 10+i, std::to_string(i), "Controller"));
        check(slots.Find(10+i) == i);
        check(slots.seats[i].consumeStart);
        for (int other = 0; other < 4; ++other) check(!slots.Claim(other, 10+i, "duplicate", "Duplicate"));
    }
    slots.Reconcile({10,12,13}, {true,true,true,true});
    check(slots.Find(11) == -1);
    check(slots.Find(10) == 0 && slots.Find(12) == 2 && slots.Find(13) == 3);
    check(slots.Reclaim("1") == 1);
    check(slots.Claim(1, 99, "1", "Reconnected"));
    check(slots.Find(99) == 1 && slots.Find(11) == -1);
    check(slots.ConsumeStart(1, true, 2));
    check(slots.ConsumeStart(1, false, 2));
    check(slots.ConsumeStart(1, false, 2));
    check(slots.ConsumeStart(1, false, 2));
    check(!slots.ConsumeStart(1, false, 2));
    slots.Reconcile({10,99,12,13}, {true,false,true,true});
    check(slots.Find(99) == -1 && slots.Reclaim("1") == -1);
    check(slots.seats[1].key.empty());
    slots.Reconcile({}, {true,true,true,true});
    for (int i = 0; i < 4; ++i) check(slots.seats[i].instance == -1);
    check(slots.Reclaim("0") == 0 && slots.Reclaim("2") == 2 && slots.Reclaim("3") == 3);
    for (int round = 0; round < 256; ++round) {
        Assignments table;
        for (int i = 0; i < 4; ++i) check(table.Claim(i, round*4+i, "same-guid", "Identical pads"));
        table.Reconcile({round*4+1,round*4+3}, {true,true,true,true});
        check(table.Reclaim("same-guid") == 0);
        check(table.Claim(0, 2000+round, "same-guid", "Replugged"));
        check(table.Reclaim("same-guid") == 2);
        check(table.Claim(2, 3000+round, "same-guid", "Replugged"));
        for (int a = 0; a < 4; ++a)
            for (int b = a+1; b < 4; ++b) check(table.seats[a].instance != table.seats[b].instance);
    }
    std::cout << checks << " party-device checks passed\n";
}
