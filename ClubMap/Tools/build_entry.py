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


CCTV_MAT = "/Game/ClubMap/Materials/M_CM_CCTV"
CCTV_HLSL = r"""
float2 p = float2(WP.y, WP.z);
float2 cellid = floor(float2(WP.y + 560.0, WP.z - 160.0) / float2(140.0, 85.0));
float seed = frac(sin(dot(cellid, float2(12.9898, 78.233))) * 43758.5453);
float2 blk = floor(p / 14.0);
float scene = frac(sin(dot(blk + seed * 31.0, float2(4.1, 7.7))) * 1000.0);
float scan = 0.8 + 0.2 * sin(p.y * 1.4 - T * 6.0);
float noise = frac(sin(dot(floor(p * 0.9) + floor(T * 15.0), float2(12.9898, 78.233))) * 43758.5453);
float roll = smoothstep(0.0, 0.05, abs(frac((p.y / 75.0) - T * 0.15 - seed) - 0.5));
float3 tint = lerp(float3(0.05, 0.12, 0.14), float3(0.07, 0.14, 0.10), seed);
return (tint * (0.35 + 0.8 * scene) * scan * (0.6 + 0.4 * roll) + noise * 0.05) * Brightness;
"""


def ensure_cctv_material():
    """Unlit CCTV feed: blocky fake scene per screen, scanlines, static and a slow roll bar."""
    if _EAL.does_asset_exist(CCTV_MAT):
        return unreal.load_asset(CCTV_MAT)
    MELx = unreal.MaterialEditingLibrary
    m = _AT.create_asset("M_CM_CCTV", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    cu = MELx.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", CCTV_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("WP", "T", "Brightness"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    MELx.connect_material_expressions(MELx.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -800, -100), "", cu, "WP")
    MELx.connect_material_expressions(MELx.create_material_expression(m, unreal.MaterialExpressionTime, -800, 0), "", cu, "T")
    br = MELx.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, 100)
    br.set_editor_property("parameter_name", "Brightness")
    br.set_editor_property("default_value", 1.0)
    MELx.connect_material_expressions(br, "", cu, "Brightness")
    MELx.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MELx.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def cctv_mi(brightness=0.55):
    path = MAT_DIR + "/MI_CM_CCTV"
    if _EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = _AT.create_asset("MI_CM_CCTV", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, ensure_cctv_material())
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Brightness", brightness)
    _EAL.save_loaded_asset(mi)
    return mi


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"S": [(1, 2)], "N": [(1, 2)]},
                      trim="Neon_Magenta", portal="Neon_Cyan")


def front_doors():
    """The front opening now leads out to the club threshold / storm drains (build_tunnels.py): just a rubber mat."""
    f = FE + "Doors"
    box("Entry_FrontMat", f, 1220, 1980, -1178, -1120, 0.2, 1.2, "Fabric_Black", DECOR)

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
    # security CCTV wall (3 x 2 screens) where the bar shelves used to be
    box("Entry_CCTVBack", f, 2372, 2380, -575, -125, 150, 330, "Speaker_Black", DECOR)
    cctv = cctv_mi()
    for col in range(3):
        for row in range(2):
            y0 = -560 + col * 140
            z0 = 160 + row * 85
            sc = box("Entry_CCTVScreen", f, 2370, 2372, y0 + 6, y0 + 134, z0 + 4, z0 + 79, "Counter_Black", DECOR)
            sc.static_mesh_component.set_material(0, cctv)
            box("Entry_CCTVRec", f, 2369, 2370, y0 + 118, y0 + 124, z0 + 66, z0 + 72, "LED_Red", DECOR)
            box("Entry_CCTVTag", f, 2369, 2370, y0 + 12, y0 + 52, z0 + 68, z0 + 71, "Label_White", DECOR)

