"""Prop variety pass for the Main Dance Hall: stocked back bar, bar-top clutter, DJ desk gear, table dressing,
tinted booth variants. Run with include=[cm_kit.py, dress_dance_hall.py, this file], one section per call:
  bar_shelves(tier) for tier 0..2, bar_counter(), dj_gear(), tables(), booth_tints()
Everything here is decor (no collision); clusters of primitives are merged into one static mesh each
(/Game/ClubMap/Props/Kit/) so the level stays light.
"""
import random
import unreal

SPHERE = "/Engine/BasicShapes/Sphere"
CONE = "/Engine/BasicShapes/Cone"
KIT = "/Game/ClubMap/Props/Kit"
FV = "Rooms/DanceHall/Props"
_SMES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


# ---------------------------------------------------------------- tiny primitive helpers (all decor)
def C(f, x, y, z0, h, d, m, sx=None):
    """Upright cylinder, bottom at z0. sx: optional different diameter along X."""
    return mesh("p", f, CYL, (x, y, z0 + h / 2), ((sx or d) / 100, d / 100, h / 100), material=m, kind=DECOR)


def B(f, x, y, z0, w, dp, h, m, yaw=0, pitch=0):
    """Box centred on x,y, bottom at z0 (w along X, dp along Y)."""
    return mesh("p", f, CUBE, (x, y, z0 + h / 2), (w / 100, dp / 100, h / 100), rot=(pitch, yaw, 0), material=m, kind=DECOR)


def S(f, x, y, z, d, m, dz=None):
    return mesh("p", f, SPHERE, (x, y, z), (d / 100, d / 100, (dz or d) / 100), material=m, kind=DECOR)


def K(f, x, y, z0, h, d, m, flip=False):
    """Cone, bottom at z0 (flip: point down)."""
    return mesh("p", f, CONE, (x, y, z0 + h / 2), (d / 100, d / 100, h / 100), rot=(0, 0, 180 if flip else 0), material=m, kind=DECOR)


