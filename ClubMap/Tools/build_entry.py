"""Entry / Front Desk (4x3 tiles, south of the Main Dance Hall). Player start + first chokepoint.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_entry().

Layout (cm): room x 800..2400, y -1200..0. Doors: S tiles 1-2 (front doors, closed for now), N tiles 1-2 (-> Dance Hall).
  east  : one counter (low cover, 120) x 2030..2130; south half = front desk, north half = small bar.
          The strip behind it (x 2130..2380) is closed off at both ends (no narrow dead-end aisle for the AI).
  west  : coat check booth (counter + partition, closed pocket with coat racks) and a small waiting lounge.
  centre: carpet runner door-to-door, velvet ropes framing the front doors, bouncer podium.
Cover: counters 120 (low), coat-check partition 250 (high). Ropes/podium/chairs block pawns only (no Weapon/visibility).
"""
import math
import random
import unreal

N = "Entry"
X0, Y0, TW, TD = 800, -1200, 4, 3
FE = "Rooms/Entry/"
LE = "Lighting/Entry"


def _clear_room():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Rooms/Entry") or fp.startswith("Lighting/Entry"):
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


def _prop(name, loc, folder, yaw=0.0, s=1.0, coll="decor", label=None):
    a = prop(name, loc, yaw=yaw, s=s, coll=coll, label=label or ("Entry_" + name))
    a.set_folder_path(folder)
    return a


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"S": [(1, 2)], "N": [(1, 2)]},
                      trim="Neon_Magenta", portal="Neon_Cyan")


def front_doors():
    """Closed double doors in the front opening (x 1200..2000) until the tunnels outside exist. Structure collision."""
    f = FE + "Doors"
    for (a0, a1) in ((1205, 1598), (1602, 1995)):
        box("Entry_FrontDoor", f, a0, a1, -1206, -1194, 0, 400, "Counter_Black")
        box("Entry_DoorWindow", f, a0 + 150, a1 - 150, -1193, -1192, 220, 360, "Window_Street", DECOR)   # street light behind the glass
        box("Entry_DoorBar", f, a0 + 40, a1 - 40, -1190, -1186, 105, 111, "Chrome", DECOR)
        box("Entry_DoorKick", f, a0 + 10, a1 - 10, -1193, -1192, 0, 25, "Chrome", DECOR)
    box("Entry_DoorSeam", f, 1598, 1602, -1192, -1191, 0, 400, "Neon_Cyan", DECOR)


def counter():
    """East counter: front desk (south) + small bar (north). Low cover 120 cm, closed behind."""
    f = FE + "Counter"
    box("Entry_Counter", f, 2030, 2130, -1180, -100, 0, 112, "Counter_Black")
    box("Entry_CounterTop", f, 2020, 2140, -1180, -100, 112, 120, "Bar_Stone")
    box("Entry_CounterNeon", f, 2026, 2030, -1170, -110, 12, 18, "Neon_Magenta", DECOR)
    box("Entry_CounterNeon", f, 2026, 2030, -1170, -110, 96, 102, "Neon_Magenta", DECOR)
    # return at the north end closes the strip behind the counter
    box("Entry_CounterReturn", f, 2130, 2380, -120, -100, 0, 112, "Counter_Black")
    box("Entry_CounterReturnTop", f, 2130, 2380, -125, -95, 112, 120, "Bar_Stone")
    # back wall: club sign behind the desk, bottle shelves behind the bar
    box("Entry_SignPanel", f, 2366, 2380, -1080, -660, 150, 350, "Wall_Panel", DECOR)
    # diamond logo: four neon bars (roll +-45) around (y -870, z 250)
    cy, cz, r = -870, 250, 70
    for (dy, dz, roll) in ((-r / 2, r / 2, -45), (r / 2, r / 2, 45), (-r / 2, -r / 2, 45), (r / 2, -r / 2, -45)):   # UE roll sign
        mesh("Entry_Logo", f, CUBE, (2364, cy + dy, cz + dz), (0.03, r * 1.414 / 100, 0.05), rot=(0, 0, roll),
             material="Neon_Magenta", kind=DECOR)
    for (dy, dz, roll) in ((-r / 4, r / 4, -45), (r / 4, r / 4, 45), (-r / 4, -r / 4, 45), (r / 4, -r / 4, -45)):
        mesh("Entry_Logo", f, CUBE, (2363, cy + dy, cz + dz), (0.03, r * 0.707 / 100, 0.04), rot=(0, 0, roll),
             material="Neon_Cyan", kind=DECOR)
    box("Entry_SignLine", f, 2363, 2365, -1060, -680, 160, 164, "Neon_Gold", DECOR)
    box("Entry_SignLine", f, 2363, 2365, -1060, -680, 336, 340, "Neon_Gold", DECOR)
    # bar shelves + glow
    box("Entry_BarGlow", f, 2376, 2380, -560, -140, 130, 300, "Bottles_Glow", DECOR)
    for z in (150, 215):
        box("Entry_BarShelf", f, 2330, 2376, -560, -140, z - 3, z, "Counter_Black", DECOR)
        box("Entry_BarShelfLED", f, 2328, 2330, -558, -142, z - 2.5, z - 1, "Neon_Gold", DECOR)


