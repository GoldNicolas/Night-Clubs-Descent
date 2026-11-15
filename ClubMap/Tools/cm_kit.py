"""ClubMap room kit: builds rooms that follow docs/MAP_DESIGN.md and the FPS-Agent level rules.

Run inside the Unreal Editor (Unreal-MCP python-execute with include=[this file]).
Units: cm. Grid: 4 m tiles. Walls: 6 m. One floor level per tile.
Collision:
  - structure / cover (walls, floors, counters, platforms, pillars): WorldStatic, BlockAll (blocks Weapon too)
  - ceilings: object type Ceiling (ECC_GameTraceChannel4), blocks everything, ignored by agent perception
  - decor (neon, coves, trims, screens, hanging frames): NoCollision
"""
import math
import unreal

TILE = 400.0
WALL_H = 600.0
WALL_T = 40.0
DOOR_H = 400.0
MAT_DIR = "/Game/ClubMap/Materials"
MASTER = MAT_DIR + "/M_CM_Surface"
CUBE = "/Engine/BasicShapes/Cube"
CYL = "/Engine/BasicShapes/Cylinder"

_EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_AT = unreal.AssetToolsHelpers.get_asset_tools()
_MEL = unreal.MaterialEditingLibrary
_EAL = unreal.EditorAssetLibrary

