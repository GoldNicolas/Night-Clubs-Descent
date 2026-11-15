"""VIP Lounge (4x5 tiles) north of Billiards: raised gold/purple lounge reached through the billiards room.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_vip().

Layout (cm): room x 3200..4800, y 2000..4000. Doors: S tiles 0-1 (x 3200..4000) = Billiards' north door; W tile 3 (y 3200..3600, on the platform) = service corridor.
  entry zone  y 2020..2600 (floor 0): host stand, velvet ropes, sconces.
  platform    y 2600..3980, raised 45 cm (one step). Front edge: low neon walls (120) with a 6 m opening x 3600..4200.
  north       two gold booths under a gold art-deco feature wall.
  east        two purple booths.          west: lit champagne display cabinet + armchair lounge with a hookah.
  centre      round gold table with a champagne tower.
Booths have invisible box colliders (low cover); chairs/tables block pawns only; gaps stay >= 2.4 m (main lanes >= 4 m).
"""
import math
import random
import unreal

N = "VIP"
X0, Y0, TW, TD = 3200, 2000, 4, 5
FV2 = "Rooms/VIP/"
LV = "Lighting/VIP"
PZ = 45          # platform height
PY = 2600        # platform front edge


def _clear_room():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Rooms/VIP") or fp.startswith("Lighting/VIP"):
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
    a = prop(name, loc, yaw=yaw, s=s, sxy=sxy, sz=sz, coll=coll, label=label or ("VIP_" + name))
    a.set_folder_path(folder)
    return a


def _hidden_struct(label, f, x0, x1, y0, y1, z0, z1):
    a = box(label, f, x0, x1, y0, y1, z0, z1, "Counter_Black")
    a.static_mesh_component.set_editor_property("visible", False)
    a.static_mesh_component.set_editor_property("cast_shadow", False)
    return a


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"S": [(0, 1)], "W": [(3, 3)]}, trim="Neon_Gold", portal="Neon_Magenta")


def platform():
    f = FV2 + "Platform"
    box("VIP_Platform", f, 3220, 4780, PY, 3980, 0, PZ, "Platform_Dark")
    box("VIP_PlatformEdge", f, 3220, 4780, PY - 4, PY, PZ - 8, PZ - 3, "Neon_Gold", DECOR)
    # low neon walls (120 above the platform) either side of a 6 m opening
    for (x0, x1) in ((3220, 3600), (4200, 4780)):
        box("VIP_LowWall", f, x0, x1, PY, PY + 25, PZ, PZ + 120, "Wall_Panel")
        box("VIP_LowWallTop", f, x0, x1, PY - 2, PY + 27, PZ + 120, PZ + 124, "Gold", DECOR)
        box("VIP_LowWallNeon", f, x0, x1, PY - 3, PY - 1, PZ + 104, PZ + 108, "Neon_Gold", DECOR)
    for x in (3600, 4200):                                                     # gold posts at the opening
        box("VIP_OpeningPost", f, x - 12, x + 12, PY - 6, PY + 31, 0, PZ + 140, "Gold")
        mesh("VIP_OpeningPostTop", f, SPHERE, (x, PY + 12, PZ + 148), (0.2, 0.2, 0.2), material="Gold", kind=DECOR)
    # purple carpet over most of the platform
    merge(rug(f, 3300, 4700, PY + 60, 3940, PZ, "Rug_Purple", "Gold"), "VIP_Carpet", "VIP_Carpet", f)


def booths():
    f = FV2 + "Booths"
    z = PZ + 54 * 1.1
    # north wall: two gold booths (opening faces -Y)
    for x in (3650, 4350):
        _prop("VIPBooth", (x, 3870, z), f, yaw=180, sxy=1.25, sz=1.1, label="VIP_BoothGold")
        _hidden_struct("VIP_BoothCollider", f, x - 120, x + 120, 3760, 3980, PZ, PZ + 110)
    # east wall: two purple booths (opening faces -X)
    for y in (2950, 3400):
        _prop("VIPBooth", (4670, y, z), f, yaw=90, sxy=1.25, sz=1.1, label="VIP_BoothPurple")
        _hidden_struct("VIP_BoothCollider", f, 4560, 4780, y - 120, y + 120, PZ, PZ + 110)
    gold = tinted_mi("VIPBooth", "Gold", (1.8, 1.1, 0.45))
    purple = tinted_mi("VIPBooth", "Purple", (0.9, 0.25, 1.6))
    for a in _EAS.get_all_level_actors():
        lb = a.get_actor_label()
        if lb in ("VIP_BoothGold", "VIP_BoothPurple"):
            c = a.static_mesh_component
            for i in range(c.get_num_materials()):
                c.set_material(i, gold if lb == "VIP_BoothGold" else purple)
    # tables in front of the booths, each dressed differently
    rng = random.Random(31)
    p = []
    for (x, y, kind) in ((3650, 3620, "champagne"), (4350, 3620, "bottle"), (4420, 2950, "cocktails"), (4420, 3400, "ashtray")):
        p += pedestal_table(f, x, y, PZ, "Marble_White" if kind in ("champagne", "cocktails") else "Gold", d=90, h=60)
        p += table_set(f, x, y, PZ + 63, rng, kind)
    merge(p, "VIP_BoothTables", "VIP_BoothTables", f)
    for (x, y) in ((3650, 3620), (4350, 3620), (4420, 2950), (4420, 3400)):
        blk = box("VIP_TableBlock", f, x - 45, x + 45, y - 45, y + 45, PZ, PZ + 63, "Counter_Black")
        blk.static_mesh_component.set_editor_property("visible", False)
        _pawn_only(blk)
    for (x, y) in ((3650, 3620), (4350, 3620)):
        _prop("ChampagneBucket", (x + 30, y - 30, PZ + 63 + 78 * 0.22), f, s=0.22)