def desk_and_bar_dressing():
    """Front desk (south half) + security clearance station (north half). No drinks: this is the checkpoint."""
    f = FE + "Props"
    z = 120
    p = []
    # front desk: monitor facing the staff (+X), guests see its back; keyboard, tablet, card reader, guest list
    p += [B(f, 2100, -900, z, 16, 18, 1.5, "Plastic_Grey"), B(f, 2100, -900, z + 1.5, 4, 4, 22, "Plastic_Grey"),
          B(f, 2097, -900, z + 20, 2.5, 52, 32, "Plastic_Grey"), B(f, 2098.5, -900, z + 21.5, 0.4, 49, 29, "Screen_Laptop")]
    p += [B(f, 2115, -900, z, 15, 44, 1.5, "Label_Black")]
    p += [B(f, 2070, -1020, z, 18, 24, 1, "Label_Black"), B(f, 2070, -1020, z + 1, 16, 22, 0.2, "Screen_Laptop")]
    p += [B(f, 2060, -780, z, 7, 12, 4, "Plastic_Grey"), B(f, 2058, -780, z + 4, 4, 6, 0.4, "Screen_Laptop")]
    p += [B(f, 2070, -1110, z, 23, 31, 1, "Label_Black"), B(f, 2070, -1110, z + 1, 21, 28, 0.3, "Label_White"),
          B(f, 2070, -1124, z + 1.3, 6, 3, 1.5, "Chrome")]
    # security station: ID scanner with a red laser line, metal-detector wand, belongings trays, UV stamp, radio
    p += [B(f, 2075, -520, z, 22, 16, 6, "Plastic_Grey"), B(f, 2075, -520, z + 6, 18, 12, 0.4, "Counter_Black"),
          B(f, 2075, -520, z + 6.5, 16, 0.6, 0.2, "LED_Red")]
    p += [B(f, 2085, -460, z, 6, 34, 4, "Label_Black", yaw=15), B(f, 2085, -432, z, 9, 14, 2.5, "Label_Black", yaw=15),
          B(f, 2083, -478, z + 4, 1, 3, 0.4, "Fruit_Lime")]                                   # wand + its green LED
    for k, y in enumerate((-380, -300, -220)):                                               # grey trays (one with keys + phone)
        p += [B(f, 2068, y, z + k * 0.01, 34, 48, 1.2, "Plastic_Grey"), B(f, 2052, y, z, 2, 48, 6, "Plastic_Grey"),
              B(f, 2084, y, z, 2, 48, 6, "Plastic_Grey"), B(f, 2068, y - 23, z, 34, 2, 6, "Plastic_Grey"),
              B(f, 2068, y + 23, z, 34, 2, 6, "Plastic_Grey")]
    p += [B(f, 2066, -385, z + 1.2, 7, 14, 0.8, "Label_Black"), B(f, 2066, -385, z + 2, 6, 13, 0.1, "Screen_Laptop"),
          C(f, 2072, -300, z + 1.2, 1, 4, "Chrome"), B(f, 2072, -296, z + 1.2, 1.5, 5, 0.5, "Chrome")]
    p += [B(f, 2100, -170, z, 10, 8, 1.5, "Counter_Black"), B(f, 2100, -170, z + 1.5, 8, 6, 0.3, "Glass_Purple"),
          C(f, 2085, -150, z, 4, 4, "Glass_Purple"), C(f, 2085, -150, z + 4, 5, 2.2, "Label_Black")]         # UV stamp + pad
    p += [B(f, 2110, -600, z, 5, 3, 16, "Label_Black"), C(f, 2111, -599, z + 16, 6, 0.8, "Label_Black"),
          B(f, 2108, -600, z + 10, 0.3, 2, 2, "Fruit_Lime")]                                  # walkie-talkie
    merge(p, "Entry_Dressing", "Entry_Dressing", f)
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
    _prop("LoungeArmchair", (1010, -430, 95 * 0.45), f, yaw=0, s=0.45, coll="pawn")
    _prop("LoungeArmchair", (1010, -120, 95 * 0.45), f, yaw=180, s=0.45, coll="pawn")
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



def metal_detector():
    """Two walk-through metal detectors (110 cm openings) on rubber mats before the dance-hall door. Pillars block pawns only."""
    f = FE + "Security"
    cy = -330
    for cx in (1460, 1740):
        box("Entry_DetectorMat", f, cx - 90, cx + 90, cy - 120, cy + 120, 0.2, 1.2, "Fabric_Black", DECOR)
        box("Entry_DetectorMatEdge", f, cx - 90, cx + 90, cy - 122, cy - 120, 0.2, 1.4, "Neon_Gold", DECOR)
        for sx in (-1, 1):
            xa, xb = sorted((cx + sx * 55, cx + sx * 71))
            pil = box("Entry_DetectorPillar", f, xa, xb, cy - 32, cy + 32, 0, 220, "Plastic_Grey")
            _pawn_only(pil)
            for (y0, y1) in ((cy - 33, cy - 32), (cy + 32, cy + 33)):
                box("Entry_DetectorLED", f, xa + 3, xb - 3, y0, y1, 40, 200, "Neon_Cyan", DECOR)
        box("Entry_DetectorTop", f, cx - 71, cx + 71, cy - 34, cy + 34, 220, 240, "Plastic_Grey", DECOR)
        for (y0, y1) in ((cy - 35, cy - 34), (cy + 34, cy + 35)):
            box("Entry_DetectorDisplay", f, cx - 20, cx + 20, y0, y1, 224, 236, "Fruit_Lime", DECOR)   # green = clear
    box("Entry_WaitLine", f, 1360, 1840, cy - 140, cy - 130, 0.2, 1.0, "Neon_Gold", DECOR)              # "wait here" line


