"""Billiards (4x3 tiles) east of the Main Dance Hall: a mid-size cover maze of pool tables.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_billiards().

Layout (cm): room x 3200..4800, y 800..2000.
Doors: W tiles 1-2 (y 1200..2000) = the Dance Hall's east door; N tiles 0-1 (x 3200..4000) -> VIP Lounge.
Tables (long axis X, 280 x 155, 85 high, box collision): A (3800,1400) green, B (4350,1100) blue, C (4350,1700) wine.
Gaps between tables >= 2.7 m, open entry zone in the west, clear walk from the W door to the N door along the north-west.
"""
import math
import random
import unreal

N = "Billiards"
X0, Y0, TW, TD = 3200, 800, 4, 3
FB = "Rooms/Billiards/"
LB = "Lighting/Billiards"
TABLES = (("A", 3800, 1400, "Felt_Green", "rack"), ("B", 4350, 1100, "Felt_Blue", "game"), ("C", 4350, 1700, "Felt_Wine", "game"))
BALLS = ["Ball_Yellow", "Ball_Blue", "Ball_Red", "Ball_Purple", "Ball_Orange", "Ball_Green", "Ball_Maroon", "Ball_Black",
         "Ball_Yellow", "Ball_Blue", "Ball_Red", "Ball_Purple", "Ball_Orange", "Ball_Green", "Ball_Maroon"]


def _clear_room():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Rooms/Billiards") or fp.startswith("Lighting/Billiards"):
            _EAS.destroy_actor(a)
            n += 1
    return n


def _pawn_only(a):
    c = a.static_mesh_component
    c.set_collision_profile_name("Custom")
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    c.set_collision_object_type(unreal.CollisionChannel.ECC_PHYSICS_BODY)
    c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponseType.ECR_BLOCK)
    c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_AGENT_PAWN, unreal.CollisionResponseType.ECR_BLOCK)


def _prop(name, loc, folder, yaw=0.0, s=1.0, sxy=None, sz=None, coll="decor", label=None):
    a = prop(name, loc, yaw=yaw, s=s, sxy=sxy, sz=sz, coll=coll, label=label or ("Billiards_" + name))
    a.set_folder_path(folder)
    return a


def _hidden_struct(label, f, x0, x1, y0, y1, z0, z1):
    a = box(label, f, x0, x1, y0, y1, z0, z1, "Counter_Black")
    a.static_mesh_component.set_editor_property("visible", False)
    a.static_mesh_component.set_editor_property("cast_shadow", False)
    return a


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"W": [(1, 2)], "N": [(0, 1)]}, trim="Neon_Cyan", portal="Neon_Magenta")