def merge(actors, name, label, folder=FV):
    """Merge primitives into /Game/ClubMap/Props/Kit/SM_CM_<name>, spawn it, delete the sources."""
    actors = [a for a in actors if a]
    path = "%s/SM_CM_%s" % (KIT, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    o = unreal.MergeStaticMeshActorsOptions()
    o.set_editor_property("base_package_name", path)
    o.set_editor_property("destroy_source_actors", True)
    o.set_editor_property("spawn_merged_actor", True)
    o.set_editor_property("new_actor_label", label)
    a = _SMES.merge_static_mesh_actors(actors, o)
    if a:
        a.set_folder_path(folder)
        c = a.static_mesh_component
        c.set_collision_profile_name("NoCollision")
        c.set_editor_property("cast_shadow", False)
    return a


def clear(label_prefixes, folder=None):
    n = 0
    for a in _EAS.get_all_level_actors():
        lb = a.get_actor_label()
        if any(lb.startswith(p) for p in label_prefixes) and (folder is None or str(a.get_folder_path()) == folder):
            _EAS.destroy_actor(a)
            n += 1
    return n


# ---------------------------------------------------------------- bottles
# kind: (body ('c', d) or ('b', w, dp)), body h, [(neck d, h)...], cap (d, h), glass choices
BOTTLES = {
    "wine":      (("c", 7.5), 22, [(5, 2.5), (2.6, 8)], (2.9, 3), ["Glass_Green", "Glass_Red", "Glass_Smoke"]),
    "vodka":     (("c", 7.0), 25, [(3.5, 2), (2.4, 6)], (2.8, 2.5), ["Glass_Clear", "Glass_Blue", "Glass_Clear"]),
    "square":    (("b", 8, 8), 19, [(3.2, 4)], (3.6, 3), ["Glass_Amber", "Glass_Amber", "Glass_Smoke"]),
    "flask":     (("b", 5, 10), 17, [(2.6, 4)], (3.0, 2.5), ["Glass_Amber", "Glass_Clear"]),
    "tequila":   (("c", 9.5), 13, [(3, 4)], (4.2, 4), ["Glass_Clear", "Glass_Amber", "Glass_Blue"]),
    "squat":     (("c", 10), 10, [(4, 2), (2.4, 8)], (2.8, 2), ["Glass_Purple", "Glass_Red", "Glass_Green"]),
    "champagne": (("c", 8.5), 19, [(5, 4), (3, 9)], (3.3, 4), ["Glass_Green", "Glass_Smoke"]),
}
LABELS = ["Label_Cream", "Label_Black", "Label_White", "Gold"]
CAPS = ["Label_Black", "Gold", "Chrome", "Glass_Red"]


def bottle(f, x, y, z, kind, rng, s=1.0, face=1):
    """One bottle standing at (x, y, z). Label on the face toward +X (face=1) or -X (face=-1)."""
    body, h, necks, cap, glasses = BOTTLES[kind]
    g = rng.choice(glasses)
    out = []
    if body[0] == "c":
        out.append(C(f, x, y, z, h * s, body[1] * s, g))
        half = body[1] * s / 2
        lw = body[1] * s * 0.7
    else:
        out.append(B(f, x, y, z, body[1] * s, body[2] * s, h * s, g))
        half = body[1] * s / 2
        lw = body[2] * s * 0.8
    lh = h * s * rng.uniform(0.3, 0.55)
    out.append(B(f, x + face * (half + 0.2), y, z + h * s * 0.2, 0.6, lw, lh, rng.choice(LABELS)))
    zz = z + h * s
    for (nd, nh) in necks:
        out.append(C(f, x, y, zz, nh * s, nd * s, g))
        zz += nh * s
    out.append(C(f, x, y, zz, cap[1] * s, cap[0] * s, rng.choice(CAPS)))
    width = (body[1] if body[0] == "c" else body[2]) * s
    return out, width


# back bar: shelves at z 110 (back-bar top), 190, 260; x 20..70; y 450..1750; dividers every 260 cm
TIERS = [(110, 52), (190, 47), (260, 47)]
DIVIDERS = [450, 710, 970, 1230, 1490, 1750]


def bar_shelves(tier):
    """Stock one back-bar tier with a seeded mix of bottle shapes and colours, then merge it."""
    f = FV + "/BarStock"
    z, x = TIERS[tier]
    rng = random.Random(100 + tier)
    parts = []
    kinds = list(BOTTLES)
    for i in range(len(DIVIDERS) - 1):
        y = DIVIDERS[i] + 8
        end = DIVIDERS[i + 1] - 8
        # each bay has a "theme" (two dominant kinds) so it reads as curated, not random noise
        theme = rng.sample(kinds, 2)
        while True:
            k = rng.choice(theme) if rng.random() < 0.7 else rng.choice(kinds)
            s = rng.uniform(1.15, 1.35)
            ps, w = bottle(f, x + rng.uniform(-6, 6), 0, z, k, rng, s)
            if y + w > end:
                for a in ps:
                    _EAS.destroy_actor(a)
                break
            for a in ps:
                l = a.get_actor_location()
                a.set_actor_location(unreal.Vector(l.x, y + w / 2, l.z), False, False)
            parts += ps
            y += w + rng.uniform(1.5, 5)
    if tier == 0:
        pass
    a = merge(parts, "BarStock_%d" % tier, "BarStock_Tier%d" % tier)
    return {"tier": tier, "parts": len(parts), "merged": a.get_actor_label() if a else None}


def bar_dressing():
    """Shelf-edge LEDs, bay dividers, remove the old repeated whiskey bottles on the shelves."""
    f = FV + "/BarStock"
    n = 0
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "WhiskeyBottle" and a.get_actor_location().z > 150:
            _EAS.destroy_actor(a)
            n += 1
    clear(["BackBar_Divider", "BackBar_ShelfLED"])
    for y in DIVIDERS:
        box("BackBar_Divider", f, 20, 72, y - 2, y + 2, 110, 330, "Counter_Black", DECOR)
    for z in (187, 257):
        box("BackBar_ShelfLED", f, 68, 70, 452, 1748, z - 1.5, z, "Neon_Gold", DECOR)
    box("BackBar_ShelfLED", f, 88, 90, 452, 1748, 104, 106, "Neon_Gold", DECOR)
    # the backlit panel itself is dimmed through MI_CM_Bottles_Glow (palette)
    mi = mat("Bottles_Glow")
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(0.10, 0.45, 1.0, 1))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.18)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return {"removed_whiskey": n}


# ---------------------------------------------------------------- bar top clutter (counter top z 120, x 240..380)
def _rocks(f, x, y, z):
    return [C(f, x, y, z, 9, 8, "Glass_Clear"), C(f, x, y, z + 1, 4, 7, "Glass_Amber")]


