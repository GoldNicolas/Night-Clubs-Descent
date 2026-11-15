"""Back-of-house band north of the Main Dance Hall + Long Bar.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_backofhouse().

Service corridor  x -800..3200, y 3200..3600 (1x10). North doors at corridor tiles 1,3,5,7,9 (spaced so no two touch).
  Private Room A  x -800..0,    y 3600..4400 (2x2)  door S tile 1 (x -400..0)    red velvet
  Private Room B  x 0..800,     y 3600..4400 (2x2)  door S tile 1 (x 400..800)   teal velvet
  Restroom M      x 800..1600,  y 3600..4800 (2x3)  door S tile 1 (x 1200..1600)
  Restroom W      x 1600..2400, y 3600..4800 (2x3)  door S tile 1 (x 2000..2400)
  Storage         x 2400..3200, y 3600..4800 (2x3)  door S tile 1 (x 2800..3200)
The corridor's west end sits above the Long Bar (link later: corridor S tile 1 <-> Long Bar N tile 1) and its east end
touches the VIP west wall at y 3200..3600 (link later: corridor E <-> VIP W tile 3).
Corridor interior is 3.6 m (1 tile minus walls): clutter hugs the walls so ~3 m stays clear.
"""
import math
import random
import unreal

FH = "Rooms/BackOfHouse/"
LH = "Lighting/BackOfHouse"
ROOMS = ("Corridor", "PrivateA", "PrivateB", "RestroomM", "RestroomW", "Storage")


def _clear():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if any(fp.startswith("Rooms/%s" % r) or fp.startswith("Lighting/%s" % r) for r in ROOMS) or \
           fp.startswith("Rooms/BackOfHouse") or fp.startswith(LH):
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


def _hidden(label, f, x0, x1, y0, y1, z0, z1, pawn=False):
    a = box(label, f, x0, x1, y0, y1, z0, z1, "Counter_Black")
    a.static_mesh_component.set_editor_property("visible", False)
    a.static_mesh_component.set_editor_property("cast_shadow", False)
    if pawn:
        _pawn_only(a)
    return a


def _prop(name, loc, folder, yaw=0.0, s=1.0, sxy=None, sz=None, coll="decor", label=None):
    a = prop(name, loc, yaw=yaw, s=s, sxy=sxy, sz=sz, coll=coll, label=label or ("BOH_" + name))
    a.set_folder_path(folder)
    return a


def _disc(label, f, x, y, z, d, m, axis):
    """Flat disc facing an axis: 'x' (wall normal along X) or 'y'."""
    rot = (90, 0, 0) if axis == "x" else (0, 0, 90)
    return mesh(label, f, CYL, (x, y, z), (d / 100, d / 100, 0.006), rot=rot, material=m, kind=DECOR)