def art_wall():
    """Gold art-deco feature wall above the north booths: amber backlight behind vertical gold fins + sunburst."""
    f = FV2 + "Art"
    box("VIP_ArtBack", f, 3300, 4700, 3976, 3980, 190, 520, "Onyx_Amber", DECOR)
    p = []
    for k in range(36):
        x = 3320 + k * 39
        h = 300 - abs(k - 17.5) * 6                                            # stepped skyline profile
        p.append(B(f, x, 3972, 200, 8, 6, h, "Gold"))
    cx, cz, ln = 4000, 340, 120                                                # sunburst: 13 rays fanning upward
    for k in range(13):
        a = k * 15.0
        p.append(mesh("p", f, CUBE, (cx + math.cos(math.radians(a)) * ln / 2, 3966, cz + math.sin(math.radians(a)) * ln / 2),
                      (ln / 100, 0.02, 0.03), rot=(a, 0, 0), material="Gold", kind=DECOR))
    p.append(B(f, 4000, 3968, 186, 1420, 4, 6, "Gold"))
    p.append(B(f, 4000, 3968, 520, 1420, 4, 6, "Gold"))
    merge(p, "VIP_ArtFins", "VIP_ArtFins", f)


def cabinet():
    """Lit champagne display cabinet on the west wall (high cover, box collision)."""
    f = FV2 + "Cabinet"
    x0, x1, y0, y1 = 3222, 3292, 2660, 3140                     # clear of the W door (y 3200..3600)
    box("VIP_Cabinet", f, x0, x1, y0, y1, PZ, PZ + 210, "Counter_Black")
    box("VIP_CabinetGlow", f, x1, x1 + 1, y0 + 20, y1 - 20, PZ + 30, PZ + 195, "Glow_Warm", DECOR)
    box("VIP_CabinetGlass", f, x1 + 18, x1 + 19, y0 + 10, y1 - 10, PZ + 25, PZ + 200, "Glass_Clear", DECOR)
    for (yy0, yy1) in ((y0, y0 + 12), (y1 - 12, y1)):
        box("VIP_CabinetFrame", f, x1, x1 + 20, yy0, yy1, PZ, PZ + 210, "Gold", DECOR)
    box("VIP_CabinetFrame", f, x1, x1 + 20, y0, y1, PZ + 200, PZ + 210, "Gold", DECOR)
    box("VIP_CabinetFrame", f, x1, x1 + 20, y0, y1, PZ, PZ + 25, "Gold", DECOR)
    rng = random.Random(55)
    p = []
    for z in (PZ + 30, PZ + 85, PZ + 140):
        p.append(B(f, x1 + 9, (y0 + y1) / 2, z - 2, 16, y1 - y0 - 30, 2, "Glass_Clear"))
        y = y0 + 30
        while y < y1 - 40:
            ps, w = bottle(f, x1 + 9, y, z, "champagne", rng, rng.uniform(1.15, 1.3), face=1)
            p += ps
            y += w + rng.uniform(10, 18)
    merge(p, "VIP_CabinetBottles", "VIP_CabinetBottles", f)


def lounge_and_centre():
    f = FV2 + "Lounge"
    # armchair lounge west of the centre (pawn-only)
    _prop("LoungeArmchair", (3560, 2880, PZ + 95 * 0.45), f, yaw=0, s=0.45, coll="pawn")
    _prop("LoungeArmchair", (3560, 3250, PZ + 95 * 0.45), f, yaw=180, s=0.45, coll="pawn")
    _prop("CoffeeTable", (3560, 3065, PZ + 38 * 0.45), f, s=0.45, coll="pawn")
    _prop("Hookah", (3440, 3065, PZ + 95 * 0.42), f, s=0.42)
    # centrepiece: round gold table with a champagne tower (4-3-2-1 coupes)
    cx, cy = 4050, 3130
    p = pedestal_table(f, cx, cy, PZ, "Gold", d=130, h=75)
    p.append(C(f, cx, cy, PZ + 78, 1.5, 120, "Mirror_Smoke"))
    zc = PZ + 79.5
    for lvl, n in enumerate((4, 3, 2, 1)):
        for i in range(n):
            for j in range(n):
                x = cx + (i - (n - 1) / 2) * 11
                y = cy + (j - (n - 1) / 2) * 11
                z = zc + lvl * 12.5
                p += [C(f, x, y, z, 0.5, 6, "Glass_Clear"), C(f, x, y, z, 7, 0.9, "Glass_Clear"),
                      C(f, x, y, z + 7, 4.5, 10.5, "Glass_Amber")]
    ps, _ = bottle(f, cx + 45, cy - 30, zc, "champagne", random.Random(2), 1.2, face=-1)
    p += ps
    merge(p, "VIP_ChampagneTower", "VIP_ChampagneTower", f)
    blk = box("VIP_TowerBlock", f, cx - 65, cx + 65, cy - 65, cy + 65, PZ, PZ + 78, "Counter_Black")
    blk.static_mesh_component.set_editor_property("visible", False)
    _pawn_only(blk)
    for (x, y) in ((3750, 3300), (4400, 3300)):
        _prop("Chandelier", (x, y, 600 - 20 - 95 * 0.55), f, s=0.55)


