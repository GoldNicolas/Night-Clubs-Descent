"""Long Bar (2x5 tiles) west of the Main Dance Hall: an upscale gold/amber cocktail lane.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_longbar().

Layout (cm): room x -800..0, y 1200..3200. Doors: E tiles 2-3 (y 2000..2800) = the Dance Hall's west door; N tile 1 (x -400..0) = service corridor.
  west : mirrored back bar with gold shelves of bottles; counter x -650..-530 (low cover 120), closed behind.
  lane : x -530..-20, stools at x -480 (pawn-only), >= 4 m clear to the east drink rail.
  east : drink rail (pawn-only) + glowing artwork either side of the door.
  north: jukebox + cigarette machine nook (box collision). south wall: neon martini sign.
"""
import math
import random
import unreal

N = "LongBar"
X0, Y0, TW, TD = -800, 1200, 2, 5
FL = "Rooms/LongBar/"
LL = "Lighting/LongBar"


def _clear_room():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Rooms/LongBar") or fp.startswith("Lighting/LongBar"):
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
    a = prop(name, loc, yaw=yaw, s=s, coll=coll, label=label or ("LongBar_" + name))
    a.set_folder_path(folder)
    return a


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"E": [(2, 3)], "N": [(1, 1)]}, trim="Neon_Gold", portal="Neon_Magenta")


def back_bar():
    """Mirror wall in gold frames, three gold shelves of bottles, warm backlight strips, cabinet base."""
    f = FL + "BackBar"
    box("LongBar_BackCabinet", f, -780, -720, 1300, 3100, 0, 100, "Counter_Black")
    box("LongBar_BackCabinetTop", f, -780, -712, 1300, 3100, 100, 104, "Gold", DECOR)
    for k in range(6):                                                       # six mirror bays
        y0 = 1300 + k * 300
        box("LongBar_Mirror", f, -780, -777, y0 + 8, y0 + 292, 120, 340, "Mirror_Smoke", DECOR)
        box("LongBar_MirrorFrame", f, -777, -772, y0, y0 + 8, 112, 348, "Gold", DECOR)
    box("LongBar_MirrorFrame", f, -777, -772, 3100, 3108, 112, 348, "Gold", DECOR)
    box("LongBar_MirrorFrame", f, -777, -772, 1300, 3108, 340, 348, "Gold", DECOR)
    box("LongBar_MirrorFrame", f, -777, -772, 1300, 3108, 112, 120, "Gold", DECOR)
    for z in (165, 230, 295):
        box("LongBar_Shelf", f, -776, -730, 1310, 3090, z - 2.5, z, "Gold", DECOR)
        box("LongBar_ShelfGlow", f, -732, -730, 1312, 3088, z - 2, z - 0.5, "Glow_Warm", DECOR)
    # bottles: three tiers, merged per tier
    rng = random.Random(77)
    for t, z in enumerate((104, 165, 230, 295)):
        p = []
        y = 1315
        while y < 3080:
            if (y - 1300) % 300 < 10:                                       # leave the frame posts clear
                y += 10
                continue
            k = rng.choice(list(BOTTLES))
            s = rng.uniform(1.1, 1.3) if z > 104 else rng.uniform(1.2, 1.4)
            ps, w = bottle(f, -752 + rng.uniform(-5, 5), y, z, k, rng, s, face=1)
            for a in ps:
                l = a.get_actor_location()
                a.set_actor_location(unreal.Vector(l.x, y + w / 2, l.z), False, False)
            p += ps
            y += w + rng.uniform(2, 6)
        merge(p, "LongBar_Bottles_%d" % t, "LongBar_Bottles_%d" % t, f)