def desk_and_bar_dressing():
    f = FE + "Props"
    rng = random.Random(41)
    z = 120
    p = []
    # front desk: monitor facing the staff (+X), guests see its back, keyboard, tablet, card reader, bell, clipboard, lamp base
    p += [B(f, 2100, -900, z, 16, 18, 1.5, "Plastic_Grey"), B(f, 2100, -900, z + 1.5, 4, 4, 22, "Plastic_Grey"),
          B(f, 2097, -900, z + 20, 2.5, 52, 32, "Plastic_Grey"), B(f, 2098.5, -900, z + 21.5, 0.4, 49, 29, "Screen_Laptop")]   # screen faces the staff side
    p += [B(f, 2115, -900, z, 15, 44, 1.5, "Label_Black")]                                   # keyboard, staff side
    p += [B(f, 2070, -1020, z, 18, 24, 1, "Label_Black"), B(f, 2070, -1020, z + 1, 16, 22, 0.2, "Screen_Laptop")]
    p += [B(f, 2060, -780, z, 7, 12, 4, "Plastic_Grey"), B(f, 2058, -780, z + 4, 4, 6, 0.4, "Screen_Laptop")]
    p += [C(f, 2085, -720, z, 2, 8, "Gold"), S(f, 2085, -720, z + 3, 7, "Gold", 5)]
    p += [B(f, 2070, -1110, z, 23, 31, 1, "Label_Black"), B(f, 2070, -1110, z + 1, 21, 28, 0.3, "Label_White"),
          B(f, 2070, -1124, z + 1.3, 6, 3, 1.5, "Chrome")]
    # bar half: bottles on the shelves (merged), glasses + tap on the counter
    for zz in (150, 215):
        y = -550
        while y < -150:
            k = rng.choice(list(BOTTLES))
            ps, w = bottle(f, 2352 + rng.uniform(-4, 4), y, zz, k, rng, rng.uniform(1.15, 1.3), face=-1)
            for a in ps:
                l = a.get_actor_location()
                a.set_actor_location(unreal.Vector(l.x, y + w / 2, l.z), False, False)
            p += ps
            y += w + rng.uniform(2, 5)
    for (y, fn) in ((-470, _rocks), (-430, _highball), (-230, _martini), (-190, _wine_glass)):
        p += fn(f, 2075, y, z)
    merge(p, "Entry_Dressing", "Entry_Dressing", f)
    _prop("BeerTap", (2090, -330, z + 95 * 0.24), f, yaw=-90, s=0.24)
    _prop("Ashtray", (2060, -640, z + 95 * 0.06), f, s=0.06)
    for y in (-450, -220):
        _prop("BarStool", (1975, y, 95 * 0.42), f, yaw=rng.uniform(0, 360), s=0.42, coll="pawn")
    for y in (-980, -760):
        _prop("PendantLight", (2085, y, 600 - 20 - 95 * 0.38), f, s=0.38)