# ------------------------------------------------------------------ service corridor
def corridor():
    room_shell("Corridor", -800, 3200, 10, 1, doors={"N": [(1, 1), (3, 3), (5, 5), (7, 7), (9, 9)], "S": [(1, 1)], "E": [(0, 0)]},
               wall_mat="Wall_Concrete", floor_mat="Floor_Concrete", trim="Speaker_Black", portal="Speaker_Black")
    f = FH + "Corridor"
    rng = random.Random(21)
    p = []
    for (yy, zz, d, m) in ((3555, 540, 14, "Steel"), (3535, 515, 9, "Fire_Red"), (3560, 495, 7, "Steel")):   # pipes
        p.append(mesh("p", f, CYL, (1200, yy, zz), (d / 100, d / 100, 39.6), rot=(90, 0, 0), material=m, kind=DECOR))
    for x in range(-700, 3200, 300):                                                                       # pipe brackets
        p.append(B(f, x, 3550, 485, 4, 50, 4, "Steel"))
        p.append(B(f, x, 3575, 485, 4, 4, 70, "Steel"))
    p.append(B(f, 1200, 3240, 520, 3960, 30, 6, "Steel"))                                                  # cable tray
    for k in range(6):
        p.append(B(f, 1200, 3235 + k * 4, 526, 3960, 2, 2, rng.choice(["Label_Black", "Fire_Red", "Steel"])))
    for x in range(-500, 3200, 600):                                                                        # fluorescent tubes
        p += [B(f, x, 3400, 590, 130, 14, 4, "Steel"), B(f, x, 3400, 588, 120, 8, 2, "Neon_White")]
    for x in (-400, 400, 1200, 2000, 2800):                                                                 # hazard thresholds
        for k in range(8):
            p.append(B(f, x + 25 + k * 50, 3578, 0.2, 25, 8, 0.6, "Hazard_Yellow"))
    for k in range(8):                                                                                      # Long Bar door (S) + VIP door (E)
        p.append(B(f, -375 + k * 50, 3222, 0.2, 25, 8, 0.6, "Hazard_Yellow"))
        p.append(B(f, 3178, 3225 + k * 50, 0.2, 8, 25, 0.6, "Hazard_Yellow"))
    for (x, y) in ((150, 3236), (1650, 3236), (2650, 3236)):                                               # breaker boxes
        p += [B(f, x, y, 140, 50, 12, 70, "Steel"), B(f, x, y + 7, 150, 40, 1, 50, "Plastic_Grey"),
              B(f, x - 15, y + 8, 175, 4, 1, 4, "Fruit_Lime")]
    for x in (-650, 1150, 2950):                                                                            # extinguishers
        p += [C(f, x, 3240, 60, 50, 14, "Fire_Red"), C(f, x, 3240, 110, 6, 6, "Label_Black"),
              B(f, x + 6, 3240, 112, 10, 2, 2, "Label_Black"), B(f, x, 3226, 150, 30, 2, 22, "Fire_Red")]
    # exit sign over the fire door (west end) + "staff" red bar near storage
    p += [B(f, -778, 3400, 250, 2, 60, 22, "Speaker_Black"), B(f, -776, 3400, 253, 1, 52, 16, "Exit_Green")]
    merge(p, "BOH_CorridorBits", "BOH_CorridorBits", f)
    # fire exit door on the west end wall (decor; the wall behind it stays solid)
    _prop("FireExitDoor", (-770, 3400, 95 * 1.15), f, yaw=90, s=1.15)
    # kegs + crates by the storage door (box collision, low cover) hugging the south wall
    q = []
    for k in range(3):
        q.append(C(f, 2600 + k * 45, 3260, 0, 58, 40, "Steel"))
        q.append(C(f, 2600 + k * 45, 3260, 52, 2, 42, "Label_Black"))
    q.append(C(f, 2622, 3260, 58, 58, 40, "Steel"))
    for k in range(2):
        q.append(B(f, 2800 + k * 70, 3262, 0, 60, 45, 45, "Wood_Crate"))
    q.append(B(f, 2835, 3262, 45, 60, 45, 40, "Cardboard"))
    merge(q, "BOH_CorridorStock", "BOH_CorridorStock", f)
    box("BOH_StockCollider", f, 2575, 2905, 3225, 3290, 0, 116, "Counter_Black").static_mesh_component.set_editor_property("visible", False)
    # mop bucket near the restrooms
    r = [C(f, 1750, 3540, 0, 30, 34, "Hazard_Yellow"), C(f, 1750, 3540, 30, 2, 36, "Label_Black"),
         mesh("p", f, CYL, (1755, 3535, 90), (0.025, 0.025, 1.3), rot=(15, 0, 0), material="Wood_Dark", kind=DECOR)]
    merge(r, "BOH_Mop", "BOH_Mop", f)
    # restroom pictograms + room plates beside each corridor door (on the north wall, facing -Y)
    for (cx, m) in ((1150, "Neon_Cyan"), (1950, "Neon_Magenta")):
        _disc("BOH_Pict", f, cx, 3576, 300, 10, m, "y")
        box("BOH_Pict", f, cx - 6, cx + 6, 3576, 3578, 250, 292, m, DECOR)
        if m == "Neon_Magenta":
            mesh("BOH_Pict", f, CONE, (cx, 3577, 262), (0.3, 0.02, 0.3), rot=(0, 0, 90) if False else (0, 0, 0), material=m, kind=DECOR)
    box("BOH_StaffPlate", f, 2730, 2790, 3576, 3578, 260, 280, "Fire_Red", DECOR)
    for (x0, x1) in ((-400, 0), (400, 800)):
        box("BOH_PrivatePlate", f, x0 - 70, x0 - 10, 3576, 3578, 255, 285, "Gold", DECOR)
    L = "Lighting/BackOfHouse"
    for x in (-300, 900, 2100, 3000):
        light("point", "BOH_CorridorLight", L, (x, 3400, 560), (0.85, 0.92, 1.0), 35, 700)
    light("point", "BOH_ExitGlow", L, (-740, 3400, 260), (0.1, 1, 0.3), 6, 300)
    spawn_marker("Corridor", 500, 3400)
    spawn_marker("Corridor", 2300, 3400)