def xray_scanner():
    """Baggage X-ray beside the detectors: body (low cover, 120), roller conveyors in/out, lead curtains, console, a bag."""
    f = FE + "Security"
    x0, x1 = 1880, 1960
    box("Entry_XRayBody", f, x0, x1, -400, -260, 0, 120, "Plastic_Grey")
    box("Entry_XRayStripe", f, x0 - 1, x0, -400, -260, 95, 100, "Neon_Cyan", DECOR)
    for (y0, y1) in ((-401, -400), (-260, -259)):                                    # tunnel mouths + lead curtains
        box("Entry_XRayTunnel", f, x0 + 10, x1 - 10, y0, y1, 78, 115, "Counter_Black", DECOR)
        for k in range(6):
            xx = x0 + 12 + k * 9.5
            box("Entry_XRayCurtain", f, xx, xx + 8, y0 - 0.5 if y0 < -300 else y1, (y0 - 0.5 if y0 < -300 else y1) + 0.5,
                80, 114, "Fabric_Black", DECOR)
    p = []
    for (ya, yb) in ((-560, -400), (-260, -100)):                                     # roller conveyors on legs
        p += [B(f, x0 + 5, (ya + yb) / 2, 74, 4, yb - ya, 6, "Chrome"), B(f, x1 - 5, (ya + yb) / 2, 74, 4, yb - ya, 6, "Chrome")]
        for yy in range(int(ya) + 6, int(yb), 9):
            p.append(mesh("p", f, CYL, ((x0 + x1) / 2, yy, 79), (0.05, 0.05, (x1 - x0 - 12) / 100), rot=(90, 0, 0),
                          material="Chrome", kind=DECOR))
        for yy in (ya + 10, yb - 10):
            p += [B(f, x0 + 5, yy, 0, 4, 4, 74, "Chrome"), B(f, x1 - 5, yy, 0, 4, 4, 74, "Chrome")]
    p += [B(f, (x0 + x1) / 2, -170, 82, 45, 30, 22, "Fabric_Black"), B(f, (x0 + x1) / 2, -170, 104, 18, 3, 6, "Fabric_Black")]   # bag
    p += [B(f, (x0 + x1) / 2, -480, 82, 50, 36, 1.2, "Plastic_Grey")]                                                          # tray
    # console on top: monitor facing the lane (-X) showing an x-ray image (orange/blue blocks)
    p += [B(f, x1 - 15, -330, 120, 20, 30, 3, "Plastic_Grey"), B(f, x1 - 15, -330, 123, 4, 4, 20, "Plastic_Grey"),
          B(f, x1 - 18, -330, 140, 2.5, 46, 32, "Plastic_Grey")]
    rng = random.Random(3)
    p.append(B(f, x1 - 19.6, -330, 141.5, 0.4, 43, 29, "Screen_Blue"))
    for k in range(7):
        p.append(B(f, x1 - 19.9, -330 + rng.uniform(-16, 16), 144 + rng.uniform(0, 20), 0.3, rng.uniform(4, 12), rng.uniform(3, 8),
                   rng.choice(["Glow_Warm", "Neon_Gold", "Screen_Cyan"])))
    merge(p, "Entry_XRayParts", "Entry_XRayParts", f)
    for (ya, yb) in ((-560, -400), (-260, -100)):                                     # conveyors block pawns only
        blk = box("Entry_ConveyorBlock", f, x0, x1, ya, yb, 0, 85, "Chrome")
        blk.static_mesh_component.set_editor_property("visible", False)
        _pawn_only(blk)