def counter():
    """Counter with glowing amber onyx panels between black pilasters, stone top, gold edge and foot rail."""
    f = FL + "Counter"
    box("LongBar_Counter", f, -650, -530, 1400, 3000, 0, 112, "Counter_Black")
    box("LongBar_CounterTop", f, -660, -520, 1390, 3010, 112, 120, "Bar_Stone")
    box("LongBar_CounterEdge", f, -522, -519, 1390, 3010, 113, 119, "Gold", DECOR)
    for k in range(10):                                                      # onyx panels on the guest face
        y0 = 1400 + k * 160
        box("LongBar_Onyx", f, -530, -527, y0 + 14, y0 + 146, 14, 102, "Onyx_Amber", DECOR)
        box("LongBar_Pilaster", f, -530, -524, y0 - 6, y0 + 6, 0, 112, "Counter_Black", DECOR)
    box("LongBar_Pilaster", f, -530, -524, 2994, 3006, 0, 112, "Counter_Black", DECOR)
    # brass foot rail on short posts
    cyl_y = lambda lbl, x, y0, y1, z, d, m: mesh(lbl, f, CYL, (x, (y0 + y1) / 2, z), (d / 100, d / 100, (y1 - y0) / 100),
                                                rot=(0, 0, 90), material=m, kind=DECOR)
    cyl_y("LongBar_FootRail", -505, 1420, 2980, 22, 5, "Gold")
    for y in range(1450, 2990, 220):
        box("LongBar_FootRailPost", f, -527, -503, y - 1.5, y + 1.5, 15, 24, "Gold", DECOR)
    # returns close the bartender strip at both ends
    for (y0, y1) in ((1380, 1400), (3000, 3020)):
        box("LongBar_Return", f, -780, -530, y0, y1, 0, 112, "Counter_Black")
        box("LongBar_ReturnTop", f, -780, -520, y0 - 5, y1 + 5, 112, 120, "Bar_Stone")


def bar_dressing():
    f = FL + "Props"
    rng = random.Random(5)
    z = 120
    p = []
    for y in range(1480, 2980, 190):                                         # a drink setup in front of each stool
        p += rng.choice([_rocks, _highball, _martini, _wine_glass])(f, -560, y + rng.uniform(-15, 15), z)
        if rng.random() < 0.5:
            p.append(B(f, -575, y + 25, z, 9, 9, 0.3, "Label_White", yaw=rng.uniform(-25, 25)))       # napkin
    for y in (1700, 2500):                                                   # cocktail stations on the bartender side
        p += _shaker(f, -630, y, z) + _shaker(f, -630, y + 20, z)
        p += [B(f, -625, y + 60, z, 22, 36, 3, "Counter_Black")]
        for k in range(5):
            p.append(S(f, -630 + (k % 2) * 9, y + 48 + k * 6, z + 4, 4.5, rng.choice(["Fruit_Lime", "Glass_Amber", "Glass_Red"])))
    for y in (1550, 2150, 2750):                                             # candles
        p += candle(f, -545, y, z)
    merge(p, "LongBar_Dressing", "LongBar_Dressing", f)
    for y in (2100, 2900):
        _prop("BeerTap", (-610, y, z + 95 * 0.24), f, yaw=90, s=0.24)
    for y in (1850, 2350):
        _prop("LiquorBottles", (-635, y, z + 57 * 0.27), f, yaw=90, s=0.27)
    for k, y in enumerate(range(1480, 2980, 175)):
        _prop("BarStool", (-480, y, 95 * 0.42), f, yaw=(k * 47) % 360, s=0.42, coll="pawn")
    for y in range(1550, 3000, 290):
        _prop("PendantLight", (-590, y, 600 - 20 - 95 * 0.38), f, s=0.38)


