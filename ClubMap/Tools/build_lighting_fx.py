"""Moving lights + floor fog for the Main Dance Hall. Run with include=[cm_kit.py, this file]:
  pulse_tiles_and_leds()   pulsing emissive (M_CM_Pulse): diagonal wave on the dance floor, sweep across the LED wall
  sweep_beams()            BP_CM_SweepBeam (spinning moving-head fixture + tilted spot) replaces the static MovingHead props
  floor_fog()              light volumetric haze + drifting translucent fog sheets low over the floor (fog_cards per room)
All of it is visual only: no collision, nothing the AI traces against.
"""
import math
import unreal

PULSE = "/Game/ClubMap/Materials/M_CM_Pulse"
BEAM_BP = "/Game/ClubMap/Blueprints/BP_CM_SweepBeam"
MEL = unreal.MaterialEditingLibrary


def ensure_pulse_master():
    """Emissive = EmissiveColor * EmissiveStrength * lerp(PulseMin, 1, 0.5 + 0.5 * sin(2pi * (Time * PulseSpeed + Phase)))."""
    if _EAL.does_asset_exist(PULSE):
        return unreal.load_asset(PULSE)
    m = _AT.create_asset("M_CM_Pulse", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def sp(name, val, x, y):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", val)
        return e

    def vp(name, val, x, y):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", unreal.LinearColor(*val))
        return e

    bc = vp("BaseColor", (0.02, 0.0, 0.01, 1), -400, -400)
    MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(sp("Metallic", 0.0, -400, -300), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(sp("Roughness", 0.3, -400, -200), "", unreal.MaterialProperty.MP_ROUGHNESS)

    t = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -1400, 100)
    spd = sp("PulseSpeed", 0.5, -1400, 200)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1200, 150)
    MEL.connect_material_expressions(t, "", mul, "A")
    MEL.connect_material_expressions(spd, "", mul, "B")
    ph = sp("Phase", 0.0, -1200, 300)
    add = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -1000, 200)
    MEL.connect_material_expressions(mul, "", add, "A")
    MEL.connect_material_expressions(ph, "", add, "B")
    sn = MEL.create_material_expression(m, unreal.MaterialExpressionSine, -850, 200)   # period 1 -> sin(2pi x)
    MEL.connect_material_expressions(add, "", sn, "")
    bs = MEL.create_material_expression(m, unreal.MaterialExpressionConstantBiasScale, -700, 200)
    bs.set_editor_property("bias", 1.0)
    bs.set_editor_property("scale", 0.5)
    MEL.connect_material_expressions(sn, "", bs, "")
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -550, 200)
    lp.set_editor_property("const_b", 1.0)
    MEL.connect_material_expressions(sp("PulseMin", 0.25, -700, 350), "", lp, "A")
    MEL.connect_material_expressions(bs, "", lp, "Alpha")
    ec = vp("EmissiveColor", (1, 0.1, 0.55, 1), -550, 0)
    es = sp("EmissiveStrength", 0.35, -550, 100)
    m1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 50)
    MEL.connect_material_expressions(ec, "", m1, "A")
    MEL.connect_material_expressions(es, "", m1, "B")
    m2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 100)
    MEL.connect_material_expressions(m1, "", m2, "A")
    MEL.connect_material_expressions(lp, "", m2, "B")
    MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def pulse_mi(name, palette_key, phase, speed, pmin, strength_mult=1.0):
    """MI_CM_P_<name>: the palette colour of <palette_key>, pulsing."""
    path = "%s/MI_CM_P_%s" % (MAT_DIR, name)
    if _EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = _AT.create_asset("MI_CM_P_" + name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, ensure_pulse_master())
    bc, met, rough, ec, es = PALETTE[palette_key]
    MEL.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(bc[0], bc[1], bc[2], 1))
    MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", met)
    MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(ec[0], ec[1], ec[2], 1))
    MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", es * strength_mult)
    MEL.set_material_instance_scalar_parameter_value(mi, "Phase", phase)
    MEL.set_material_instance_scalar_parameter_value(mi, "PulseSpeed", speed)
    MEL.set_material_instance_scalar_parameter_value(mi, "PulseMin", pmin)
    _EAL.save_loaded_asset(mi)
    return mi


