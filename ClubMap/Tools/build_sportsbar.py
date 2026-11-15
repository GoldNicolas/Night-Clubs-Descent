"""Sports Bar + Stage (6x5 tiles) east of Billiards: the second large fighting space.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_sportsbar().

Layout (cm): room x 4800..7200, y 400..2400. Door: W tiles 2-3 (y 1200..2000) = Billiards' east door.
  east   bar counter x 6800..6920 (low cover), closed behind; back bar with bottles; TVs above.
  north  stage x 5600..6400, y 1780..2380 (45 cm) with drums, amps, mics, backdrop, truss lights.
  centre LED dance floor 2x2 tiles (x 5600..6400, y 980..1780) in front of the stage.
  south  diner booths along the wall (box colliders), west: high-top tables. TVs on the walls play a live match.
"""
import math
import random
import unreal

N = "SportsBar"
X0, Y0, TW, TD = 4800, 400, 6, 5
FS = "Rooms/SportsBar/"
LS = "Lighting/SportsBar"
SZ = 45                    # stage height
TV_MAT = "/Game/ClubMap/Materials/M_CM_SportsTV"
TV_HLSL = r"""
float2 p = UV;
float3 col = lerp(float3(0.03, 0.17, 0.04), float3(0.045, 0.22, 0.05), step(0.5, frac(p.x * 8.0)));
float ln = step(abs(p.x - 0.5), 0.004) + step(abs(length((p - 0.5) * float2(1.6, 1.0)) - 0.14), 0.005);
ln += step(p.x, 0.03) * step(0.06, p.y) * step(p.y, 0.94) + step(0.97, p.x) * step(0.06, p.y) * step(p.y, 0.94);
ln += step(abs(p.y - 0.06), 0.004) + step(abs(p.y - 0.94), 0.004);
col += saturate(ln) * 0.55;
for (int i = 0; i < 12; i++) {
    float fi = (float)i;
    float2 pos = float2(0.1 + 0.8 * frac(0.13 * fi + 0.05 * T * (0.6 + frac(fi * 0.37))),
                        0.15 + 0.7 * (0.5 + 0.5 * sin(T * 0.45 + fi * 1.7)));
    float d = length((p - pos) * float2(1.6, 1.0));
    col = lerp(col, (i < 6) ? float3(0.9, 0.12, 0.1) : float3(0.15, 0.35, 1.0), step(d, 0.016));
}
float2 bp = float2(0.5 + 0.38 * sin(T * 0.6), 0.5 + 0.32 * sin(T * 1.05));
col = lerp(col, float3(1.0, 1.0, 1.0), step(length((p - bp) * float2(1.6, 1.0)), 0.009));
float bar = step(0.86, p.y) * step(p.y, 0.97) * step(0.04, p.x) * step(p.x, 0.36);
col = lerp(col, float3(0.03, 0.03, 0.08), bar);
col += bar * step(0.89, p.y) * step(p.y, 0.94) * (step(0.07, p.x) * step(p.x, 0.12) * float3(0.9, 0.15, 0.1)
       + step(0.28, p.x) * step(p.x, 0.33) * float3(0.15, 0.35, 1.0));
col *= 0.9 + 0.1 * sin(p.y * 600.0);
return col * Brightness;
"""


def _clear_room():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Rooms/SportsBar") or fp.startswith("Lighting/SportsBar"):
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
    a = prop(name, loc, yaw=yaw, s=s, sxy=sxy, sz=sz, coll=coll, label=label or ("Sports_" + name))
    a.set_folder_path(folder)
    return a