def no_weapons_sign():
    """Neon pistol in a red prohibition circle on the north wall, east of the dance-hall door (faces -Y)."""
    f = FE + "Security"
    cx, cz, wy = 2190, 300, -22
    box("Entry_NoGunPlate", f, cx - 60, cx + 60, wy - 2, wy, cz - 60, cz + 60, "Counter_Black", DECOR)
    mesh("Entry_NoGunRing", f, CYL, (cx, wy - 2.5, cz), (0.9, 0.9, 0.01), rot=(0, 0, 90), material="LED_Red", kind=DECOR)
    mesh("Entry_NoGunInner", f, CYL, (cx, wy - 3.2, cz), (0.78, 0.78, 0.01), rot=(0, 0, 90), material="Counter_Black", kind=DECOR)
    gy = wy - 4
    box("Entry_NoGun", f, cx - 24, cx + 20, gy - 0.5, gy + 0.5, cz + 2, cz + 13, "Neon_White", DECOR)       # slide/barrel
    box("Entry_NoGun", f, cx + 6, cx + 18, gy - 0.5, gy + 0.5, cz - 20, cz + 2, "Neon_White", DECOR)        # grip
    box("Entry_NoGun", f, cx - 3, cx + 6, gy - 0.5, gy + 0.5, cz - 6, cz - 4, "Neon_White", DECOR)          # trigger guard
    box("Entry_NoGun", f, cx - 3, cx - 1, gy - 0.5, gy + 0.5, cz - 6, cz + 2, "Neon_White", DECOR)
    mesh("Entry_NoGunSlash", f, CUBE, (cx, wy - 5, cz), (0.8, 0.01, 0.07), rot=(45, 0, 0), material="LED_Red", kind=DECOR)


def more_security():
    f = FE + "Security"
    _prop("SecurityCam", (1250, -40, 560), f, yaw=-90 - 45, s=0.16)        # over the checkpoint, looking south-east
    _prop("SecurityCam", (1950, -40, 560), f, yaw=135, s=0.16)     # looking south-west
    for k in range(3):                                                    # spare crowd barriers stacked in the corner
        _prop("CrowdBarrier", (870 + k * 12, -60 - k * 4, 95 * 0.55), f, yaw=0, s=0.55)
    no_weapons_sign()

def lights():
    WARM, MAG, CYAN = (1, 0.72, 0.45), (1, 0.1, 0.55), (0.15, 0.75, 1)
    for y in (-950, -560, -180):                                   # recessed downlights along the runner
        light("spot", "Entry_Downlight", LE, (1600, y, 590), WARM, 260, 800, rot=(-90, 0, 0),
              outer_cone_angle=38.0, inner_cone_angle=15.0, shadows=(y == -560))
    light("rect", "Entry_SignLight", LE, (2350, -870, 250), MAG, 40, 700, rot=(0, 180, 0), source_width=420.0, source_height=200.0)
    light("rect", "Entry_CCTVGlow", LE, (2350, -350, 240), (0.3, 0.5, 0.6), 12, 500, rot=(0, 180, 0), source_width=420.0, source_height=170.0)
    light("point", "Entry_DetectorGlow", LE, (1600, -330, 250), CYAN, 15, 350)
    light("spot", "Entry_CheckpointLight", LE, (1600, -330, 590), (0.85, 0.92, 1.0), 350, 800, rot=(-90, 0, 0),
          outer_cone_angle=34.0, inner_cone_angle=20.0, shadows=True)
    light("rect", "Entry_NoGunLight", LE, (2190, -60, 300), (1, 0.1, 0.1), 6, 300, rot=(0, 90, 0), source_width=120.0, source_height=120.0)
    for y in (-980, -760):
        light("point", "Entry_Pendant", LE, (2085, y, 470), (1, 0.6, 0.25), 25, 450)
    light("point", "Entry_CoatGlow", LE, (980, -870, 300), CYAN, 10, 500)
    light("spot", "Entry_CoatLight", LE, (960, -880, 480), WARM, 300, 700, rot=(-90, 0, 0), outer_cone_angle=55.0, inner_cone_angle=25.0)
    light("rect", "Entry_CoatBackLight", LE, (840, -880, 210), (1, 0.6, 0.25), 12, 500, rot=(0, 0, 0), source_width=560.0, source_height=180.0)
    light("point", "Entry_LoungeLamp", LE, (1010, -275, 300), WARM, 45, 520)
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
    metal_detector()
    xray_scanner()
    more_security()
    lights()
    fog_cards(N, 840, -1160, 2010, -40, layers=((8, 0.6), (30, 0.35)))
    spawn_marker(N, 1300, -150)        # guard post, club side of the detectors (~9 m from the player start)
    spawn_marker(N, 1900, -150)        # guard post, 6 m from the other
    r["player_start"] = player_start()
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/Entry", "Lighting/Entry"))])
    return r