# ------------------------------------------------------------------ private rooms
def private_room(tag, x0, wall, booth_tint, accent, icon):
    name = "Private" + tag
    room_shell(name, x0, 3600, 2, 2, doors={"S": [(1, 1)]}, wall_mat=wall, floor_mat="Rug_Charcoal",
               trim="Neon_Gold", portal="Neon_Gold")
    f = "Rooms/%s/Props" % name
    cx = x0 + 400
    _prop("VIPBooth", (cx, 4290, 54 * 1.1), f, yaw=180, sxy=1.25, sz=1.1, label="BOH_PrivateBooth" + tag)
    mi = tinted_mi("VIPBooth", booth_tint[0], booth_tint[1])
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "BOH_PrivateBooth" + tag:
            c = a.static_mesh_component
            for i in range(c.get_num_materials()):
                c.set_material(i, mi)
    _hidden("BOH_PrivateBoothCollider", f, cx - 120, cx + 120, 4180, 4380, 0, 110)
    rng = random.Random(x0)
    p = pedestal_table(f, cx, 4030, 0, "Gold" if tag == "A" else "Marble_White", d=90, h=60)
    p += table_set(f, cx, 4030, 63, rng, "champagne" if tag == "A" else "bottle")
    p += rug(f, x0 + 120, x0 + 680, 3800, 4370, 0, "Rug_Purple" if tag == "A" else "Rug_Teal", "Gold")
    # bar cart against the side wall
    bx = x0 + 70
    p += [B(f, bx, 3850, 0, 50, 90, 3, "Chrome"), B(f, bx, 3850, 40, 50, 90, 2, "Glass_Clear"), B(f, bx, 3850, 80, 50, 90, 2, "Glass_Clear")]
    for (dx, dy) in ((-22, -42), (22, -42), (-22, 42), (22, 42)):
        p.append(B(f, bx + dx, 3850 + dy, 0, 3, 3, 82, "Gold"))
    for k in range(4):
        ps, _ = bottle(f, bx - 5, 3820 + k * 20, 82, rng.choice(["square", "vodka", "wine", "champagne"]), rng, 1.0, face=1)
        p += ps
    p += _rocks(f, bx + 10, 3880, 42) + _rocks(f, bx - 10, 3885, 42)
    # mirror panel with LED frame on the other side wall (faces -X)
    mx = x0 + 778
    p += [B(f, mx, 4000, 120, 3, 300, 160, "Mirror_Smoke")]
    for (dy, dz, w, h) in ((0, 118, 304, 3), (0, 280, 304, 3), (-151, 120, 3, 163), (151, 120, 3, 163)):
        p.append(B(f, mx - 2, 4000 + dy, dz, 2, w, h, accent))
    merge(p, "BOH_Private%sBits" % tag, "BOH_Private%sBits" % tag, f)
    _hidden("BOH_PrivateTableBlock", f, cx - 45, cx + 45, 3985, 4075, 0, 63, pawn=True)
    _prop("LoungeArmchair", (cx - 220, 4000, 95 * 0.45), f, yaw=-90, s=0.45, coll="pawn")
    if tag == "A":
        stand = [C(f, cx + 200, 4080, 0, 2, 36, "Gold"), C(f, cx + 200, 4080, 0, 68, 4, "Gold"), C(f, cx + 200, 4080, 66, 3, 26, "Gold")]
        merge(stand, "BOH_BucketStand", "BOH_BucketStand", f)
        _prop("ChampagneBucket", (cx + 200, 4080, 69 + 78 * 0.3), f, s=0.3)
    else:
        _prop("Hookah", (cx + 200, 4080, 95 * 0.42), f, s=0.42)
    # neon icon on the back wall above the booth (faces -Y)
    wy = 4377
    if icon == "heart":
        mesh("BOH_Icon", f, CUBE, (cx, wy, 430), (0.42, 0.01, 0.42), rot=(45, 0, 0), material=accent, kind=DECOR)
        for dx in (-15, 15):
            _disc("BOH_Icon", f, cx + dx, wy, 445, 42, accent, "y")
    else:
        _disc("BOH_Icon", f, cx, wy, 440, 80, accent, "y")
        _disc("BOH_Icon", f, cx + 22, wy - 1, 450, 70, wall, "y")                  # crescent moon cut-out
    L = "Lighting/%s" % name
    light("point", "BOH_PrivateAmbient", L, (cx, 4000, 420), (1, 0.25, 0.45) if tag == "A" else (0.2, 0.7, 1.0), 25, 650)
    light("point", "BOH_PrivateCandle", L, (cx, 4030, 95), (1, 0.5, 0.18), 10, 300)
    light("rect", "BOH_PrivateIconLight", L, (cx, 4330, 430), (1, 0.15, 0.4) if tag == "A" else (0.2, 0.8, 1.0), 8, 400,
          rot=(0, -90, 0), source_width=120.0, source_height=120.0)
    spawn_marker(name, x0 + 550, 3800)