def pool_table(tag, cx, cy, felt, state, rng):
    """Visual table merged into one mesh + an invisible box collider (85 cm, low cover)."""
    f = FB + "Tables"
    L, W = 280.0, 155.0
    hx, hy = L / 2, W / 2
    p = []
    for (lx, ly) in ((-hx + 18, -hy + 18), (hx - 18, -hy + 18), (-hx + 18, hy - 18), (hx - 18, hy - 18), (0, -hy + 18), (0, hy - 18)):
        p.append(B(f, cx + lx, cy + ly, 0, 16, 16, 20, "Wood_Dark"))                                   # legs
        p.append(B(f, cx + lx, cy + ly, 0, 20, 20, 3, "Gold"))                                         # brass feet
    p.append(B(f, cx, cy, 20, L - 24, W - 24, 52, "Wood_Dark"))                                         # cabinet / apron
    p.append(B(f, cx, cy, 58, L - 22, W - 22, 2, "Gold"))                                               # brass apron line
    p.append(B(f, cx, cy, 72, L - 28, W - 28, 5, felt))                                                 # bed
    for (bx, by, bw, bd) in ((0, -hy + 7, L, 14), (0, hy - 7, L, 14), (-hx + 7, 0, 14, W), (hx - 7, 0, 14, W)):
        p.append(B(f, cx + bx, cy + by, 72, bw, bd, 13, "Wood_Dark"))                                   # rails
    for (bx, by, bw, bd) in ((0, -hy + 16, L - 32, 4), (0, hy - 16, L - 32, 4), (-hx + 16, 0, 4, W - 32), (hx - 16, 0, 4, W - 32)):
        p.append(B(f, cx + bx, cy + by, 77, bw, bd, 6, felt))                                           # cushions
    for (px, py) in ((-hx + 15, -hy + 15), (hx - 15, -hy + 15), (-hx + 15, hy - 15), (hx - 15, hy - 15), (0, -hy + 13), (0, hy - 13)):
        p.append(C(f, cx + px, cy + py, 76.5, 1.2, 13, "Counter_Black"))                                # pockets
        p.append(C(f, cx + px, cy + py, 84.6, 0.6, 15, "Chrome"))                                       # pocket rims
    for k in (1, 2, 3):                                                                                 # rail sights
        for sy in (-1, 1):
            p.append(B(f, cx - hx + k * L / 4, cy + sy * (hy - 7), 85, 2, 1.5, 0.3, "Label_Cream"))
    z = 77 + 2.85
    if state == "rack":
        x0 = cx + L / 4
        order = rng.sample(BALLS, len(BALLS))
        order[4] = "Ball_Black"
        k = 0
        for r in range(5):
            for i in range(r + 1):
                p.append(S(f, x0 + r * 4.94, cy + (i - r / 2) * 5.7, z, 5.7, order[k]))
                k += 1
        p.append(B(f, x0 + 10, cy, 77, 28, 1, 0.6, "Wood_Dark", yaw=0))                               # rack frame hint
        p.append(S(f, cx - L / 4, cy, z, 5.7, "Ball_White"))
    else:
        for k in range(rng.randint(6, 10)):
            p.append(S(f, cx + rng.uniform(-hx + 30, hx - 30), cy + rng.uniform(-hy + 30, hy - 30), z, 5.7, rng.choice(BALLS)))
        p.append(S(f, cx + rng.uniform(-60, 60), cy + rng.uniform(-30, 30), z, 5.7, "Ball_White"))
        ang = rng.uniform(-30, 30)                                                                      # cue lying across the table
        p.append(mesh("p", f, CYL, (cx - 20, cy + 10, 81), (0.025, 0.025, 1.47), rot=(90, ang, 0), material="Wood_Dark", kind=DECOR))
    for (chx, chy) in ((cx - hx + 6, cy - hy + 6), (cx + hx - 6, cy + hy - 6)):                         # chalk on the rails
        p.append(B(f, chx, chy, 85, 2.2, 2.2, 2.2, "Chalk_Blue"))
    merge(p, "Billiards_Table" + tag, "Billiards_Table" + tag, f)
    _hidden_struct("Billiards_TableCollider", f, cx - hx, cx + hx, cy - hy, cy + hy, 0, 85)
    # hanging billiard lamp: long green shade, three warm bulbs, two chains
    lp = [B(f, cx, cy, 180, 190, 38, 18, "Shade_Green"), B(f, cx, cy, 197, 196, 42, 3, "Gold")]   # ~95 cm above the felt
    for dx in (-60, 0, 60):
        lp.append(C(f, cx + dx, cy, 179, 1.5, 14, "Glow_Warm"))
    for dx in (-80, 80):
        lp.append(B(f, cx + dx, cy, 200, 1.2, 1.2, 400, "Chrome"))                               # chains to the ceiling
    merge(lp, "Billiards_Lamp" + tag, "Billiards_Lamp" + tag, f)


def walls_dressing():
    f = FB + "Walls"
    rng = random.Random(12)
    p = []
    # cue rack on the south wall: board, brass hooks, eight cues leaning on it
    p.append(B(f, 3700, 823, 50, 330, 6, 170, "Wood_Dark"))
    p.append(B(f, 3700, 826, 160, 320, 4, 3, "Gold"))
    for k in range(8):
        x = 3560 + k * 40
        p.append(mesh("p", f, CYL, (x, 836, 88), (0.026, 0.026, 1.47), rot=(0, 0, -4.7), material=rng.choice(["Wood_Dark", "Label_Black", "Wood_Dark"]),
                      kind=DECOR))
        p.append(C(f, x, 840, 0, 2, 4, "Gold"))
    # second rack on the east wall
    p.append(B(f, 4777, 1400 - 520, 50, 6, 220, 170, "Wood_Dark"))
    for k in range(5):
        y = 1400 - 600 + k * 40
        p.append(mesh("p", f, CYL, (4766, y, 88), (0.026, 0.026, 1.47), rot=(4.7, 0, 0) if False else (0, 90, -4.7),
                      material="Wood_Dark", kind=DECOR))
    # bead scoreboard (abacus) on the north wall
    for k, z in enumerate((205, 225)):
        p.append(B(f, 4400, 1977, z, 260, 1, 0.8, "Chrome"))
        for i in range(14):
            p.append(C(f, 4290 + i * 9 + (40 if i > 6 else 0), 1974, z - 2, 5, 4.5, "Ball_Red" if (i + k) % 3 else "Label_Cream"))
    p.append(B(f, 4400, 1978, 190, 280, 2, 55, "Wood_Dark"))
    merge(p, "Billiards_WallBits", "Billiards_WallBits", f)
    # dartboard on the east wall with a neon ring (faces -X)
    dz, dy = 175, 1450
    for (d, m, dx) in ((56, "Neon_Magenta", 0), (46, "Counter_Black", 0.6), (40, "Ball_Red", 1.2), (34, "Label_Cream", 1.8),
                       (24, "Felt_Green", 2.4), (16, "Label_Cream", 3.0), (5, "Ball_Red", 3.6)):
        mesh("Billiards_Dart", f, CYL, (4778 - dx, dy, dz), (d / 100, d / 100, 0.006), rot=(90, 0, 0), material=m, kind=DECOR)
    # neon 8-ball sign on the south wall (faces +Y)
    sx, sz = 4300, 330
    for (d, m, dy) in ((84, "Neon_Magenta", 0), (78, "Counter_Black", 0.6), (34, "Neon_White", 1.2)):
        mesh("Billiards_8Ball", f, CYL, (sx, 824 + dy, sz), (d / 100, d / 100, 0.006), rot=(0, 0, 90), material=m, kind=DECOR)
    for (d0, d1, zz) in ((11, 6, 8), (13, 8, -6)):
        mesh("Billiards_8Ball", f, CYL, (sx, 826, sz + zz), (d0 / 100, d0 / 100, 0.006), rot=(0, 0, 90), material="Counter_Black", kind=DECOR)
        mesh("Billiards_8Ball", f, CYL, (sx, 826.6, sz + zz), (d1 / 100, d1 / 100, 0.006), rot=(0, 0, 90), material="Neon_White", kind=DECOR)