def _set_mat(a, mi):
    c = a.static_mesh_component
    c.modify()
    c.set_material(0, mi)


def pulse_tiles_and_leds():
    tiles = [a for a in _EAS.get_all_level_actors() if a.get_actor_label() == "DanceTile"]
    cols = ["Tile_Magenta", "Tile_Cyan", "Tile_Blue"]
    n = 0
    for a in tiles:
        l = a.get_actor_location()
        i, j = int((l.x - 800) // 400), int((l.y - 800) // 400)
        d = i + j                                        # 0..6 diagonal index -> travelling wave
        col = cols[(i + j) % 3]
        _set_mat(a, pulse_mi("%s_%d" % (col, d), col, phase=-d / 7.0, speed=1.0, pmin=0.15, strength_mult=1.0))
        n += 1
    leds = [a for a in _EAS.get_all_level_actors() if a.get_actor_label() == "LED_Panel"]
    scr = ["Screen_Blue", "Screen_Pink", "Screen_Cyan"]
    m = 0
    for a in leds:
        l = a.get_actor_location()
        i = int((l.x - 800) // 270)
        k = 0 if l.z < 360 else 1
        col = scr[(i + k) % 3]
        _set_mat(a, pulse_mi("%s_%d" % (col, i), col, phase=-i / 6.0, speed=0.35, pmin=0.45))
        m += 1
    return {"tiles": n, "led_panels": m}


def sweep_beams():
    """Swap the 4 static MovingHead props for spinning BP_CM_SweepBeam fixtures (alternating colour and direction)."""
    old = [a for a in _EAS.get_all_level_actors() if a.get_actor_label() == "MovingHead"]
    spots = [(a.get_actor_location(), a.get_actor_rotation().yaw) for a in old]
    for a in old:
        _EAS.destroy_actor(a)
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "SweepBeam":
            spots = spots or []
            _EAS.destroy_actor(a)
    if not spots:   # re-run: original MovingHead positions
        spots = [(unreal.Vector(x, y, 549.6), yaw) for (x, y, yaw) in
                 ((1000, 1000, 45), (2200, 1000, 135), (1000, 2200, -45), (2200, 2200, -135))]
    cls = unreal.EditorAssetLibrary.load_blueprint_class(BEAM_BP)
    colors = [(1.0, 0.10, 0.55), (0.15, 0.75, 1.0), (0.15, 0.75, 1.0), (1.0, 0.10, 0.55)]
    rates = [42, -36, 30, -48]
    out = []
    for k, (loc, yaw) in enumerate(spots):
        a = _EAS.spawn_actor_from_class(cls, unreal.Vector(loc.x, loc.y, 575), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
        a.set_actor_label("SweepBeam")
        a.set_folder_path("Lighting/DanceHall/Moving")
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            c.set_collision_profile_name("NoCollision")
        for c in a.get_components_by_class(unreal.SpotLightComponent):
            r, g, b = colors[k % 4]
            c.set_light_color(unreal.LinearColor(r, g, b, 1))
            c.set_intensity(6000.0)                                   # candelas
            c.set_volumetric_scattering_intensity(10.0)               # strong shaft in the haze
            c.set_editor_property("outer_cone_angle", 7.0)
            c.set_editor_property("inner_cone_angle", 2.0)
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if c.get_name().startswith("Lens"):                       # glowing lens in the beam colour
                c.set_material(0, mat("Neon_Magenta" if k % 3 == 0 else "Neon_Cyan"))
        for c in a.get_components_by_class(unreal.RotatingMovementComponent):
            c.set_editor_property("rotation_rate", unreal.Rotator(pitch=0, yaw=rates[k % 4], roll=0))
        out.append(a.get_actor_label())
    return {"beams": len(out)}


FOG_MAT = "/Game/ClubMap/Materials/M_CM_FloorFog"


def ensure_fog_material():
    """Translucent, lit by the room's lights (volumetric non-directional); opacity = two drifting noise layers x depth fade."""
    if _EAL.does_asset_exist(FOG_MAT):
        return unreal.load_asset(FOG_MAT)
    m = _AT.create_asset("M_CM_FloorFog", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)

    def sp(name, val, x, y):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", val)
        return e

    col = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -300, -300)
    col.set_editor_property("parameter_name", "FogColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.9, 1))
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -1600, 200)
    layers = []
    for k, (scale, wind) in enumerate(((0.0015, (14, 6, 0)), (0.004, (-9, 11, 0)))):
        w = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -1600, 300 + k * 200)
        w.set_editor_property("constant", unreal.LinearColor(wind[0], wind[1], wind[2], 1))
        wm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1400, 250 + k * 200)
        MEL.connect_material_expressions(tm, "", wm, "A")
        MEL.connect_material_expressions(w, "", wm, "B")
        pa = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -1200, 100 + k * 200)
        MEL.connect_material_expressions(wp, "", pa, "A")
        MEL.connect_material_expressions(wm, "", pa, "B")
        nz = MEL.create_material_expression(m, unreal.MaterialExpressionNoise, -1000, 100 + k * 200)
        nz.set_editor_property("scale", scale)
        nz.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX)
        nz.set_editor_property("levels", 3)
        nz.set_editor_property("output_min", 0.0)
        nz.set_editor_property("output_max", 1.0)
        MEL.connect_material_expressions(pa, "", nz, "Position")
        layers.append(nz)
    mix = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 200)
    MEL.connect_material_expressions(layers[0], "", mix, "A")
    MEL.connect_material_expressions(layers[1], "", mix, "B")
    op = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 200)
    MEL.connect_material_expressions(mix, "", op, "A")
    MEL.connect_material_expressions(sp("Opacity", 0.6, -800, 350), "", op, "B")
    df = MEL.create_material_expression(m, unreal.MaterialExpressionDepthFade, -400, 200)
    df.set_editor_property("fade_distance_default", 60.0)
    MEL.connect_material_expressions(op, "", df, "Opacity")
    MEL.connect_material_property(df, "", unreal.MaterialProperty.MP_OPACITY)
    # soft self-glow so the fog reads as pale purple haze even where little light reaches the floor
    gc = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -300, -500)
    gc.set_editor_property("parameter_name", "GlowColor")
    gc.set_editor_property("default_value", unreal.LinearColor(0.55, 0.35, 0.9, 1))
    gm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, -450)
    MEL.connect_material_expressions(gc, "", gm, "A")
    MEL.connect_material_expressions(sp("Glow", 0.35, -300, -400), "", gm, "B")
    MEL.connect_material_property(gm, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def fog_mi(name, opacity, color=(0.8, 0.8, 0.9)):
    path = "%s/MI_CM_FloorFog_%s" % (MAT_DIR, name)
    if _EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = _AT.create_asset("MI_CM_FloorFog_" + name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, ensure_fog_material())
    MEL.set_material_instance_scalar_parameter_value(mi, "Opacity", opacity)
    MEL.set_material_instance_vector_parameter_value(mi, "FogColor", unreal.LinearColor(color[0], color[1], color[2], 1))
    _EAL.save_loaded_asset(mi)
    return mi


def fog_cards(room, x0, y0, x1, y1, layers=((8, 1.0), (30, 0.7), (55, 0.4))):
    """Stacked translucent fog sheets low over a room's floor. Decor: no collision, no shadows."""
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "FloorFog_%s" % room:
            _EAS.destroy_actor(a)
    out = 0
    for (z, o) in layers:
        mesh("FloorFog_%s" % room, "Lighting/%s/Fog" % room, "/Engine/BasicShapes/Plane", ((x0 + x1) / 2, (y0 + y1) / 2, z),
             ((x1 - x0) / 100, (y1 - y0) / 100, 1), rot=(0, (z * 7) % 360, 0), kind=DECOR)
        a = [b for b in _EAS.get_all_level_actors() if b.get_actor_label() == "FloorFog_%s" % room][-1]
        a.static_mesh_component.set_material(0, fog_mi("%s_L%d" % (room, z), o))     # per room: rooms can differ
        a.static_mesh_component.set_editor_property("cast_shadow", False)
        out += 1
    return out


def floor_fog():
    """(Local Fog Volumes did not render here, so the floor fog is translucent sheets.) Light room haze (volumetric fog, so beams read in the air) + fog sheets hugging the dance-hall floor."""
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "FloorFog_DanceHall" and a.get_class().get_name() == "LocalFogVolume":
            _EAS.destroy_actor(a)
    hf = [a for a in _EAS.get_all_level_actors() if a.get_class().get_name() == "ExponentialHeightFog"][0]
    c = hf.component
    c.modify()
    c.set_editor_property("enable_volumetric_fog", True)
    c.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
    sd = c.get_editor_property("second_fog_data")
    sd.set_editor_property("fog_density", 0.06)       # slight extra haze toward the floor
    sd.set_editor_property("fog_height_falloff", 2.0)
    sd.set_editor_property("fog_height_offset", 0.0)
    c.set_editor_property("second_fog_data", sd)
    n = fog_cards("DanceHall", 40, 40, 3160, 3160)
    return {"fog_sheets": n}


# ---------------------------------------------------------------- DJ LED video wall
LED_MAT = "/Game/ClubMap/Materials/M_CM_LEDWall"
LED_HLSL = r"""
float u = saturate((WP.x - X0) / W);
float v = saturate((WP.z - Z0) / H);
// LED pixel grid
float2 pg = frac(float2(WP.x, WP.z) / Pitch);
float dotm = lerp(GridMin, 1.0, step(0.2, pg.x) * step(0.2, pg.y));
// equalizer: segmented bars, magenta at the top fading to cyan at the bottom
float cols = 28.0;
float c = floor(u * cols);
float cu = frac(u * cols);
float h = 0.18 + 0.7 * abs(sin(T * 2.1 + c * 1.37) * sin(T * 0.83 + c * 0.61));
float inCol = step(0.14, cu) * step(cu, 0.86);
float seg = step(0.25, frac(v * 26.0));
float bar = step(v, h) * inCol * seg;
float3 bot = float3(0.10, 0.70, 1.00);
float3 top = float3(1.00, 0.08, 0.55);
float3 barCol = lerp(bot, top, saturate(v / max(h, 0.05)));
float cap = step(abs(v - (h + 0.04)), 0.012) * inCol;
// background: dark purple with slow diagonal light sweeps
float sweep = pow(saturate(sin((u * 2.5 - v * 1.1 - T * 0.22) * 6.2831) * 0.5 + 0.5), 8.0);
float3 bg = lerp(float3(0.03, 0.0, 0.07), float3(0.30, 0.04, 0.55), sweep);
float3 col = bg + barCol * bar * 1.3 + cap * float3(1.0, 0.95, 1.0) * 1.5;
return col * dotm * Brightness;
"""


def ensure_led_material():
    """Emissive LED video wall in world space (continuous across the cabinets): equalizer + sweeps + pixel grid."""
    if _EAL.does_asset_exist(LED_MAT):
        unreal.EditorAssetLibrary.delete_asset(LED_MAT)
    m = _AT.create_asset("M_CM_LEDWall", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    cu = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", LED_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["WP", "T", "X0", "Z0", "W", "H", "Pitch", "Brightness", "GridMin"]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -800, -200)
    MEL.connect_material_expressions(wp, "", cu, "WP")
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -800, -100)
    MEL.connect_material_expressions(tm, "", cu, "T")
    defaults = {"X0": 806.0, "Z0": 180.0, "W": 1608.0, "H": 374.0, "Pitch": 3.0, "Brightness": 1.2, "GridMin": 0.8}
    for k, n in enumerate(names[2:]):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, k * 90)
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", defaults[n])
        MEL.connect_material_expressions(e, "", cu, n)
    MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def led_wall():
    """Put the video-wall material on the 12 LED cabinets, add a black bezel, retint the wall light."""
    for a in _EAS.get_all_level_actors():          # release the old material so it can be rebuilt
        if a.get_actor_label() == "LED_Panel":
            _set_mat(a, mat("Speaker_Black"))
    m = ensure_led_material()
    n = 0
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "LED_Panel":
            _set_mat(a, m)
            n += 1
    clear_labels = ("LED_Bezel",)
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() in clear_labels:
            _EAS.destroy_actor(a)
    f = "Rooms/DanceHall/DJ"
    for (x0, x1, z0, z1) in ((790, 2430, 168, 180), (790, 2430, 554, 566), (790, 806, 168, 566), (2414, 2430, 168, 566)):
        box("LED_Bezel", f, x0, x1, 3170, 3180, z0, z1, "Speaker_Black", DECOR)
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "LEDWall_Light":
            c = a.get_component_by_class(unreal.RectLightComponent)
            c.modify()
            c.set_light_color(unreal.LinearColor(0.55, 0.15, 1.0, 1))
    return {"panels": n}