# ---------------------------------------------------------------- materials
# name: (base rgb, metallic, roughness, emissive rgb or None, emissive strength)
PALETTE = {
    "Floor_Gloss":   ((0.010, 0.010, 0.013), 0.3, 0.08, None, 0),
    "Wall_Panel":    ((0.014, 0.016, 0.024), 0.5, 0.40, None, 0),
    "Wall_Concrete": ((0.060, 0.060, 0.065), 0.0, 0.85, None, 0),
    "Ceiling_Dark":  ((0.008, 0.008, 0.010), 0.0, 0.90, None, 0),
    "Counter_Black": ((0.010, 0.010, 0.012), 0.2, 0.15, None, 0),
    "Platform_Dark": ((0.020, 0.020, 0.028), 0.3, 0.30, None, 0),
    "Chrome":        ((0.800, 0.800, 0.820), 1.0, 0.15, None, 0),
    "Gold":          ((1.000, 0.650, 0.300), 1.0, 0.30, None, 0),
    "Leather_Wine":  ((0.060, 0.006, 0.020), 0.0, 0.50, None, 0),
    "Speaker_Black": ((0.012, 0.012, 0.014), 0.1, 0.70, None, 0),
    "Neon_Magenta":  ((0, 0, 0), 0.0, 0.5, (1.00, 0.10, 0.55), 12),
    "Neon_Cyan":     ((0, 0, 0), 0.0, 0.5, (0.15, 0.75, 1.00), 12),
    "Neon_Blue":     ((0, 0, 0), 0.0, 0.5, (0.10, 0.25, 1.00), 12),
    "Neon_Gold":     ((0, 0, 0), 0.0, 0.5, (1.00, 0.60, 0.25), 8),
    "Neon_White":    ((0, 0, 0), 0.0, 0.5, (1.00, 0.95, 0.90), 6),
    "Screen_Blue":   ((0, 0, 0), 0.0, 0.5, (0.20, 0.35, 1.00), 0.8),
    "Screen_Pink":   ((0, 0, 0), 0.0, 0.5, (1.00, 0.20, 0.70), 0.7),
    "Screen_Cyan":   ((0, 0, 0), 0.0, 0.5, (0.20, 0.80, 1.00), 0.6),
    "Tile_Magenta":  ((0.02, 0.0, 0.01), 0.0, 0.30, (1.00, 0.10, 0.55), 0.35),
    "Tile_Cyan":     ((0.0, 0.02, 0.02), 0.0, 0.30, (0.15, 0.75, 1.00), 0.25),
    "Tile_Blue":     ((0.0, 0.0, 0.02), 0.0, 0.30, (0.15, 0.25, 1.00), 0.4),
    "Bottles_Glow":  ((0, 0, 0), 0.0, 0.5, (0.10, 0.45, 1.00), 0.18),
    # bar glass + labels (glass gets a faint glow so it reads as backlit in the dark)
    "Glass_Amber":   ((0.30, 0.10, 0.01), 0.0, 0.05, (1.00, 0.45, 0.10), 0.10),
    "Glass_Green":   ((0.02, 0.10, 0.03), 0.0, 0.05, (0.20, 1.00, 0.35), 0.06),
    "Glass_Clear":   ((0.12, 0.14, 0.16), 0.6, 0.04, (0.60, 0.85, 1.00), 0.03),
    "Bar_Stone":     ((0.014, 0.012, 0.012), 0.0, 0.55, None, 0),
    "Glass_Red":     ((0.25, 0.01, 0.02), 0.0, 0.05, (1.00, 0.08, 0.10), 0.08),
    "Glass_Purple":  ((0.10, 0.01, 0.20), 0.0, 0.05, (0.60, 0.15, 1.00), 0.08),
    "Glass_Blue":    ((0.01, 0.05, 0.25), 0.0, 0.05, (0.15, 0.45, 1.00), 0.08),
    "Glass_Smoke":   ((0.03, 0.03, 0.035), 0.1, 0.05, None, 0),
    "Label_Cream":   ((0.55, 0.48, 0.33), 0.0, 0.70, None, 0),
    "Label_Black":   ((0.015, 0.015, 0.015), 0.0, 0.40, None, 0),
    "Label_White":   ((0.75, 0.75, 0.75), 0.0, 0.60, None, 0),
    # small props
    "Vinyl_Black":   ((0.005, 0.005, 0.006), 0.0, 0.25, None, 0),
    "Sleeve_Red":    ((0.45, 0.04, 0.05), 0.0, 0.60, None, 0),
    "Sleeve_Teal":   ((0.02, 0.30, 0.32), 0.0, 0.60, None, 0),
    "Sleeve_Yellow": ((0.60, 0.45, 0.05), 0.0, 0.60, None, 0),
    "Plastic_Grey":  ((0.050, 0.050, 0.055), 0.0, 0.45, None, 0),
    "Screen_Laptop": ((0, 0, 0), 0.0, 0.2, (0.35, 0.55, 1.00), 1.5),
    "Rug_Purple":    ((0.045, 0.008, 0.070), 0.0, 0.95, None, 0),
    "Rug_Teal":      ((0.000, 0.035, 0.040), 0.0, 0.95, None, 0),
    "Rug_Charcoal":  ((0.020, 0.020, 0.022), 0.0, 0.95, None, 0),
    "Marble_White":  ((0.70, 0.70, 0.72), 0.0, 0.15, None, 0),
    "Wax_Cream":     ((0.60, 0.55, 0.45), 0.0, 0.60, None, 0),
    "Flame":         ((0, 0, 0), 0.0, 0.5, (1.00, 0.55, 0.15), 6),
    "Fruit_Lime":    ((0.15, 0.45, 0.04), 0.0, 0.60, None, 0),
    "Window_Street": ((0.02, 0.03, 0.05), 0.3, 0.05, (0.30, 0.45, 1.00), 0.25),
    "Glow_Warm":     ((0, 0, 0), 0.0, 0.5, (1.00, 0.50, 0.18), 0.5),
    "Onyx_Amber":    ((0.05, 0.02, 0.005), 0.0, 0.10, (1.00, 0.42, 0.10), 0.04),
    "Art_Pink":      ((0.02, 0.0, 0.01), 0.0, 0.40, (1.00, 0.15, 0.55), 0.12),
    "Art_Cyan":      ((0.0, 0.01, 0.02), 0.0, 0.40, (0.15, 0.70, 1.00), 0.10),
    "Mirror_Smoke":  ((0.25, 0.22, 0.20), 1.0, 0.02, None, 0),
    "LED_Red":       ((0, 0, 0), 0.0, 0.5, (1.00, 0.04, 0.04), 4),
    "Felt_Green":    ((0.010, 0.090, 0.040), 0.0, 0.92, None, 0),
    "Felt_Blue":     ((0.008, 0.040, 0.120), 0.0, 0.92, None, 0),
    "Felt_Wine":     ((0.100, 0.008, 0.025), 0.0, 0.92, None, 0),
    "Wood_Dark":     ((0.045, 0.022, 0.012), 0.0, 0.35, None, 0),
    "Shade_Green":   ((0.015, 0.060, 0.030), 0.6, 0.30, None, 0),
    "Ball_Yellow":   ((0.80, 0.55, 0.02), 0.0, 0.08, None, 0),
    "Ball_Blue":     ((0.02, 0.06, 0.50), 0.0, 0.08, None, 0),
    "Ball_Red":      ((0.60, 0.02, 0.02), 0.0, 0.08, None, 0),
    "Ball_Purple":   ((0.20, 0.02, 0.35), 0.0, 0.08, None, 0),
    "Ball_Orange":   ((0.80, 0.25, 0.02), 0.0, 0.08, None, 0),
    "Ball_Green":    ((0.02, 0.30, 0.06), 0.0, 0.08, None, 0),
    "Ball_Maroon":   ((0.25, 0.03, 0.02), 0.0, 0.08, None, 0),
    "Ball_Black":    ((0.01, 0.01, 0.01), 0.0, 0.08, None, 0),
    "Ball_White":    ((0.80, 0.78, 0.72), 0.0, 0.08, None, 0),
    "Chalk_Blue":    ((0.05, 0.25, 0.60), 0.0, 0.90, None, 0),
    "Floor_Concrete": ((0.045, 0.045, 0.048), 0.0, 0.75, None, 0),
    "Velvet_Red":    ((0.110, 0.006, 0.020), 0.0, 0.85, None, 0),
    "Velvet_Teal":   ((0.004, 0.050, 0.060), 0.0, 0.85, None, 0),
    "Tile_Black":    ((0.035, 0.035, 0.040), 0.0, 0.12, None, 0),
    "Porcelain":     ((0.75, 0.75, 0.74), 0.0, 0.10, None, 0),
    "Steel":         ((0.30, 0.30, 0.32), 0.9, 0.45, None, 0),
    "Cardboard":     ((0.28, 0.18, 0.09), 0.0, 0.90, None, 0),
    "Wood_Crate":    ((0.22, 0.14, 0.07), 0.0, 0.80, None, 0),
    "Wood_Floor":    ((0.10, 0.05, 0.025), 0.0, 0.45, None, 0),
    "Hazard_Yellow": ((0.70, 0.50, 0.02), 0.0, 0.60, None, 0),
    "Fire_Red":      ((0.50, 0.02, 0.02), 0.2, 0.35, None, 0),
    "Exit_Green":    ((0, 0, 0), 0.0, 0.5, (0.10, 1.00, 0.30), 4),
    "Neon_Orange":   ((0, 0, 0), 0.0, 0.5, (1.00, 0.40, 0.05), 10),
    "Concrete_Wet":  ((0.034, 0.035, 0.037), 0.0, 0.40, None, 0),
    "Floor_Wet":     ((0.018, 0.019, 0.020), 0.0, 0.12, None, 0),
    "Water":         ((0.004, 0.008, 0.010), 0.3, 0.02, None, 0),
    "Stain_Dark":    ((0.012, 0.012, 0.011), 0.0, 0.55, None, 0),
    "Rust":          ((0.16, 0.06, 0.025), 0.6, 0.65, None, 0),
    "Paint_Pink":    ((0.55, 0.04, 0.28), 0.0, 0.70, None, 0),
    "Paint_Cyan":    ((0.04, 0.35, 0.45), 0.0, 0.70, None, 0),
    "Paint_Yellow":  ((0.65, 0.50, 0.04), 0.0, 0.70, None, 0),
    "Paint_White":   ((0.60, 0.60, 0.60), 0.0, 0.70, None, 0),
    "Sodium":        ((0, 0, 0), 0.0, 0.5, (1.00, 0.48, 0.12), 6),
    "Leak_Cool":     ((0, 0, 0), 0.0, 0.5, (0.75, 0.85, 1.00), 5),
    "Mattress":      ((0.32, 0.29, 0.24), 0.0, 0.90, None, 0),
    "Tarp_Blue":     ((0.03, 0.08, 0.25), 0.0, 0.60, None, 0),
    "Trash_Black":   ((0.010, 0.010, 0.012), 0.0, 0.30, None, 0),
    "Void_Black":    ((0.0, 0.0, 0.0), 0.0, 1.00, None, 0),
    "Fire":          ((0, 0, 0), 0.0, 0.5, (1.00, 0.32, 0.04), 3),
    "Fabric_Black":  ((0.012, 0.012, 0.014), 0.0, 0.85, None, 0),
    "Fabric_Camel":  ((0.25, 0.15, 0.07), 0.0, 0.85, None, 0),
    "Fabric_Navy":   ((0.015, 0.02, 0.06), 0.0, 0.85, None, 0),
    "Fabric_Wine":   ((0.12, 0.01, 0.03), 0.0, 0.80, None, 0),
    "Fabric_Grey":   ((0.08, 0.08, 0.085), 0.0, 0.85, None, 0),
}