def ensure_tv_material():
    if _EAL.does_asset_exist(TV_MAT):
        return unreal.load_asset(TV_MAT)
    MELx = unreal.MaterialEditingLibrary
    m = _AT.create_asset("M_CM_SportsTV", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    cu = MELx.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", TV_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("UV", "T", "Brightness"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    MELx.connect_material_expressions(MELx.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -800, -100), "", cu, "UV")
    MELx.connect_material_expressions(MELx.create_material_expression(m, unreal.MaterialExpressionTime, -800, 0), "", cu, "T")
    br = MELx.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, 100)
    br.set_editor_property("parameter_name", "Brightness")
    br.set_editor_property("default_value", 0.7)
    MELx.connect_material_expressions(br, "", cu, "Brightness")
    MELx.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MELx.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def tv(f, cx, cy, cz, w, h, axis, sign):
    """Wall TV: black bezel + screen with the live-match material. axis 'x' = on an east/west wall, 'y' = north/south."""
    tvm = ensure_tv_material()
    a0, a1 = sorted((cx - 4 * sign, cx)) if axis == "x" else sorted((cy - 4 * sign, cy))
    b0, b1 = sorted((cx - 5 * sign, cx - 4 * sign)) if axis == "x" else sorted((cy - 5 * sign, cy - 4 * sign))
    if axis == "x":
        box("Sports_TVBezel", f, a0, a1, cy - w / 2 - 4, cy + w / 2 + 4, cz - h / 2 - 4, cz + h / 2 + 4, "Counter_Black", DECOR)
        sc = box("Sports_TVScreen", f, b0, b1, cy - w / 2, cy + w / 2, cz - h / 2, cz + h / 2, "Counter_Black", DECOR)
    else:
        box("Sports_TVBezel", f, cx - w / 2 - 4, cx + w / 2 + 4, a0, a1, cz - h / 2 - 4, cz + h / 2 + 4, "Counter_Black", DECOR)
        sc = box("Sports_TVScreen", f, cx - w / 2, cx + w / 2, b0, b1, cz - h / 2, cz + h / 2, "Counter_Black", DECOR)
    sc.static_mesh_component.set_material(0, tvm)


def shell():
    return room_shell(N, X0, Y0, TW, TD, doors={"W": [(2, 3)]}, wall_mat="Wall_Panel", floor_mat="Wood_Floor",
                      trim="Neon_Orange", portal="Neon_Magenta")


def bar():
    f = FS + "Bar"
    box("Sports_BarCounter", f, 6800, 6920, 700, 2100, 0, 112, "Wood_Dark")
    box("Sports_BarTop", f, 6790, 6930, 690, 2110, 112, 120, "Bar_Stone")
    box("Sports_BarNeon", f, 6796, 6800, 710, 2090, 100, 106, "Neon_Orange", DECOR)
    box("Sports_BarRail", f, 6770, 6775, 720, 2080, 18, 23, "Chrome", DECOR)
    for (y0, y1) in ((680, 700), (2100, 2120)):
        box("Sports_BarReturn", f, 6800, 7180, y0, y1, 0, 112, "Wood_Dark")
        box("Sports_BarReturnTop", f, 6790, 7180, y0 - 5, y1 + 5, 112, 120, "Bar_Stone")
    box("Sports_BackBar", f, 7120, 7180, 760, 2040, 0, 100, "Wood_Dark")
    box("Sports_BackGlow", f, 7176, 7180, 780, 2020, 120, 260, "Onyx_Amber", DECOR)
    rng = random.Random(61)
    p = []
    for z in (100, 150, 205):
        p.append(B(f, 7150, 1400, z - 3, 50, 1240, 3, "Wood_Dark"))
        y = 790
        while y < 2000:
            k = rng.choice(["square", "vodka", "wine", "tequila", "flask"])
            ps, w = bottle(f, 7150 + rng.uniform(-6, 6), y, z, k, rng, rng.uniform(1.1, 1.3), face=-1)
            for a in ps:
                l = a.get_actor_location()
                a.set_actor_location(unreal.Vector(l.x, y + w / 2, l.z), False, False)
            p += ps
            y += w + rng.uniform(3, 8)
    for y in range(820, 2060, 160):                                              # pints + baskets on the counter
        p += _highball(f, 6830, y, 120)
        if rng.random() < 0.4:
            p += [B(f, 6860, y + 40, 120, 22, 16, 5, "Fire_Red"), B(f, 6860, y + 40, 125, 18, 12, 2, "Glass_Amber")]
    merge(p, "Sports_BarDressing", "Sports_BarDressing", f)
    for y in (1000, 1400, 1800):
        _prop("BeerTap", (6880, y, 120 + 95 * 0.24), f, yaw=90, s=0.24)
    for k, y in enumerate(range(800, 2080, 180)):
        _prop("BarStool", (6740, y, 95 * 0.42), f, yaw=(k * 53) % 360, s=0.42, coll="pawn")
    for (y, w) in ((1000, 220), (1800, 220)):
        tv(f, 7178, y, 360, w, 125, "x", 1)