def _highball(f, x, y, z):
    return [C(f, x, y, z, 15, 6.5, "Glass_Clear"), C(f, x, y, z + 1, 9, 5.5, "Glass_Red"), C(f, x + 1.5, y, z + 9, 12, 0.6, "Neon_Magenta")]


def _martini(f, x, y, z):
    return [C(f, x, y, z, 0.6, 7, "Glass_Clear"), C(f, x, y, z, 10, 1, "Glass_Clear"), K(f, x, y, z + 10, 7, 11, "Glass_Blue", flip=True)]


def _wine_glass(f, x, y, z):
    return [C(f, x, y, z, 0.6, 7, "Glass_Clear"), C(f, x, y, z, 9, 1, "Glass_Clear"), S(f, x, y, z + 13, 8, "Glass_Red", 10)]


def _shaker(f, x, y, z):
    return [C(f, x, y, z, 14, 9, "Chrome"), K(f, x, y, z + 14, 6, 8.5, "Chrome"), C(f, x, y, z + 20, 2.5, 3, "Chrome")]


def bar_counter():
    f = FV + "/BarTop"
    clear(["BarTop"])
    rng = random.Random(7)
    z = 120
    parts = []
    # bar mats with glasses on them
    for y in (520, 880, 1330, 1660):
        parts.append(B(f, 330, y, z, 14, 55, 0.8, "Label_Black"))
        g = [_rocks, _highball, _martini, _wine_glass]
        for k in range(3):
            parts += rng.choice(g)(f, 330, y - 18 + k * 18, z + 0.8)
    # shakers, jiggers, garnish tray, napkins, straws behind the counter lip (bartender side)
    for y in (600, 1250, 1580):
        parts += _shaker(f, 265, y, z)
        parts.append(C(f, 265, y + 14, z, 6, 4, "Chrome"))
    for y in (760, 1450):
        parts.append(B(f, 268, y, z, 20, 34, 3, "Counter_Black"))
        for k in range(5):
            parts.append(S(f, 262 + (k % 2) * 10, y - 12 + k * 6, z + 4, 4.5, rng.choice(["Fruit_Lime", "Glass_Amber", "Glass_Red"])))
    for y in (980, 1720):
        parts.append(B(f, 360, y, z, 12, 12, 4, "Label_White"))                 # napkin stack
        parts.append(C(f, 360, y + 16, z, 10, 5, "Glass_Clear"))               # straw holder
        for k in range(6):
            parts.append(C(f, 359 + (k % 3) * 1.2, y + 15 + (k // 3) * 1.5, z + 4, 15, 0.5, rng.choice(["Neon_Magenta", "Neon_Cyan", "Label_Black"])))
    # ice bucket and a bottle being poured from
    parts += [C(f, 300, 1180, z, 18, 20, "Chrome"), C(f, 300, 1180, z + 14, 4, 18, "Glass_Clear")]
    ps, _ = bottle(f, 300, 1205, z, "vodka", rng, 1.2, face=1)
    parts += ps
    # tip jar, tealight candles
    parts += [C(f, 360, 1020, z, 14, 10, "Glass_Clear"), C(f, 360, 1020, z, 4, 9, "Sleeve_Yellow")]
    for y in (700, 1050, 1400):
        parts += [C(f, 345, y, z, 4, 5, "Glass_Red"), C(f, 345, y, z + 2, 2, 3.5, "Wax_Cream"), S(f, 345, y, z + 5, 1.2, "Flame", 2.2)]
    # POS terminal at the north end
    parts += [B(f, 290, 1760, z, 18, 18, 3, "Plastic_Grey"), B(f, 290, 1760, z + 3, 4, 4, 10, "Plastic_Grey"),
              B(f, 293, 1760, z + 18, 1.5, 26, 18, "Screen_Laptop", pitch=-20)]
    m = merge(parts, "BarTop", "BarTop_Clutter")
    for y in (900, 1500):   # warm candle glow on the counter
        light("point", "BarTop_Candle", "Lighting/DanceHall", (345, y, 135), (1, 0.55, 0.2), 4, 200)
    return {"parts": len(parts), "merged": bool(m)}


# ---------------------------------------------------------------- DJ desk (top z 165, X 1300..1900, Y 2760..2860; DJ stands at +Y)
def dj_gear(with_turntables=True):
    f = FV + "/DJGear"
    clear(["DJ_Gear", "DJ_Floor"])
    z = 165
    parts = []
    if with_turntables:
        for x in (1420, 1780):
            parts += [B(f, x, 2810, z, 46, 36, 9, "Speaker_Black"),               # deck body
                      C(f, x - 4, 2810, z + 9, 1.5, 31, "Chrome"),                 # platter
                      C(f, x - 4, 2810, z + 10.5, 0.6, 30, "Vinyl_Black"),         # record
                      C(f, x - 4, 2810, z + 11, 0.4, 10, rng_label(x)),            # record label
                      C(f, x - 4, 2810, z + 11, 1.2, 1, "Chrome"),                 # spindle
                      C(f, x + 17, 2826, z + 9, 4, 4, "Chrome"),                   # tonearm base
                      B(f, x + 8, 2818, z + 12, 1, 22, 1, "Chrome", yaw=-20),      # tonearm
                      B(f, x + 18, 2796, z + 9.2, 2, 9, 0.6, "Plastic_Grey"),      # pitch fader
                      C(f, x - 20, 2794, z + 9, 0.6, 3, "Neon_Cyan")]              # start/stop LED
    # laptop on a stand, behind the controller
    # hinge on the crowd side (-Y), screen faces the DJ (+Y)
    parts += [B(f, 1600, 2848, z, 26, 3, 14, "Chrome"), B(f, 1600, 2846, z + 14, 33, 22, 1.5, "Plastic_Grey"),
              B(f, 1600, 2835, z + 15.5, 33, 1.2, 21, "Plastic_Grey"),
              B(f, 1600, 2835.8, z + 16.5, 31, 0.4, 19, "Screen_Laptop"),
              B(f, 1600, 2848, z + 15.5, 28, 12, 0.2, "Label_Black")]                          # keyboard
    # studio monitors at both ends (cones face the DJ, +Y)
    for x in (1325, 1875):
        parts += [B(f, x, 2810, z, 22, 26, 34, "Speaker_Black"),
                  mesh("p", f, CYL, (x, 2823.5, z + 12), (0.16, 0.16, 0.01), rot=(0, 0, 90), material="Plastic_Grey", kind=DECOR),
                  mesh("p", f, CYL, (x, 2823.5, z + 27), (0.05, 0.05, 0.01), rot=(0, 0, 90), material="Chrome", kind=DECOR)]
    # headphones resting on the desk, a drink, a phone, a USB stick
    hx, hy = 1520, 2775
    parts += [C(f, hx - 9, hy, z, 4, 8, "Speaker_Black"), C(f, hx + 9, hy, z, 4, 8, "Speaker_Black"),
              B(f, hx, hy + 5, z + 3, 22, 3, 2, "Plastic_Grey", yaw=0), B(f, hx - 11, hy + 3, z + 2, 2, 3, 4, "Plastic_Grey"),
              B(f, hx + 11, hy + 3, z + 2, 2, 3, 4, "Plastic_Grey")]
    parts += _highball(f, 1690, 2775, z)
    parts += [B(f, 1720, 2850, z, 8, 15, 0.8, "Label_Black"), B(f, 1720, 2850, z + 0.8, 7, 14, 0.1, "Screen_Laptop")]
    parts.append(B(f, 1580, 2775, z, 2, 5, 0.8, "Neon_Magenta"))
    # cables down the back of the desk to the floor
    for x in (1360, 1450, 1600, 1750, 1840):
        parts.append(B(f, x, 2862, 50, 1.2, 1.2, z - 50, "Label_Black"))
        parts.append(B(f, x + 20, 2880, 45, 40, 1.2, 1.2, "Label_Black", yaw=30))
    m1 = merge(parts, "DJGear", "DJ_Gear")

    # on the platform floor behind the desk: record crates, a flight case, a water bottle
    p2 = []
    rng = random.Random(3)
    for (cx, cy) in ((1150, 3000), (2060, 3040)):
        p2 += [B(f, cx, cy, 45, 36, 36, 3, "Plastic_Grey"), B(f, cx - 17, cy, 45, 2, 36, 32, "Plastic_Grey"),
               B(f, cx + 17, cy, 45, 2, 36, 32, "Plastic_Grey"), B(f, cx, cy - 17, 45, 36, 2, 32, "Plastic_Grey"),
               B(f, cx, cy + 17, 45, 36, 2, 32, "Plastic_Grey")]
        for k in range(10):
            p2.append(B(f, cx, cy - 13 + k * 2.8, 48, 31, 0.6, rng.uniform(29, 31),
                        rng.choice(["Sleeve_Red", "Sleeve_Teal", "Sleeve_Yellow", "Label_White", "Label_Black", "Glass_Purple"]),
                        pitch=0, yaw=rng.uniform(-3, 3)))
    p2 += [B(f, 2000, 2950, 45, 70, 45, 50, "Speaker_Black"), B(f, 2000, 2950, 95, 72, 47, 1.5, "Chrome")]
    for (ex, ey) in ((-35, -22), (35, -22), (-35, 22), (35, 22)):
        p2.append(B(f, 2000 + ex, 2950 + ey, 45, 4, 4, 52, "Chrome"))
    p2.append(B(f, 2000, 2927, 60, 30, 0.5, 18, "Neon_Cyan"))           # stencil-like stripe on the case
    ps, _ = bottle(f, 2010, 2950, 96.5, "vodka", rng, 1.0)
    p2 += ps
    m2 = merge(p2, "DJFloor", "DJ_Floor")
    return {"desk_parts": len(parts), "floor_parts": len(p2), "ok": bool(m1 and m2)}


def rng_label(x):
    return "Sleeve_Red" if x < 1600 else "Sleeve_Teal"


# ---------------------------------------------------------------- tables: different tops + dressing
def pedestal_table(f, x, y, z, top, d=80, h=72, square=False):
    p = [C(f, x, y, z, 2, 46, "Chrome" if top != "Gold" else "Gold"), C(f, x, y, z, h, 8, "Chrome" if top != "Gold" else "Gold")]
    if square:
        p.append(B(f, x, y, z + h, d, d, 3, top))
    else:
        p.append(C(f, x, y, z + h, 3, d, top))
    return p


def candle(f, x, y, z):
    return [C(f, x, y, z, 7, 6, "Glass_Purple"), C(f, x, y, z + 1, 3, 4, "Wax_Cream"), S(f, x, y, z + 5, 1.4, "Flame", 2.6)]


def table_set(f, x, y, z, rng, kind):
    """Things on a table: kind 'cocktails' | 'bottle' | 'champagne' | 'ashtray'."""
    p = candle(f, x, y, z)
    if kind == "cocktails":
        for k in range(3):
            p += rng.choice([_rocks, _highball, _martini, _wine_glass])(f, x + 18 * (k - 1), y + rng.uniform(12, 22), z)
    elif kind == "bottle":
        ps, _ = bottle(f, x + 14, y - 12, z, rng.choice(["square", "tequila", "vodka"]), rng, 1.1, face=-1)
        p += ps + _rocks(f, x - 15, y + 12, z) + _rocks(f, x + 2, y + 18, z)
    elif kind == "champagne":
        for k in range(4):
            p += [C(f, x - 20 + k * 13, y + 15, z, 0.5, 6, "Glass_Clear"), C(f, x - 20 + k * 13, y + 15, z, 9, 0.8, "Glass_Clear"),
                  C(f, x - 20 + k * 13, y + 15, z + 9, 11, 4.5, "Glass_Amber")]
    else:
        p += [C(f, x + 15, y - 10, z, 2.5, 11, "Glass_Smoke"), B(f, x - 15, y - 12, z, 8, 15, 0.8, "Label_Black"),
              B(f, x - 15, y - 12, z + 0.8, 7, 14, 0.1, "Screen_Laptop")] + _highball(f, x, y + 18, z)
    p.append(B(f, x - 28, y - 22, z, 9, 14, 0.3, "Label_White", yaw=rng.uniform(-30, 30)))   # menu / napkin
    return p


def rug(f, x0, x1, y0, y1, z, m, border="Gold"):
    return [box("p", f, x0, x1, y0, y1, z, z + 1.2, m, DECOR),
            box("p", f, x0 + 15, x1 - 15, y0 + 15, y0 + 18, z + 1.2, z + 1.4, border, DECOR),
            box("p", f, x0 + 15, x1 - 15, y1 - 18, y1 - 15, z + 1.2, z + 1.4, border, DECOR),
            box("p", f, x0 + 15, x0 + 18, y0 + 15, y1 - 15, z + 1.2, z + 1.4, border, DECOR),
            box("p", f, x1 - 18, x1 - 15, y0 + 15, y1 - 15, z + 1.2, z + 1.4, border, DECOR)]


def tables():
    """South booth tables get distinct tops + dressing and rugs; VIP tables get dressing."""
    f = FV + "/Tables"
    clear(["Tables_"])
    rng = random.Random(11)
    # hide the plain black drum tables (keep their collision as pawn-blocking clutter)
    hide_placeholders({"Booth_Table", "VIP_Table"})
    parts = []
    # south booths: (x, top material, square?, dressing, rug)
    for (x, top, sq, kind, rg) in ((480, "Marble_White", False, "cocktails", "Rug_Purple"),
                                   (870, "Glass_Smoke", True, "bottle", "Rug_Charcoal"),
                                   (2350, "Gold", False, "ashtray", "Rug_Teal")):
        parts += rug(f, x - 190, x + 190, 30, 430, 0, rg, "Gold" if rg != "Rug_Teal" else "Neon_Cyan")
        parts += pedestal_table(f, x, 300, 1.2, top, square=sq)
        parts += table_set(f, x, 300, 76.2, rng, kind)
    # VIP: the remaining booth table at y 325 gets champagne flutes + candle on a gold-rimmed top
    parts += [C(f, 2860, 325, 100, 1.5, 74, "Gold")]
    parts += table_set(f, 2860, 325, 101.5, rng, "champagne")
    m = merge(parts, "Tables", "Tables_Dressing")
    for (x, y, z) in ((480, 300, 95), (870, 300, 95), (2350, 300, 95), (2860, 325, 125)):   # candle glow per table
        light("point", "Tables_Candle", "Lighting/DanceHall", (x, y, z), (1, 0.5, 0.18), 12, 320)
    return {"parts": len(parts), "ok": bool(m)}


# ---------------------------------------------------------------- tintable PBR master + booth colour variants
TINT_MASTER = "/Game/ClubMap/Materials/M_CM_GenPBR_Tint"


def ensure_tint_master():
    EAL = unreal.EditorAssetLibrary
    if EAL.does_asset_exist(TINT_MASTER):
        return unreal.load_asset(TINT_MASTER)
    MEL = unreal.MaterialEditingLibrary
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_CM_GenPBR_Tint", "/Game/ClubMap/Materials", unreal.Material,
                                                                unreal.MaterialFactoryNew())
    bc = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -300)
    bc.set_editor_property("parameter_name", "BaseColorMap")
    bc.set_editor_property("texture", unreal.load_asset("/Game/ClubMap/Props/VIPBooth/T_Gen_VIPBooth_base_color"))
    de = MEL.create_material_expression(m, unreal.MaterialExpressionDesaturation, -600, -300)
    one = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -800, -150)
    one.set_editor_property("r", 1.0)
    MEL.connect_material_expressions(bc, "RGB", de, "")
    MEL.connect_material_expressions(one, "", de, "Fraction")
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, -100)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, -250)
    MEL.connect_material_expressions(de, "", mul, "A")
    MEL.connect_material_expressions(tint, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    specs = [("NormalMap", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialProperty.MP_NORMAL, "RGB", 0,
              "/Game/ClubMap/Props/VIPBooth/T_Gen_VIPBooth_normal"),
             ("RoughnessMap", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_ROUGHNESS, "R", 300,
              "/Game/ClubMap/Props/VIPBooth/T_Gen_VIPBooth_roughness"),
             ("MetallicMap", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_METALLIC, "R", 600,
              "/Game/ClubMap/Props/VIPBooth/T_Gen_VIPBooth_metallic")]
    for (name, st, prop_, out, y, tex) in specs:
        e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
        e.set_editor_property("parameter_name", name)
        if EAL.does_asset_exist(tex):
            e.set_editor_property("texture", unreal.load_asset(tex))
        e.set_editor_property("sampler_type", st)
        MEL.connect_material_property(e, out, prop_)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def tinted_mi(src_name, variant, rgb):
    """MI_<src>_<variant>: the prop's own textures on the tint master, recoloured."""
    EAL = unreal.EditorAssetLibrary
    MEL = unreal.MaterialEditingLibrary
    d = "/Game/ClubMap/Props/%s" % src_name
    path = "%s/MI_%s_%s" % (d, src_name, variant)
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_%s_%s" % (src_name, variant), d,
                                                                     unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, ensure_tint_master())
    for kind, param in (("base_color", "BaseColorMap"), ("normal", "NormalMap"), ("roughness", "RoughnessMap"), ("metallic", "MetallicMap")):
        t = "%s/T_Gen_%s_%s" % (d, src_name, kind)
        if EAL.does_asset_exist(t):
            MEL.set_material_instance_texture_parameter_value(mi, param, unreal.load_asset(t))
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    EAL.save_loaded_asset(mi)
    return mi