def _ensure_master():
    if _EAL.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    mat = _AT.create_asset("M_CM_Surface", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def param(cls, name, default, x, y):
        e = _MEL.create_material_expression(mat, cls, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", default)
        return e

    bc = param(unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(0.5, 0.5, 0.5, 1), -600, -300)
    me = param(unreal.MaterialExpressionScalarParameter, "Metallic", 0.0, -600, -100)
    ro = param(unreal.MaterialExpressionScalarParameter, "Roughness", 0.5, -600, 0)
    ec = param(unreal.MaterialExpressionVectorParameter, "EmissiveColor", unreal.LinearColor(0, 0, 0, 1), -800, 200)
    es = param(unreal.MaterialExpressionScalarParameter, "EmissiveStrength", 0.0, -800, 400)
    mul = _MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 300)
    _MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    _MEL.connect_material_property(me, "", unreal.MaterialProperty.MP_METALLIC)
    _MEL.connect_material_property(ro, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _MEL.connect_material_expressions(ec, "", mul, "A")
    _MEL.connect_material_expressions(es, "", mul, "B")
    _MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _MEL.recompile_material(mat)
    _EAL.save_loaded_asset(mat)
    return mat


_MAT_CACHE = {}


def mat(name):
    """Material instance MI_CM_<name>, created on first use from PALETTE."""
    if name in _MAT_CACHE:
        return _MAT_CACHE[name]
    path = "%s/MI_CM_%s" % (MAT_DIR, name)
    if _EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        master = _ensure_master()
        mi = _AT.create_asset("MI_CM_" + name, MAT_DIR, unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        _MEL.set_material_instance_parent(mi, master)
        bc, met, rough, ec, es = PALETTE[name]
        _MEL.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(bc[0], bc[1], bc[2], 1))
        _MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", met)
        _MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        if ec:
            _MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(ec[0], ec[1], ec[2], 1))
            _MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", es)
        _EAL.save_loaded_asset(mi)
    _MAT_CACHE[name] = mi
    return mi


# ---------------------------------------------------------------- primitives
STRUCT, DECOR, CEILING = "struct", "decor", "ceiling"


def _apply_collision(comp, kind):
    if kind == DECOR:
        comp.set_collision_profile_name("NoCollision")
        comp.set_editor_property("cast_shadow", False)
    elif kind == CEILING:
        comp.set_collision_profile_name("Custom")
        comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        comp.set_collision_object_type(unreal.CollisionChannel.ECC_CEILING)  # ECC_GameTraceChannel4
        comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_BLOCK)
    else:
        comp.set_collision_profile_name("BlockAll")