def stage():
    f = FS + "Stage"
    x0, x1, y0, y1 = 5600, 6400, 1780, 2380
    box("Sports_Stage", f, x0, x1, y0, y1, 0, SZ, "Platform_Dark")
    box("Sports_StageEdge", f, x0, x1, y0 - 4, y0, SZ - 8, SZ - 3, "Neon_Orange", DECOR)
    # backdrop curtain: vertical velvet folds
    p = []
    for k in range(40):
        x = x0 + 10 + k * 20
        p.append(B(f, x, y1 - 8 - (k % 2) * 6, SZ, 20, 6, 420, "Velvet_Red"))
    # truss bar over the front edge with 4 can lights
    p.append(B(f, (x0 + x1) / 2, y0 + 60, 470, x1 - x0 + 40, 25, 25, "Steel"))
    for dx in (-300, -100, 100, 300):
        p += [C(f, (x0 + x1) / 2 + dx, y0 + 60, 430, 40, 22, "Counter_Black"), C(f, (x0 + x1) / 2 + dx, y0 + 60, 428, 2, 18, "Glow_Warm")]
    for x in (x0 - 20, x1 + 20):
        p.append(B(f, x, y0 + 60, 0, 20, 20, 470, "Steel"))
    # drum kit (centre back)
    dx, dy, dz = 6000, 2200, SZ
    p.append(mesh("p", f, CYL, (dx, dy - 30, dz + 30), (0.55, 0.55, 0.4), rot=(0, 0, 90), material="Ball_Red", kind=DECOR))       # kick
    p.append(mesh("p", f, CYL, (dx, dy - 50.5, dz + 30), (0.5, 0.5, 0.01), rot=(0, 0, 90), material="Label_White", kind=DECOR))
    for (ox, oy, h, d) in ((-45, -60, 65, 35), (45, -60, 65, 35), (-70, 10, 45, 42), (60, 20, 55, 30)):
        p += [C(f, dx + ox, dy + oy, dz + h, 22, d, "Ball_Red"), C(f, dx + ox, dy + oy, dz + h + 22, 0.5, d, "Label_White"),
              C(f, dx + ox, dy + oy, dz, h, 2, "Chrome")]
    for (ox, oy, h) in ((-90, -40, 120), (85, -45, 125), (100, 40, 110)):
        p += [C(f, dx + ox, dy + oy, dz, h, 2, "Chrome"), C(f, dx + ox, dy + oy, dz + h, 0.8, 42, "Gold")]
    p += [C(f, dx, dy + 60, dz, 50, 4, "Chrome"), C(f, dx, dy + 60, dz + 50, 8, 36, "Counter_Black")]                                 # throne
    # mic stands, guitar on a stand, monitor wedges
    for x in (5780, 6220):
        p += [C(f, x, y0 + 120, SZ, 2, 28, "Counter_Black"), C(f, x, y0 + 120, SZ, 150, 2, "Chrome"),
              mesh("p", f, CYL, (x, y0 + 110, SZ + 155), (0.04, 0.04, 0.2), rot=(60, 90, 0), material="Counter_Black", kind=DECOR)]
        p += [B(f, x, y0 + 40, SZ, 60, 40, 30, "Counter_Black", pitch=0), B(f, x, y0 + 21, SZ + 8, 48, 1, 16, "Plastic_Grey")]
    gx, gy = 6280, 2050
    p += [B(f, gx, gy, SZ, 30, 20, 8, "Counter_Black"), B(f, gx, gy, SZ + 8, 4, 4, 50, "Counter_Black"),
          mesh("p", f, CUBE, (gx, gy - 6, SZ + 75), (0.38, 0.08, 0.5), rot=(0, 0, 8), material="Fire_Red", kind=DECOR),
          mesh("p", f, CUBE, (gx + 4, gy - 6, SZ + 135), (0.06, 0.03, 0.75), rot=(0, 0, 8), material="Wood_Dark", kind=DECOR)]
    merge(p, "Sports_StageGear", "Sports_StageGear", f)
    for x in (5700, 6300):
        _prop("SpeakerStack", (x, 2250, SZ + 95 * 0.7), f, yaw=180, s=0.7)
    _hidden("Sports_SpeakerCollider", f, 5640, 5760, 2190, 2310, SZ, SZ + 135)
    _hidden("Sports_SpeakerCollider", f, 6240, 6360, 2190, 2310, SZ, SZ + 135)
    _hidden("Sports_DrumCollider", f, 5880, 6120, 2120, 2290, SZ, SZ + 110)
    # stage-side steps (two 22 cm steps) on both sides
    for (sx0, sx1) in ((5520, 5600), (6400, 6480)):
        box("Sports_StageStep", f, sx0, sx1, 1900, 2100, 0, 22, "Platform_Dark")