# ---------------------------------------------------------------- mirror ball (spinning, throws light dots)
MB_BP = "/Game/ClubMap/Blueprints/BP_CM_MirrorBall"
MB_LF = "/Game/ClubMap/Materials/M_CM_MirrorBallDots"
MB_HLSL = r"""
float3 d = normalize(L);
float theta = atan2(d.y, d.x);
float phi = acos(clamp(d.z, -1.0, 1.0));
float rows = Rows;
float r = phi / 3.14159 * rows;
float ri = floor(r);
float ring = max(1.0, floor(sin((ri + 0.5) / rows * 3.14159) * rows * 2.0));
float c = (theta / 6.28318 + 0.5) * ring;
float2 cell = float2(frac(c), frac(r)) - 0.5;
float dotv = 1.0 - smoothstep(DotSize * 0.6, DotSize, length(cell));
float h = frac(sin(dot(float2(floor(c), ri), float2(12.9898, 78.233))) * 43758.5453);
return dotv * step(Dropout, h);
"""


def ensure_mirrorball_lightfunction():
    """Light-function material: a sphere of dots in the light's own space, so the dots turn with the ball."""
    if _EAL.does_asset_exist(MB_LF):
        return unreal.load_asset(MB_LF)
    m = _AT.create_asset("M_CM_MirrorBallDots", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    cu = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", MB_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    names = ["L", "Rows", "DotSize", "Dropout"]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    lv = MEL.create_material_expression(m, unreal.MaterialExpressionLightVector, -800, -100)
    MEL.connect_material_expressions(lv, "", cu, "L")
    for k, (n, v) in enumerate((("Rows", 26.0), ("DotSize", 0.22), ("Dropout", 0.3))):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, k * 90)
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", v)
        MEL.connect_material_expressions(e, "", cu, n)
    MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