def mesh(label, folder, mesh_path, loc, scale=(1, 1, 1), rot=(0, 0, 0), material=None, kind=STRUCT):
    a = _EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc),
                                    unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(*scale))
    c = a.static_mesh_component
    c.set_static_mesh(unreal.load_asset(mesh_path))
    if material:
        c.set_material(0, mat(material))
    _apply_collision(c, kind)
    return a


def box(label, folder, x0, x1, y0, y1, z0, z1, material, kind=STRUCT):
    if x1 - x0 < 0.5 or y1 - y0 < 0.5 or z1 - z0 < 0.5:
        return None
    return mesh(label, folder, CUBE, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
                ((x1 - x0) / 100, (y1 - y0) / 100, (z1 - z0) / 100), material=material, kind=kind)


def rbox(label, folder, center, size, material, pitch=0, yaw=0, kind=DECOR):
    """Rotated box (size along its local axes)."""
    return mesh(label, folder, CUBE, center, (size[0] / 100, size[1] / 100, size[2] / 100),
                rot=(pitch, yaw, 0), material=material, kind=kind)


def cyl(label, folder, x, y, z0, z1, d, material, kind=STRUCT):
    return mesh(label, folder, CYL, (x, y, (z0 + z1) / 2), (d / 100, d / 100, (z1 - z0) / 100), material=material, kind=kind)