def dance_floor():
    f = FS + "DanceFloor"
    base = ensure_tile_material()
    for i in range(2):
        for j in range(2):
            x, y = 5600 + i * 400, 980 + j * 400
            a = box("Sports_DanceTile", f, x + 8, x + 392, y + 8, y + 392, 0, 2, "Counter_Black", DECOR)
            d = i + j
            cn = ["Magenta", "Cyan", "Blue"][d % 3]
            a.static_mesh_component.set_material(0, unreal.load_asset("%s/MI_CM_T_%s_%d" % (MAT_DIR, cn, d)))
    for (a0, a1, b0, b1) in ((5590, 6410, 970, 980), (5590, 6410, 1780, 1790), (5590, 5600, 980, 1780), (6400, 6410, 980, 1780)):
        box("Sports_DanceEdge", f, a0, a1, b0, b1, 0, 3, "Neon_Orange", DECOR)


def seating():
    f = FS + "Seating"
    rng = random.Random(14)
    p = []
    # diner booths along the south wall: bench / table / bench, each pair with a hanging lamp
    for k in range(5):
        bx = 4950 + k * 320
        p += [B(f, bx, 450, 0, 200, 60, 45, "Leather_Wine"), B(f, bx, 425, 45, 200, 10, 70, "Leather_Wine"),          # south bench + back
              B(f, bx, 640, 0, 200, 60, 45, "Leather_Wine"), B(f, bx, 665, 45, 200, 10, 70, "Leather_Wine"),          # north bench + back
              B(f, bx, 545, 72, 180, 80, 4, "Wood_Dark"), B(f, bx, 545, 0, 10, 10, 72, "Chrome")]
        p += _highball(f, bx - 40, 540, 76) + _highball(f, bx + 30, 555, 76)
        if k % 2 == 0:
            p += [B(f, bx, 540, 76, 30, 22, 5, "Fire_Red"), B(f, bx, 540, 81, 26, 18, 2, "Glass_Amber")]
        p += [C(f, bx, 545, 230, 22, 40, "Fire_Red"), C(f, bx, 545, 228, 2, 30, "Glow_Warm"), B(f, bx, 545, 252, 1, 1, 348, "Chrome")]
        _hidden("Sports_BoothCollider", f, bx - 100, bx + 100, 420, 700, 0, 115)
    merge(p, "Sports_Booths", "Sports_Booths", f)
    # high-top tables in the west area (pawn-only) with stools
    for (x, y) in ((5100, 1100), (5100, 1900), (5300, 1500)):
        _prop("CocktailTable", (x, y, 94 * 0.58), f, sxy=0.4, sz=0.58, coll="pawn")
        for a in (30, 210):
            _prop("BarStool", (x + 55 * math.cos(math.radians(a)), y + 55 * math.sin(math.radians(a)), 95 * 0.42), f,
                  yaw=rng.uniform(0, 360), s=0.42, coll="pawn")
    q = []
    for (x, y) in ((5100, 1100), (5100, 1900), (5300, 1500)):
        q += _highball(f, x - 12, y + 8, 109) + _highball(f, x + 14, y - 6, 109)
    merge(q, "Sports_HighTopDrinks", "Sports_HighTopDrinks", f)
    # TVs on the south wall above the booths and on the west wall, a neon basketball sign by the door
    for x in (5110, 5750, 6390):
        tv(f, x, 422, 360, 240, 135, "y", -1)
    tv(f, 4822, 800, 340, 200, 115, "x", -1)
    sx, sy, sz = 4823, 2180, 330                                                  # basketball: ring + seams (faces +X)
    for (d, m, dx) in ((90, "Neon_Orange", 0), (84, "Wall_Panel", 0.6)):
        mesh("Sports_Ball", f, CYL, (sx + dx, sy, sz), (d / 100, d / 100, 0.006), rot=(90, 0, 0), material=m, kind=DECOR)
    box("Sports_Ball", f, sx + 1, sx + 2, sy - 42, sy + 42, sz - 1.5, sz + 1.5, "Neon_Orange", DECOR)
    box("Sports_Ball", f, sx + 1, sx + 2, sy - 1.5, sy + 1.5, sz - 42, sz + 42, "Neon_Orange", DECOR)
    _prop("WallSpeaker", (4837, 1000, 430), f, yaw=-90, s=0.3)
    _prop("WallSpeaker", (7163, 2250, 430), f, yaw=90, s=0.3)
    _prop("SecurityCam", (7155, 445, 560), f, yaw=45, s=0.16)