def east_wall():
    """Drink rail (pawn-only) either side of the door, glowing art panels above it."""
    f = FL + "East"
    for (y0, y1) in ((1300, 1950), (2850, 3150)):
        r = box("LongBar_DrinkRail", f, -55, -20, y0, y1, 105, 110, "Gold")
        _pawn_only(r)
        box("LongBar_DrinkRailBracket", f, -30, -20, y0 + 10, y0 + 14, 85, 105, "Gold", DECOR)
        box("LongBar_DrinkRailBracket", f, -30, -20, y1 - 14, y1 - 10, 85, 105, "Gold", DECOR)
    art = [((1380, 1880), "Art_Pink"), ((2880, 3120), "Art_Cyan")]
    for (y0, y1), m in art:
        box("LongBar_ArtFrame", f, -26, -20, y0 - 8, y1 + 8, 172, 338, "Gold", DECOR)
        box("LongBar_Art", f, -28, -26, y0, y1, 180, 330, m, DECOR)
        # a few dark "brush strokes" over the glow so it reads as artwork, not a screen
        rng = random.Random(y0)
        for k in range(int((y1 - y0) / 40)):
            yy = rng.uniform(y0 + 30, y1 - 30)
            mesh("LongBar_ArtStroke", f, CUBE, (-29, yy, rng.uniform(200, 310)), (0.01, rng.uniform(0.25, 0.8), rng.uniform(0.03, 0.08)),
                 rot=(0, 0, rng.uniform(-50, 50)), material=rng.choice(["Counter_Black", "Counter_Black", "Gold"]), kind=DECOR)


def north_nook():
    """Jukebox and cigarette machine against the north wall (box collision = cover)."""
    f = FL + "Nook"
    # jukebox: body, glowing arch, grille, selection panel
    jx, jy = -680, 3135                                      # west half of the nook; the N door is x -400..0
    box("LongBar_Jukebox", f, jx - 40, jx + 40, jy - 30, jy + 30, 0, 140, "Counter_Black")
    # dome (dark) + thin glowing oval frame on the front; cylinder rolled 90: scale = (width, height, depth)
    mesh("LongBar_JukeboxArch", f, CYL, (jx, jy, 140), (0.8, 0.6, 0.6), rot=(0, 0, 90), material="Counter_Black", kind=DECOR)
    mesh("LongBar_JukeboxArchGlow", f, CYL, (jx, jy - 30.5, 140), (0.74, 0.52, 0.01), rot=(0, 0, 90), material="Neon_Magenta", kind=DECOR)
    mesh("LongBar_JukeboxArchInner", f, CYL, (jx, jy - 31.2, 140), (0.66, 0.44, 0.01), rot=(0, 0, 90), material="Counter_Black", kind=DECOR)
    box("LongBar_JukeboxWindow", f, jx - 26, jx + 26, jy - 31, jy - 30, 50, 102, "Glow_Warm", DECOR)
    box("LongBar_JukeboxGrille", f, jx - 30, jx + 30, jy - 31, jy - 30, 10, 42, "Chrome", DECOR)
    for k in range(5):
        box("LongBar_JukeboxBar", f, jx - 30, jx + 30, jy - 32, jy - 31, 13 + k * 6, 15 + k * 6, "Counter_Black", DECOR)
    for sx in (-38, 38):
        box("LongBar_JukeboxSide", f, jx + sx - 2, jx + sx + 2, jy - 31, jy - 29, 10, 135, "Neon_Gold", DECOR)
    # cigarette machine: tall box, lit front with rows of coloured packs
    cx, cy = -500, 3140
    box("LongBar_CigMachine", f, cx - 40, cx + 40, cy - 30, cy + 30, 0, 185, "Speaker_Black")
    box("LongBar_CigFront", f, cx - 32, cx + 32, cy - 31, cy - 30, 70, 175, "Glow_Warm", DECOR)
    rng = random.Random(9)
    p = []
    for r_ in range(5):
        for c_ in range(6):
            p.append(B(f, cx - 26 + c_ * 10.4, cy - 32, 78 + r_ * 19, 7, 1.5, 10,
                       rng.choice(["Sleeve_Red", "Label_White", "Gold", "Sleeve_Teal", "Label_Black"])))
    p += [B(f, cx, cy - 32, 25, 50, 2, 22, "Chrome"), B(f, cx + 22, cy - 33, 45, 8, 1, 14, "Plastic_Grey")]
    merge(p, "LongBar_CigPacks", "LongBar_CigPacks", f)