def wall(label, folder, axis, c0, c1, a0, a1, z0, z1, openings, material, kind=STRUCT):
    """Wall along X (axis 'x', band y=c0..c1) or along Y (axis 'y', band x=c0..c1), with openings (a0, a1, z0, z1)."""
    pts = sorted(set([a0, a1] + [min(max(o[0], a0), a1) for o in openings] + [min(max(o[1], a0), a1) for o in openings]))
    cols = []
    for i in range(len(pts) - 1):
        sa, sb = pts[i], pts[i + 1]
        if sb - sa < 0.5:
            continue
        zs = [(z0, z1)]
        for (oa, ob, oz0, oz1) in openings:
            if oa <= sa + 0.01 and ob >= sb - 0.01:
                nz = []
                for (p, q) in zs:
                    if oz1 <= p or oz0 >= q:
                        nz.append((p, q))
                        continue
                    if oz0 > p:
                        nz.append((p, oz0))
                    if oz1 < q:
                        nz.append((oz1, q))
                zs = nz
        cols.append([sa, sb, zs])
    merged = []
    for c in cols:
        if merged and merged[-1][2] == c[2] and abs(merged[-1][1] - c[0]) < 0.01:
            merged[-1][1] = c[1]
        else:
            merged.append(c)
    out = []
    for (sa, sb, zs) in merged:
        for (p, q) in zs:
            out.append(box(label, folder, sa, sb, c0, c1, p, q, material, kind) if axis == "x"
                       else box(label, folder, c0, c1, sa, sb, p, q, material, kind))
    return out


def strip(label, folder, axis, c0, c1, a0, a1, z0, z1, gaps, material):
    """Decor strip along a face, skipping gaps [(a, b)]."""
    cur = a0
    for (g0, g1) in sorted(gaps) + [(a1, a1)]:
        if g0 > cur:
            if axis == "x":
                box(label, folder, cur, g0, c0, c1, z0, z1, material, DECOR)
            else:
                box(label, folder, c0, c1, cur, g0, z0, z1, material, DECOR)
        cur = max(cur, g1)


# ---------------------------------------------------------------- lights
def _color(rgb):
    return unreal.Color(r=int(rgb[0] * 255), g=int(rgb[1] * 255), b=int(rgb[2] * 255), a=255)


def light(kind, label, folder, loc, rgb, intensity, radius, rot=(0, 0, 0), shadows=False, **extra):
    cls = {"point": unreal.PointLight, "spot": unreal.SpotLight, "rect": unreal.RectLight}[kind]
    a = _EAS.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    c = a.get_component_by_class(unreal.LocalLightComponent)
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("light_color", _color(rgb))
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("cast_shadows", shadows)
    for k, v in extra.items():
        c.set_editor_property(k, v)
    return a


NEON_RGB = {"Neon_Magenta": (1.0, 0.1, 0.55), "Neon_Cyan": (0.15, 0.75, 1.0), "Neon_Blue": (0.1, 0.25, 1.0),
            "Neon_Gold": (1.0, 0.6, 0.25), "Neon_White": (1.0, 0.95, 0.9)}