# ------------------------------------------------------------------ restrooms
def restroom(tag, x0, accent, men):
    name = "Restroom" + tag
    room_shell(name, x0, 3600, 2, 3, doors={"S": [(1, 1)]}, wall_mat="Tile_Black", floor_mat="Tile_Black",
               trim=accent, portal=accent)
    f = "Rooms/%s/Props" % name
    ix0 = x0 + 20
    # six stalls on the west wall: partitions are high cover (250), stall doors vary (closed / ajar / open)
    sy0, sw, depth = 3960, 136, 160
    rng = random.Random(x0)
    for k in range(7):
        y = sy0 + k * sw
        box("BOH_StallWall", f, ix0, ix0 + depth, y - 2, y + 2, 12, 250, "Steel")
    for k in range(6):
        y = sy0 + k * sw
        box("BOH_StallFront", f, ix0 + depth - 4, ix0 + depth, y + 2, y + 18, 12, 250, "Steel")       # jamb
        state = rng.choice(["closed", "ajar", "open", "ajar"])
        hx, hy = ix0 + depth - 2, y + 18
        ang = {"closed": 0, "ajar": 35, "open": 80}[state]
        a = mesh("BOH_StallDoor", f, CUBE, (hx - math.sin(math.radians(ang)) * 57, hy + math.cos(math.radians(ang)) * 57, 131),
                 (0.03, 1.14, 2.38), rot=(0, ang, 0), material="Steel", kind=(STRUCT if state == "closed" else DECOR))
        box("BOH_StallLock", f, ix0 + depth, ix0 + depth + 1, y + sw - 30, y + sw - 24, 110, 116,
            "LED_Red" if state == "closed" else "Fruit_Lime", DECOR)
        # toilet: bowl, seat, tank
        tx, ty = ix0 + 45, y + sw / 2
        p = [C(f, tx + 10, ty, 0, 38, 34, "Porcelain", sx=44), C(f, tx + 10, ty, 38, 3, 36, "Label_Black", sx=46),
             B(f, tx - 18, ty, 38, 18, 40, 40, "Porcelain"), B(f, tx - 18, ty, 78, 20, 42, 3, "Porcelain"),
             C(f, ix0 + 4, ty + 45, 80, 12, 12, "Steel")]
        merge(p, "BOH_Toilet", "BOH_Toilet", f)
    box("BOH_StallNeon", f, ix0 + depth, ix0 + depth + 2, sy0, sy0 + 6 * sw, 250, 254, accent, DECOR)
    # sink counter + mirror on the east wall
    ex = x0 + 780
    box("BOH_SinkCounter", f, ex - 60, ex, 3720, 4440, 0, 90, "Counter_Black")
    _pawn_only(_hidden("BOH_SinkBlock", f, ex - 60, ex, 3720, 4440, 0, 90))
    box("BOH_SinkTop", f, ex - 64, ex, 3715, 4445, 88, 92, "Marble_White", DECOR)
    box("BOH_Mirror", f, ex - 3, ex, 3730, 4430, 120, 230, "Mirror_Smoke", DECOR)
    box("BOH_MirrorLED", f, ex - 5, ex - 3, 3730, 4430, 116, 119, accent, DECOR)
    box("BOH_MirrorLED", f, ex - 5, ex - 3, 3730, 4430, 231, 234, "Neon_White", DECOR)
    p = []
    for k in range(4):
        y = 3810 + k * 170
        p += [C(f, ex - 32, y, 86, 6, 42, "Porcelain", sx=36), C(f, ex - 32, y, 91.5, 0.5, 34, "Steel", sx=28),
              B(f, ex - 8, y, 92, 5, 4, 20, "Chrome"), B(f, ex - 16, y, 108, 16, 3, 3, "Chrome")]
    p += [B(f, ex - 10, 4520, 120, 18, 26, 30, "Chrome"), B(f, ex - 20, 4520, 122, 2, 12, 3, "Steel")]         # hand dryer
    p += [B(f, ex - 6, 3660, 130, 10, 26, 36, "Steel")]                                                       # towel box
    if men:
        for k in range(3):                                                                                     # urinals, north wall
            ux = x0 + 330 + k * 120
            p += [B(f, ux, 4768, 50, 40, 22, 70, "Porcelain"), B(f, ux, 4775, 118, 6, 6, 14, "Chrome")]
            if k < 2:
                p.append(B(f, ux + 60, 4745, 40, 3, 60, 110, "Counter_Black"))
    else:
        p += [B(f, x0 + 450, 4765, 0, 260, 30, 75, "Counter_Black"), B(f, x0 + 450, 4778, 110, 220, 3, 120, "Mirror_Smoke")]
        for k in range(10):                                                                                    # bulb-framed vanity mirror
            p.append(S(f, x0 + 340 + k * 24.5, 4775, 236, 6, "Glow_Warm"))
        p += [C(f, x0 + 450, 4720, 0, 45, 40, "Velvet_Red"), C(f, x0 + 450, 4720, 45, 3, 42, "Gold")]          # stool
    merge(p, "BOH_RestroomBits", "BOH_RestroomBits", f)
    # pictogram inside, above the sinks (faces -X)
    _disc("BOH_Pict", f, ex - 6, 3650, 300, 16, accent, "x")
    box("BOH_Pict", f, ex - 7, ex - 5, 3642, 3658, 230, 290, accent, DECOR)
    L = "Lighting/%s" % name
    light("rect", "BOH_MirrorLight", L, (ex - 30, 4080, 250), (0.9, 0.95, 1.0), 60, 500, rot=(-30, 180, 0), source_width=700.0, source_height=40.0)
    light("point", "BOH_RestroomNeon", L, (x0 + 400, 4200, 450), (0.15, 0.75, 1.0) if men else (1, 0.1, 0.55), 40, 800)
    light("point", "BOH_RestroomFill", L, (x0 + 450, 4000, 520), (0.9, 0.95, 1.0), 45, 800)
    merge([B(f, x0 + 450, y, 590, 130, 14, 4, "Steel") for y in (3900, 4400)] + [B(f, x0 + 450, y, 588, 120, 8, 2, "Neon_White") for y in (3900, 4400)],
          "BOH_RestroomTubes", "BOH_RestroomTubes", f)
    light("point", "BOH_StallLight", L, (ix0 + 80, 4370, 450), (0.85, 0.9, 1.0), 35, 600)
    spawn_marker(name, x0 + 450, 3800)