def booth_tints():
    """South booths: wine red / deep purple / teal. VIP booth: champagne-gold leather."""
    purple = tinted_mi("VIPBooth", "Purple", (0.9, 0.25, 1.6))
    teal = tinted_mi("VIPBooth", "Teal", (0.15, 1.1, 1.1))
    gold = tinted_mi("VIPBooth", "Gold", (1.8, 1.1, 0.45))
    done = []
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() != "VIPBooth":
            continue
        l = a.get_actor_location()
        c = a.static_mesh_component
        pick = None
        if abs(l.y - 128) < 5 and abs(l.x - 870) < 5:
            pick = purple
        elif abs(l.y - 128) < 5 and abs(l.x - 2350) < 5:
            pick = teal
        elif l.x > 3000 and abs(l.y - 325) < 5:
            pick = gold
        if pick:
            for i in range(c.get_num_materials()):
                c.set_material(i, pick)
            done.append((round(l.x), round(l.y), pick.get_name()))
    return done


# ---------------------------------------------------------------- VIP lounge (replaces the second VIP booth) + generated props
def vip_lounge(chair_yaw_offset=0.0):
    """Second VIP bay (y 620..1040, platform z 45): rug, coffee table, two armchairs, hookah. Chairs/table block pawns only."""
    f = FV + "/VIPLounge"
    clear(["VIPLounge"])
    # remove the second booth, its bucket and the placeholder collision/table that belonged to it
    gone = 0
    for a in _EAS.get_all_level_actors():
        lb, l = a.get_actor_label(), a.get_actor_location()
        if (lb in ("VIPBooth", "ChampagneBucket", "VIP_Table") and abs(l.y - 795) < 10) or \
           (lb in ("VIP_Booth_Seat", "VIP_Booth_Back") and abs(l.y - 795) < 10):
            _EAS.destroy_actor(a)
            gone += 1
    z = 45
    parts = rug(f, 2680, 3150, 640, 1040, z, "Rug_Purple", "Gold")
    cx, cy = 2900, 840
    t = prop("CoffeeTable", (cx, cy, z + 38 * 0.5), s=0.5, coll="pawn", label="VIPLounge_Table")
    t.set_folder_path(f)
    for (x, y, yaw) in ((cx, cy - 130, 90), (cx, cy + 130, -90), (cx + 150, cy, 180)):
        c = prop("LoungeArmchair", (x, y, z + 95 * 0.45), yaw=yaw + chair_yaw_offset, s=0.45, coll="pawn", label="VIPLounge_Chair")
        c.set_folder_path(f)
    h = prop("Hookah", (cx - 120, cy + 70, z + 95 * 0.45), s=0.45, label="VIPLounge_Hookah")
    h.set_folder_path(f)
    tz = z + 76 * 0.5
    rng = random.Random(21)
    parts += candle(f, cx - 10, cy - 5, tz)
    ps, _ = bottle(f, cx + 15, cy + 10, tz, "champagne", rng, 1.1, face=-1)
    parts += ps + _wine_glass(f, cx - 25, cy + 20, tz) + _martini(f, cx + 25, cy - 22, tz)
    merge(parts, "VIPLounge", "VIPLounge_Dressing", f)
    light("point", "VIPLounge_Candle", "Lighting/DanceHall", (cx, cy, tz + 25), (1, 0.5, 0.18), 12, 320)
    light("point", "VIPLounge_Purple", "Lighting/DanceHall", (3100, 840, 300), (0.6, 0.15, 1.0), 25, 600)
    return {"removed": gone}


def generated_extras():
    """Liquor-bottle clusters on the bar well + flight case."""
    out = []
    for (loc, yaw, lb) in (((262, 1000, 120 + 57 * 0.27), 90, "BarWell_Bottles"), ((262, 1380, 120 + 57 * 0.27), 90, "BarWell_Bottles")):
        out.append(prop("LiquorBottles", loc, yaw=yaw, s=0.27, label=lb).get_actor_label())
    return out