# ---------------------------------------------------------------- room shell
def _arc(a0, a1, segs):
    st = (a1 - a0) / float(segs)
    return [math.radians(a0 + st * (i + 0.5)) for i in range(segs)], abs(st)


def cove_x(folder, yc, zc, R, a0, a1, x0, x1, material, segs=6):
    """Rounded cove along X (wall faces +-Y): arc in the Y-Z plane around (yc, zc). Decor, no collision."""
    ths, st = _arc(a0, a1, segs)
    ch = 2 * R * math.sin(math.radians(st) / 2) + 1.5
    for th in ths:
        rbox("Cove", folder, ((x0 + x1) / 2, yc + R * math.cos(th), zc + R * math.sin(th)), (4, x1 - x0, ch),
             material, pitch=math.degrees(th), yaw=90)


def cove_y(folder, xc, zc, R, a0, a1, y0, y1, material, segs=6):
    """Rounded cove along Y (wall faces +-X): arc in the X-Z plane around (xc, zc)."""
    ths, st = _arc(a0, a1, segs)
    ch = 2 * R * math.sin(math.radians(st) / 2) + 1.5
    for th in ths:
        rbox("Cove", folder, (xc + R * math.cos(th), (y0 + y1) / 2, zc + R * math.sin(th)), (4, y1 - y0, ch),
             material, pitch=math.degrees(th))


def corner(folder, cx, cy, sx, sy, R, z0, z1, material, segs=6):
    """Rounded vertical inside corner at (cx, cy); the room lies toward (sx, sy)."""
    a0 = 180 if sx > 0 else 0
    a1 = 270 if sy > 0 else 90
    if sx < 0 and sy > 0:
        a0, a1 = 270, 360
    ox, oy = cx + sx * R, cy + sy * R
    ths, st = _arc(a0, a1, segs)
    ch = 2 * R * math.sin(math.radians(st) / 2) + 1.5
    for th in ths:
        rbox("CornerCove", folder, (ox + R * math.cos(th), oy + R * math.sin(th), (z0 + z1) / 2), (4, ch, z1 - z0),
             material, yaw=math.degrees(th))


def door_portal(folder, axis, c, a0, a1, color, sides=(-1, 1)):
    """Neon frame around a doorway opening on a wall band centred at c. sides: which faces (-1 / +1) get a frame;
    room_shell passes only the room's own side so neighbouring rooms never stack frames on the same face."""
    t = 8
    if axis == "x":
        for side in sides:
            yy = c + side * (WALL_T / 2 + 2)
            box("DoorNeon", folder, a0 - t, a0, yy - 2, yy + 2, 0, DOOR_H + t, color, DECOR)
            box("DoorNeon", folder, a1, a1 + t, yy - 2, yy + 2, 0, DOOR_H + t, color, DECOR)
            box("DoorNeon", folder, a0 - t, a1 + t, yy - 2, yy + 2, DOOR_H, DOOR_H + t, color, DECOR)
    else:
        for side in sides:
            xx = c + side * (WALL_T / 2 + 2)
            box("DoorNeon", folder, xx - 2, xx + 2, a0 - t, a0, 0, DOOR_H + t, color, DECOR)
            box("DoorNeon", folder, xx - 2, xx + 2, a1, a1 + t, 0, DOOR_H + t, color, DECOR)
            box("DoorNeon", folder, xx - 2, xx + 2, a0 - t, a1 + t, DOOR_H, DOOR_H + t, color, DECOR)