# ------------------------------------------------------------------ storage
def storage():
    name = "Storage"
    room_shell(name, 2400, 3600, 2, 3, doors={"S": [(1, 1)]}, wall_mat="Wall_Concrete", floor_mat="Floor_Concrete",
               trim="Speaker_Black", portal="Speaker_Black")
    f = "Rooms/Storage/Props"
    rng = random.Random(77)

    def shelving(x0, x1, y0, y1):
        """Steel rack (high cover 250): uprights, 4 shelves, random stock. Box collider."""
        p = []
        for (xx, yy) in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
            p.append(B(f, xx, yy, 0, 5, 5, 250, "Steel"))
        for z in (15, 75, 140, 205):
            p.append(B(f, (x0 + x1) / 2, (y0 + y1) / 2, z, x1 - x0, y1 - y0, 3, "Steel"))
            y = y0 + 8
            while y < y1 - 30:
                kind = rng.random()
                w = rng.uniform(30, 55)
                if kind < 0.5:
                    p.append(B(f, (x0 + x1) / 2, y + w / 2, z + 3, (x1 - x0) - 10, w, rng.uniform(25, 50), "Cardboard"))
                elif kind < 0.75:
                    p.append(B(f, (x0 + x1) / 2, y + w / 2, z + 3, (x1 - x0) - 12, w, rng.uniform(25, 45), "Wood_Crate"))
                else:
                    for k in range(int(w // 12)):
                        p.append(C(f, (x0 + x1) / 2, y + 6 + k * 12, z + 3, 30, 9, rng.choice(["Glass_Amber", "Glass_Green", "Glass_Clear"])))
                y += w + rng.uniform(3, 12)
        box("BOH_ShelfCollider", f, min(x0, x1), max(x0, x1), y0, y1, 0, 250, "Counter_Black").static_mesh_component.set_editor_property("visible", False)
        return p

    p = shelving(2425, 2485, 3700, 4250) + shelving(2425, 2485, 4300, 4760)          # west wall racks
    p += shelving(3115, 3175, 4000, 4760)                                            # east wall rack
    p += shelving(2630, 2690, 4560, 4775)                                            # short rack on the north wall
    merge(p, "BOH_StorageShelves", "BOH_StorageShelves", f)
    # centre island: kegs + crates (low cover 120) on pallets
    q = []
    for (px, py) in ((2760, 4050), (2760, 4330)):
        q += [B(f, px, py, 0, 120, 100, 12, "Wood_Crate")]
    for i in range(3):
        for j in range(2):
            q += [C(f, 2725 + i * 36, 4020 + j * 50, 12, 58, 40, "Steel"), C(f, 2725 + i * 36, 4020 + j * 50, 64, 2, 42, "Label_Black")]
    for (i, j, h, m) in ((0, 0, 50, "Wood_Crate"), (1, 0, 50, "Wood_Crate"), (0, 1, 50, "Cardboard"), (0, 0, 100, "Cardboard")):
        q.append(B(f, 2730 + i * 60, 4310 + j * 45, 12 + (h - 50), 58, 44, 48, m))
    merge(q, "BOH_StorageIsland", "BOH_StorageIsland", f)
    box("BOH_IslandCollider", f, 2700, 2820, 3990, 4385, 0, 120, "Counter_Black").static_mesh_component.set_editor_property("visible", False)
    # mop sink + pallet jack + loose boxes by the door
    r = [B(f, 3140, 3700, 0, 70, 60, 70, "Steel"), B(f, 3140, 3700, 70, 66, 56, 2, "Porcelain"), B(f, 3172, 3700, 70, 4, 8, 40, "Chrome")]
    r += [B(f, 2950, 3750, 8, 60, 110, 6, "Hazard_Yellow"), B(f, 2950, 3690, 8, 20, 8, 110, "Hazard_Yellow"),
          C(f, 2930, 3800, 0, 8, 8, "Label_Black"), C(f, 2970, 3800, 0, 8, 8, "Label_Black")]
    r += [B(f, 2560, 3700, 0, 50, 40, 40, "Cardboard"), B(f, 2565, 3700, 40, 45, 38, 35, "Cardboard", yaw=12)]
    merge(r, "BOH_StorageBits", "BOH_StorageBits", f)
    L = "Lighting/Storage"
    for y in (3900, 4450):
        light("point", "BOH_StorageLight", L, (2800, y, 560), (0.85, 0.92, 1.0), 55, 750)
    p2 = [B(f, 2800, y, 590, 130, 14, 4, "Steel") for y in (3900, 4450)] + [B(f, 2800, y, 588, 120, 8, 2, "Neon_White") for y in (3900, 4450)]
    merge(p2, "BOH_StorageTubes", "BOH_StorageTubes", f)
    spawn_marker(name, 2600, 3850)
    spawn_marker(name, 3000, 4600)


def build_backofhouse():
    r = {"cleared": _clear()}
    corridor()
    private_room("A", -800, "Velvet_Red", ("Gold", (1.8, 1.1, 0.45)), "Neon_Magenta", "heart")
    private_room("B", 0, "Velvet_Teal", ("Purple", (0.9, 0.25, 1.6)), "Neon_Cyan", "moon")
    restroom("M", 800, "Neon_Cyan", True)
    restroom("W", 1600, "Neon_Magenta", False)
    storage()
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if any(str(a.get_folder_path()).startswith(("Rooms/%s" % x, "Lighting/%s" % x)) for x in ROOMS)
                       or str(a.get_folder_path()).startswith(LH)])
    return r
