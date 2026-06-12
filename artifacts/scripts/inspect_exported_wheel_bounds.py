import re
from collections import defaultdict

path = r"E:\SpaghettiKart\src\first_person_kart_wheels.inc.c"
pattern = re.compile(
    r"\{\{ \{ (?P<x>-?\d+), (?P<y>-?\d+), (?P<z>-?\d+) \}, 0, \{ 0, 0 \}, "
    r"\{ 0x(?P<r>[0-9A-F]{2}), 0x(?P<g>[0-9A-F]{2}), 0x(?P<b>[0-9A-F]{2}), 0x(?P<a>[0-9A-F]{2}) \} \}\}"
)

current = None
stats = defaultdict(lambda: {"count": 0, "mins": [10**9, 10**9, 10**9], "maxs": [-10**9, -10**9, -10**9]})

for line in open(path, encoding="utf-8"):
    if "sFirstPersonKartFrontWheelVtx" in line:
        current = "front"
    elif "sFirstPersonKartRearWheelVtx" in line:
        current = "rear"
    match = pattern.search(line)
    if not match or current is None:
        continue
    x = int(match.group("x"))
    y = int(match.group("y"))
    z = int(match.group("z"))
    color = (int(match.group("r"), 16), int(match.group("g"), 16), int(match.group("b"), 16))
    key = (current, color)
    stats[key]["count"] += 1
    for i, value in enumerate((x, y, z)):
        stats[key]["mins"][i] = min(stats[key]["mins"][i], value)
        stats[key]["maxs"][i] = max(stats[key]["maxs"][i], value)

for (wheel, color), stat in sorted(stats.items()):
    print(
        f"{wheel} color={color} count={stat['count']} "
        f"mins={tuple(stat['mins'])} maxs={tuple(stat['maxs'])}"
    )