def mirror_ball():
    """Replace the static MirrorBall prop with BP_CM_MirrorBall: spins 18 deg/s, inner point light throws dots, pin spot on it."""
    lf = ensure_mirrorball_lightfunction()
    loc = unreal.Vector(1600, 1600, 470)
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() in ("MirrorBall", "MirrorBall_Spinning", "MirrorBall_PinSpot"):
            if a.get_actor_label() == "MirrorBall":
                loc = a.get_actor_location()
            _EAS.destroy_actor(a)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(MB_BP)
    a = _EAS.spawn_actor_from_class(cls, loc)
    a.set_actor_label("MirrorBall_Spinning")
    a.set_folder_path("Lighting/DanceHall/Moving")
    # mesh, dot light (light function), spin rate live in the Blueprint defaults (instance overrides get reset)
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        c.set_collision_profile_name("NoCollision")
    # pin spot from the ceiling aimed at the ball so it sparkles
    light("spot", "MirrorBall_PinSpot", "Lighting/DanceHall/Moving", (1600, 1200, 590), (1, 1, 1), 600, 900,
          rot=(-math.degrees(math.atan2(120, 400)), 90, 0), outer_cone_angle=4.0, inner_cone_angle=1.5,
          volumetric_scattering_intensity=0.6)
    return {"ball": a.get_actor_label()}