def seating():
    f = FB + "Seating"
    # spectator bench on the south wall (pawn-only)
    seat = box("Billiards_Bench", f, 3960, 4640, 822, 885, 0, 45, "Leather_Wine")
    _pawn_only(seat)
    box("Billiards_BenchBack", f, 3960, 4640, 822, 842, 45, 100, "Leather_Wine", DECOR)
    box("Billiards_BenchTrim", f, 3960, 4640, 884, 886, 4, 8, "Gold", DECOR)
    for x in (3970, 4630):
        box("Billiards_BenchArm", f, x - 8, x + 8, 822, 885, 0, 65, "Wood_Dark", DECOR)
    rng = random.Random(4)
    for (x, y) in ((3420, 960), (3720, 960)):
        _prop("CocktailTable", (x, y, 94 * 0.58), f, sxy=0.4, sz=0.58, coll="pawn")
        _prop("Ashtray", (x + 12, y - 8, 109 + 95 * 0.06), f, s=0.06)
        for a in (-60, 120):
            _prop("BarStool", (x + 55 * math.cos(math.radians(a)), y + 55 * math.sin(math.radians(a)), 95 * 0.42), f,
                  yaw=rng.uniform(0, 360), s=0.42, coll="pawn")
    p = []
    for (x, y) in ((3420, 960), (3720, 960)):
        p += _highball(f, x - 15, y + 10, 109) + _rocks(f, x + 5, y + 18, 109)
    merge(p, "Billiards_Drinks", "Billiards_Drinks", f)
    _prop("WallSpeaker", (3237, 1000, 430), f, yaw=-90, s=0.28)
    _prop("WallSpeaker", (4763, 1800, 430), f, yaw=90, s=0.28)
    _prop("SecurityCam", (4755, 845, 560), f, yaw=45, s=0.16)


def lights():
    WARM, MAG = (1, 0.72, 0.42), (1, 0.1, 0.55)
    for (tag, cx, cy, _, _) in TABLES:
        light("spot", "Billiards_TableLight", LB, (cx, cy, 176), WARM, 160, 420, rot=(-90, 0, 0),
              outer_cone_angle=62.0, inner_cone_angle=40.0, shadows=(tag == "A"))
    light("rect", "Billiards_SignLight", LB, (4300, 860, 330), MAG, 12, 450, rot=(0, 90, 0), source_width=120.0, source_height=120.0)
    light("point", "Billiards_DartLight", LB, (4720, 1450, 230), MAG, 8, 300)
    light("rect", "Billiards_RackWash", LB, (3700, 900, 300), WARM, 25, 500, rot=(-60, -90, 0), source_width=350.0, source_height=80.0)
    light("point", "Billiards_DoorGlow", LB, (3280, 1600, 380), MAG, 20, 500)


def build_billiards():
    r = {"cleared": _clear_room()}
    shell()
    rng = random.Random(8)
    for (tag, cx, cy, felt, state) in TABLES:
        pool_table(tag, cx, cy, felt, state, rng)
    walls_dressing()
    seating()
    lights()
    fog_cards(N, 3240, 840, 4760, 1960, layers=((8, 0.5), (30, 0.3)))
    spawn_marker(N, 3550, 1800)
    spawn_marker(N, 4650, 1400)
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/Billiards", "Lighting/Billiards"))])
    return r