def entry_zone():
    f = FV2 + "Entry"
    _prop("BouncerPodium", (3900, 2300, 95 * 0.6), f, yaw=-90, s=0.6, coll="pawn", label="VIP_HostStand")
    for x in (3260, 3940):                                                     # ropes framing the door inside
        posts = [2060, 2180, 2300]
        for y in posts:
            cyl("VIP_RopePost", f, x, y, 0, 95, 6, "Gold", DECOR)
            cyl("VIP_RopePostBase", f, x, y, 0, 3, 28, "Gold", DECOR)
            mesh("VIP_RopePostTop", f, SPHERE, (x, y, 98), (0.09, 0.09, 0.09), material="Gold", kind=DECOR)
        for (y0, y1) in zip(posts, posts[1:]):
            mesh("VIP_Rope", f, CYL, (x, (y0 + y1) / 2, 82), (0.035, 0.035, (y1 - y0) / 100), rot=(0, 0, 90),
                 material="Glass_Purple", kind=DECOR)
    # wall sconces (gold, warm) on the entry walls
    for (x, y) in ((3222, 2350), (4778, 2350)):
        box("VIP_Sconce", f, x - 3 if x < 4000 else x - 7, x + 7 if x < 4000 else x + 3, y - 15, y + 15, 230, 290, "Gold", DECOR)
        box("VIP_SconceGlow", f, x + 6 if x < 4000 else x - 8, x + 8 if x < 4000 else x - 6, y - 10, y + 10, 238, 282, "Glow_Warm", DECOR)
    _prop("SecurityCam", (4755, 2025, 560), f, yaw=45, s=0.16)
    _prop("WallSpeaker", (3237, 3820, 430), f, yaw=-90, s=0.28)
    _prop("WallSpeaker", (4763, 2400, 430), f, yaw=90, s=0.28)


def lights():
    WARM, GOLD, PURPLE = (1, 0.7, 0.4), (1, 0.6, 0.25), (0.6, 0.15, 1.0)
    for (x, y) in ((3750, 3300), (4400, 3300)):
        light("point", "VIP_Chandelier", LV, (x, y, 450), GOLD, 45, 900)
    light("rect", "VIP_ArtWash", LV, (4000, 3900, 560), GOLD, 50, 800, rot=(-70, 90, 0), source_width=1400.0, source_height=100.0)
    light("spot", "VIP_TowerSpot", LV, (4050, 3130, 590), (1, 0.95, 0.85), 500, 700, rot=(-90, 0, 0),
          outer_cone_angle=12.0, inner_cone_angle=6.0, shadows=True)
    light("rect", "VIP_CabinetLight", LV, (3320, 2900, 160), GOLD, 15, 450, rot=(0, 0, 0), source_width=450.0, source_height=170.0)
    light("point", "VIP_CorridorDoorGlow", LV, (3290, 3400, 420), (1, 0.1, 0.55), 18, 450)
    light("point", "VIP_PurpleBooths", LV, (4550, 3175, 300), PURPLE, 30, 650)
    light("point", "VIP_EntryGlow", LV, (3600, 2300, 350), (1, 0.1, 0.55), 20, 600)
    for (x, y) in ((3240, 2350), (4760, 2350)):
        light("point", "VIP_SconceLight", LV, (x + (25 if x < 4000 else -25), y, 260), WARM, 8, 300)


def build_vip():
    r = {"cleared": _clear_room()}
    shell()
    platform()
    booths()
    art_wall()
    cabinet()
    lounge_and_centre()
    entry_zone()
    lights()
    fog_cards(N, 3240, 2040, 4760, PY - 10, layers=((8, 0.5), (30, 0.3)))
    fog_cards("VIP_Platform", 3300, PY + 40, 4700, 3940, layers=((PZ + 8, 0.45), (PZ + 30, 0.25)))
    spawn_marker(N, 3500, 2350)       # entry zone
    spawn_marker(N, 4100, 2800, PZ)   # platform, south of the champagne tower
    spawn_marker(N, 3720, 3480, PZ)   # platform, north-west (clear of the corridor door)
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/VIP", "Lighting/VIP"))])
    return r