# ---------------------------------------------------------------- dance tiles: glossy LED glass with inner glow + frame
TILE_MAT = "/Game/ClubMap/Materials/M_CM_DanceTile"
TILE_HLSL = r"""
float2 l = frac((WP.xy - Origin) / Size);
float2 e = min(l, 1.0 - l);
float edge = min(e.x, e.y);
float frame = smoothstep(0.035, 0.045, edge) * (1.0 - smoothstep(0.055, 0.065, edge));
float center = 1.0 - saturate(length(l - 0.5) * 1.5);
float pulse = lerp(PMin, 1.0, 0.5 + 0.5 * sin(6.28318 * (T * Speed + Phase)));
float body = (0.15 + 0.85 * center * center) * pulse;
float seam = smoothstep(0.0, 0.015, edge);
return Color * (body + frame * (0.5 + 0.8 * pulse)) * Strength * seam;
"""


def ensure_tile_material():
    if _EAL.does_asset_exist(TILE_MAT):
        return unreal.load_asset(TILE_MAT)
    m = _AT.create_asset("M_CM_DanceTile", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    bc = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -400)
    bc.set_editor_property("constant", unreal.LinearColor(0.004, 0.004, 0.006, 1))
    MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    ro = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, -300)
    ro.set_editor_property("r", 0.12)                                     # glossy glass: reflects beams and dots
    MEL.connect_material_property(ro, "", unreal.MaterialProperty.MP_ROUGHNESS)
    cu = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", TILE_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["WP", "T", "Color", "Origin", "Size", "Phase", "Speed", "PMin", "Strength"]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    MEL.connect_material_expressions(MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, -200), "", cu, "WP")
    MEL.connect_material_expressions(MEL.create_material_expression(m, unreal.MaterialExpressionTime, -900, -120), "", cu, "T")
    col = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, -40)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1, 0.1, 0.55, 1))
    MEL.connect_material_expressions(col, "", cu, "Color")
    for k, (n, v) in enumerate((("Origin", 800.0), ("Size", 400.0), ("Phase", 0.0), ("Speed", 1.0), ("PMin", 0.2), ("Strength", 0.9))):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 60 + k * 90)
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", v)
        MEL.connect_material_expressions(e, "", cu, n)
    MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    _EAL.save_loaded_asset(m)
    return m