def lights():
    WARM, ORANGE = (1, 0.7, 0.4), (1, 0.45, 0.1)
    for (x, c) in ((5750, (1, 0.2, 0.5)), (6000, (1, 0.95, 0.85)), (6250, (0.2, 0.6, 1.0))):
        light("spot", "Sports_StageSpot", LS, (x, 1840, 430), c, 500, 900, rot=(-55, 90, 0),
              outer_cone_angle=20.0, inner_cone_angle=10.0, shadows=(x == 6000), volumetric_scattering_intensity=2.0)
    for y in (900, 1400, 1900):
        light("point", "Sports_BarPendant", LS, (6860, y, 420), WARM, 30, 520)
    light("rect", "Sports_BackBarLight", LS, (7140, 1400, 190), WARM, 30, 650, rot=(0, 180, 0), source_width=1200.0, source_height=150.0)
    for x in (4950, 5590, 6230):
        light("point", "Sports_BoothLamp", LS, (x + 320 * 0, 545, 215), WARM, 15, 380)
    light("point", "Sports_Fill", LS, (5300, 1500, 520), (1, 0.75, 0.5), 40, 1100)
    light("point", "Sports_Fill", LS, (6400, 900, 520), (1, 0.75, 0.5), 30, 900)
    light("rect", "Sports_SignLight", LS, (4860, 2180, 330), ORANGE, 10, 400, rot=(0, 0, 0), source_width=120.0, source_height=120.0)


def build_sportsbar():
    r = {"cleared": _clear_room()}
    shell()
    bar()
    stage()
    dance_floor()
    seating()
    lights()
    fog_cards(N, 4840, 720, 6760, 1760, layers=((8, 0.5), (30, 0.3)))
    for (x, y) in ((5100, 760), (5050, 2150), (6550, 800), (6550, 1950)):
        spawn_marker(N, x, y)
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if str(a.get_folder_path()).startswith(("Rooms/SportsBar", "Lighting/SportsBar"))])
    return r