def coat_check():
    """West booth: counter (low cover) + partition (high cover) close a pocket full of coat racks."""
    f = FE + "CoatCheck"
    box("Entry_CoatCounter", f, 1100, 1180, -1180, -580, 0, 112, "Counter_Black")
    box("Entry_CoatCounterTop", f, 1092, 1186, -1180, -580, 112, 120, "Bar_Stone")
    box("Entry_CoatNeon", f, 1186, 1190, -1170, -590, 12, 18, "Neon_Cyan", DECOR)
    box("Entry_CoatPartition", f, 820, 1186, -580, -560, 0, 250, "Wall_Panel")           # high cover
    box("Entry_CoatPartitionNeon", f, 820, 1186, -582, -578, 244, 250, "Neon_Cyan", DECOR)
    # neon coat-hanger icon mounted on the booth's back wall (x 820), above the coats and backlight, facing the lane (+X)
    # UE roll: seen from +X, the left bar (-Y) takes -ang and the right bar (+Y) +ang (checked in game)
    wx, cy, zb, zt, hw = 826, -880, 335, 372, 62
    box("Entry_CoatSignPlate", f, 820, 824, cy - hw - 25, cy + hw + 25, zb - 22, zt + 40, "Counter_Black", DECOR)
    ang = math.degrees(math.atan2(zt - zb, hw))
    ln = math.hypot(hw, zt - zb) / 100
    for (dy, roll) in ((-hw / 2, -ang), (hw / 2, ang)):
        mesh("Entry_CoatSign", f, CUBE, (wx, cy + dy, (zb + zt) / 2), (0.02, ln, 0.035), rot=(0, 0, roll),
             material="Neon_Cyan", kind=DECOR)
    box("Entry_CoatSign", f, wx - 1, wx + 1, cy - hw, cy + hw, zb - 1.75, zb + 1.75, "Neon_Cyan", DECOR)    # bottom bar
    box("Entry_CoatSign", f, wx - 1, wx + 1, cy - 1.75, cy + 1.75, zt, zt + 14, "Neon_Cyan", DECOR)          # hook stem
    box("Entry_CoatSign", f, wx - 1, wx + 1, cy - 1.75, cy + 9, zt + 12.5, zt + 16, "Neon_Cyan", DECOR)      # hook top
    box("Entry_CoatSign", f, wx - 1, wx + 1, cy + 7.5, cy + 11, zt + 7, zt + 16, "Neon_Cyan", DECOR)         # hook tip
    # two chrome rails of hanging coats (procedural, merged): long coats, jackets, a few empty hangers
    rng = random.Random(7)
    coats = []
    for x in (900, 1020):
        coats.append(B(f, x, -870, 185, 3, 560, 3, "Chrome"))                                 # rail
        for yy in (-1150, -590):
            coats.append(B(f, x, yy, 0, 4, 4, 188, "Chrome"))                                 # uprights
        y = -1130
        while y < -610:
            if rng.random() < 0.12:                                                           # empty hanger
                coats.append(B(f, x, y, 172, 1, 40, 1.5, "Chrome", yaw=rng.uniform(-8, 8)))
                y += 9
                continue
            length = rng.choice([95, 110, 125])
            col = rng.choice(["Fabric_Black", "Fabric_Black", "Fabric_Camel", "Fabric_Navy", "Fabric_Wine", "Fabric_Grey"])
            yaw = rng.uniform(-12, 12)
            coats.append(B(f, x, y, 180 - length, 9, 48, length - 12, col, yaw=yaw))         # body
            coats.append(B(f, x, y, 168, 11, 42, 10, col, yaw=yaw))                           # shoulders
            coats.append(B(f, x, y, 176, 1, 4, 9, "Chrome"))                                  # hook
            y += rng.uniform(11, 15)
    merge(coats, "Entry_Coats", "Entry_Coats", f)
    box("Entry_CoatBacklight", f, 820, 824, -1160, -600, 120, 300, "Glow_Warm", DECOR)          # warm wall glow behind the coats
    # little bell + ticket stubs on the counter
    p = [C(f, 1140, -700, 120, 2, 8, "Gold"), S(f, 1140, -700, 123, 7, "Gold", 5)]
    for k in range(6):
        p.append(B(f, 1130 + (k % 2) * 6, -900 + k * 3, 120, 5, 3, 0.3 + k * 0.2, "Sleeve_Red"))
    merge(p, "Entry_CoatBits", "Entry_CoatBits", f)