TILE_COLORS = {"Magenta": (1.0, 0.08, 0.55), "Cyan": (0.10, 0.70, 1.0), "Blue": (0.25, 0.20, 1.0)}


def dance_tiles():
    """Same diagonal wave and colour pattern, new LED-glass look."""
    base = ensure_tile_material()
    names = ["Magenta", "Cyan", "Blue"]
    n = 0
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() != "DanceTile":
            continue
        l = a.get_actor_location()
        i, j = int((l.x - 800) // 400), int((l.y - 800) // 400)
        d = i + j
        cn = names[d % 3]
        path = "%s/MI_CM_T_%s_%d" % (MAT_DIR, cn, d)
        if _EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = _AT.create_asset("MI_CM_T_%s_%d" % (cn, d), MAT_DIR, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
            MEL.set_material_instance_parent(mi, base)
        r, g, b = TILE_COLORS[cn]
        MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1))
        MEL.set_material_instance_scalar_parameter_value(mi, "Phase", -d / 7.0)
        MEL.set_material_instance_scalar_parameter_value(mi, "Speed", 1.0)
        MEL.set_material_instance_scalar_parameter_value(mi, "Strength", 1.6)
        MEL.set_material_instance_scalar_parameter_value(mi, "PMin", 0.3)
        _EAL.save_loaded_asset(mi)
        _set_mat(a, mi)
        n += 1
    return {"tiles": n}