def room_shell(name, x0, y0, tiles_w, tiles_d, doors, wall_mat="Wall_Panel", floor_mat="Floor_Gloss",
               trim="Neon_Cyan", portal="Neon_Magenta", ceiling=True):
    """Build a room on the 4 m grid.

    x0, y0: south-west corner (cm, on the grid). tiles_w along X, tiles_d along Y.
    doors: {"S": [(t0, t1)], "N": [...], "W": [...], "E": [...]} tile index ranges (inclusive) along that edge.
    Returns a dict with interior bounds and door spans (cm).
    """
    f = "Rooms/%s/Shell" % name
    x1, y1 = x0 + tiles_w * TILE, y0 + tiles_d * TILE
    h = WALL_T / 2
    spans = {}
    for edge, rngs in doors.items():
        base = x0 if edge in ("S", "N") else y0
        spans[edge] = [(base + t0 * TILE, base + (t1 + 1) * TILE) for (t0, t1) in rngs]

    def ops(edge):
        return [(a, b, -50, DOOR_H) for (a, b) in spans.get(edge, [])]

    # Every room owns only its side of a shared wall (inner WALL_T/2) and its floor/ceiling stop at the room edge,
    # so two adjacent rooms never put coincident faces in the same place (that z-fights / flickers).
    box("Floor", f, x0, x1, y0, y1, -20, 0, floor_mat)
    wall("Wall_S", f, "x", y0, y0 + h, x0, x1, -20, WALL_H, ops("S"), wall_mat)
    wall("Wall_N", f, "x", y1 - h, y1, x0, x1, -20, WALL_H, ops("N"), wall_mat)
    wall("Wall_W", f, "y", x0, x0 + h, y0 + h, y1 - h, -20, WALL_H, ops("W"), wall_mat)
    wall("Wall_E", f, "y", x1 - h, x1, y0 + h, y1 - h, -20, WALL_H, ops("E"), wall_mat)
    if ceiling:
        box("Ceiling", f, x0, x1, y0, y1, WALL_H, WALL_H + 20, "Ceiling_Dark", CEILING)
    ix0, ix1, iy0, iy1 = x0 + h, x1 - h, y0 + h, y1 - h
    if ceiling:
        R = 60
        cove_x(f, iy0 + R, WALL_H - R, R, 90, 180, ix0, ix1, wall_mat)
        cove_x(f, iy1 - R, WALL_H - R, R, 0, 90, ix0, ix1, wall_mat)
        cove_y(f, ix0 + R, WALL_H - R, R, 90, 180, iy0, iy1, wall_mat)
        cove_y(f, ix1 - R, WALL_H - R, R, 0, 90, iy0, iy1, wall_mat)
        for (cx, cy, sx, sy) in ((ix0, iy0, 1, 1), (ix1, iy0, -1, 1), (ix1, iy1, -1, -1), (ix0, iy1, 1, -1)):
            corner(f, cx, cy, sx, sy, 35, 0, WALL_H - R, wall_mat)
    # LED base trim + mid trim on interior faces (gaps at doors)
    for (z0, z1) in ((0, 6), (300, 306)):
        strip("Trim", f, "x", iy0, iy0 + 3, ix0, ix1, z0, z1, spans.get("S", []), trim)
        strip("Trim", f, "x", iy1 - 3, iy1, ix0, ix1, z0, z1, spans.get("N", []), trim)
        strip("Trim", f, "y", ix0, ix0 + 3, iy0, iy1, z0, z1, spans.get("W", []), trim)
        strip("Trim", f, "y", ix1 - 3, ix1, iy0, iy1, z0, z1, spans.get("E", []), trim)
    # neon portals on every doorway
    for edge, lst in spans.items():
        for (a, b) in lst:
            if edge == "S":
                door_portal(f, "x", y0, a, b, portal, sides=(1,))
            elif edge == "N":
                door_portal(f, "x", y1, a, b, portal, sides=(-1,))
            elif edge == "W":
                door_portal(f, "y", x0, a, b, portal, sides=(1,))
            else:
                door_portal(f, "y", x1, a, b, portal, sides=(-1,))
    return {"x0": ix0, "x1": ix1, "y0": iy0, "y1": iy1, "spans": spans, "folder": "Rooms/%s" % name}


def spawn_marker(name, x, y, floor_z=0):
    """Enemy spawn point 50 cm above the floor it stands on (floor_z for raised platforms)."""
    a = _EAS.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, 50 + floor_z), unreal.Rotator(0, 0, 0))
    a.set_actor_label("EnemySpawn")
    a.set_folder_path("Rooms/%s/Spawns" % name)
    a.set_editor_property("tags", [unreal.Name("EnemySpawn")])
    return a