def martini_sign():
    """Neon martini glass on the south wall, facing north (viewer sees +X on the right; pitch + = rising right)."""
    f = FL + "Sign"
    wy, cx, zb = 1224, -400, 230
    box("LongBar_SignPlate", f, cx - 110, cx + 110, 1220, 1223, zb - 30, zb + 170, "Counter_Black", DECOR)
    w, h = 70, 60                                     # cone half-width / height
    ang = math.degrees(math.atan2(h, w))
    ln = math.hypot(w, h) / 100
    zt = zb + 60 + h
    for (dx, pitch) in ((-w / 2, -ang), (w / 2, ang)):
        mesh("LongBar_Martini", f, CUBE, (cx + dx, wy, zb + 60 + h / 2), (ln, 0.02, 0.035), rot=(pitch, 0, 0),
             material="Neon_Magenta", kind=DECOR)
    box("LongBar_Martini", f, cx - w, cx + w, wy - 1, wy + 1, zt - 1.75, zt + 1.75, "Neon_Magenta", DECOR)   # rim
    box("LongBar_Martini", f, cx - 1.75, cx + 1.75, wy - 1, wy + 1, zb + 3, zb + 60, "Neon_Magenta", DECOR)  # stem
    box("LongBar_Martini", f, cx - 35, cx + 35, wy - 1, wy + 1, zb, zb + 3.5, "Neon_Magenta", DECOR)       # base
    mesh("LongBar_MartiniOlive", f, CYL, (cx + 18, wy, zt - 22), (0.14, 0.14, 0.02), rot=(0, 0, 90),
         material="Neon_Gold", kind=DECOR)
    mesh("LongBar_MartiniPick", f, CUBE, (cx + 30, wy, zt - 8), (0.5, 0.02, 0.02), rot=(-35, 0, 0), material="Neon_Gold", kind=DECOR)


def lights():
    WARM, GOLD, MAG = (1, 0.65, 0.35), (1, 0.6, 0.25), (1, 0.1, 0.55)
    for y in (1700, 2300, 2900):
        light("point", "LongBar_Pendant", LL, (-590, y, 470), GOLD, 30, 520)
    light("rect", "LongBar_BackBarLight", LL, (-740, 2200, 230), WARM, 40, 700, rot=(0, 0, 0), source_width=1800.0, source_height=200.0)
    light("rect", "LongBar_OnyxLight", LL, (-515, 2200, 60), GOLD, 8, 450, rot=(0, 0, 0), source_width=1600.0, source_height=90.0)
    for y in (1600, 2850):
        light("spot", "LongBar_Downlight", LL, (-250, y, 590), WARM, 180, 800, rot=(-90, 0, 0),
              outer_cone_angle=40.0, inner_cone_angle=15.0, shadows=(y == 1600))
    light("point", "LongBar_JukeboxGlow", LL, (-680, 3060, 150), MAG, 15, 450)
    light("point", "LongBar_CorridorDoorGlow", LL, (-200, 3140, 380), MAG, 20, 500)
    light("rect", "LongBar_SignLight", LL, (-400, 1250, 330), MAG, 20, 600, rot=(0, 90, 0), source_width=220.0, source_height=200.0)
    light("rect", "LongBar_ArtLight", LL, (-40, 1630, 255), (1, 0.2, 0.7), 5, 450, rot=(0, 180, 0), source_width=500.0, source_height=150.0)


def build_longbar():
    r = {"cleared": _clear_room()}
    shell()
    back_bar()
    counter()
    bar_dressing()
    east_wall()
    north_nook()
    martini_sign()
    lights()
    fog_cards(N, -510, 1240, -40, 3160, layers=((8, 0.55), (30, 0.3)))
    spawn_marker(N, -250, 1450)
    spawn_marker(N, -250, 2950)
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/LongBar", "Lighting/LongBar"))])
    return r