def lounge_and_entrance():
    f = FE + "Lounge"
    # waiting lounge (north-west)
    _prop("LoungeArmchair", (1010, -430, 95 * 0.45), f, yaw=90, s=0.45, coll="pawn")
    _prop("LoungeArmchair", (1010, -120, 95 * 0.45), f, yaw=-90, s=0.45, coll="pawn")
    _prop("CoffeeTable", (1010, -275, 38 * 0.4), f, s=0.4, coll="pawn")
    _prop("Ashtray", (1030, -280, 76 * 0.4 + 95 * 0.06), f, s=0.06)
    _prop("PendantLight", (1010, -275, 600 - 20 - 95 * 0.38), f, s=0.38)
    p = rug(f, 860, 1170, -540, -40, 0, "Rug_Teal", "Neon_Cyan") + candle(f, 990, -265, 76 * 0.4)
    # carpet runner from the front doors to the dance hall
    p += rug(f, 1380, 1820, -1175, -25, 0, "Rug_Purple", "Gold")
    merge(p, "Entry_Rugs", "Entry_Rugs", f)
    # velvet ropes framing the front doors (pawn-blocking only), bouncer podium
    fr = FE + "Ropes"
    for x in (1260, 1940):
        posts = [-1150, -1050, -950]
        for y in posts:
            cyl("Entry_RopePost", fr, x, y, 0, 95, 6, "Gold", DECOR)
            cyl("Entry_RopePostBase", fr, x, y, 0, 3, 28, "Gold", DECOR)
            mesh("Entry_RopePostTop", fr, SPHERE, (x, y, 98), (0.09, 0.09, 0.09), material="Gold", kind=DECOR)
        for (y0, y1) in zip(posts, posts[1:]):
            mesh("Entry_Rope", fr, CYL, (x, (y0 + y1) / 2, 82), (0.035, 0.035, (y1 - y0) / 100), rot=(0, 0, 90),
                 material="Leather_Wine", kind=DECOR)
        blk = box("Entry_RopeBlock", fr, x - 10, x + 10, -1180, -950, 0, 100, "Gold")
        blk.static_mesh_component.set_editor_property("visible", False)
        _pawn_only(blk)
    _prop("BouncerPodium", (1340, -820, 95 * 0.6), fr, yaw=-90, s=0.6, coll="pawn")
    # security: cameras in two corners, wall speakers
    _prop("SecurityCam", (845, -1175, 560), fr, yaw=-45, s=0.16)     # lens (+Y local) looks north-east
    _prop("SecurityCam", (2355, -45, 560), fr, yaw=135, s=0.16)     # looks south-west
    for (x, y, yaw) in ((825, -800, -90), (2375, -760, 90)):
        _prop("WallSpeaker", (x + (17 if x < 1600 else -17), y, 430), fr, yaw=yaw, s=0.28)


def lights():
    WARM, MAG, CYAN = (1, 0.72, 0.45), (1, 0.1, 0.55), (0.15, 0.75, 1)
    for y in (-950, -560, -180):                                   # recessed downlights along the runner
        light("spot", "Entry_Downlight", LE, (1600, y, 590), WARM, 260, 800, rot=(-90, 0, 0),
              outer_cone_angle=38.0, inner_cone_angle=15.0, shadows=(y == -560))
    light("rect", "Entry_SignLight", LE, (2350, -870, 250), MAG, 40, 700, rot=(0, 180, 0), source_width=420.0, source_height=200.0)
    light("rect", "Entry_BarGlowLight", LE, (2355, -350, 215), CYAN, 30, 600, rot=(0, 180, 0), source_width=420.0, source_height=170.0)
    for y in (-980, -760):
        light("point", "Entry_Pendant", LE, (2085, y, 470), (1, 0.6, 0.25), 25, 450)
    light("point", "Entry_CoatGlow", LE, (980, -870, 300), CYAN, 10, 500)
    light("spot", "Entry_CoatLight", LE, (960, -880, 480), WARM, 300, 700, rot=(-90, 0, 0), outer_cone_angle=55.0, inner_cone_angle=25.0)
    light("rect", "Entry_CoatBackLight", LE, (840, -880, 210), (1, 0.6, 0.25), 12, 500, rot=(0, 0, 0), source_width=560.0, source_height=180.0)
    light("point", "Entry_LoungeLamp", LE, (1010, -275, 300), WARM, 45, 520)
    light("rect", "Entry_StreetGlow", LE, (1600, -1185, 300), (0.35, 0.5, 1.0), 25, 700, rot=(0, 90, 0), source_width=700.0, source_height=150.0)
    light("point", "Entry_LoungeCandle", LE, (990, -265, 70), (1, 0.5, 0.18), 10, 300)
    light("point", "Entry_DoorGlow", LE, (1600, -1140, 380), CYAN, 25, 600)


def player_start():
    """Move (or create) the PlayerStart to the entry, facing the dance hall."""
    ps = [a for a in _EAS.get_all_level_actors() if a.get_class().get_name() == "PlayerStart"]
    a = ps[0] if ps else _EAS.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 0))
    a.modify()
    a.set_actor_location(unreal.Vector(1600, -1000, 100), False, False)
    a.set_actor_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0), False)
    a.set_folder_path("Setup")
    return a.get_actor_label()


def build_entry():
    r = {"cleared": _clear_room()}
    shell()
    front_doors()
    counter()
    desk_and_bar_dressing()
    coat_check()
    lounge_and_entrance()
    lights()
    fog_cards(N, 840, -1160, 2010, -40, layers=((8, 0.6), (30, 0.35)))
    spawn_marker(N, 1000, -280)        # waiting lounge, ~9.6 m from the player start
    r["player_start"] = player_start()
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/Entry", "Lighting/Entry"))])
    return r
