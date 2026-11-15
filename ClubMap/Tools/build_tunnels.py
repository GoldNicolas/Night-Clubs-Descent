"""Storm-drain tunnels outside the club (Las Vegas drains theme) + the club threshold.
Run with include=[cm_kit.py, dress_dance_hall.py, dress_variety.py, build_lighting_fx.py, this file], then build_tunnels().

The club is one "location"; the tunnels are the connective tissue the procedural generator plugs other locations into.
Every tunnel end is a LOCATION SOCKET: a steel bar gate (bars with gaps: AI can see/shoot through, pawns can't pass)
with the tunnel visibly carrying on into fog beyond it, and a TargetPoint tagged ("LocationSocket", <dir>) facing outward.
The generator removes the gate (actors labelled Gate_<dir>*) and attaches the next location's tunnel at the socket.

Grid (cm), all floors at z 0, tunnel ceilings 4 m:
  ClubThreshold 2x2  x 1200..2000,  y -2000..-1200   N = Entry front opening (8 m), S tile 1 -> T1
  T1 straight   1x3  x 1600..2000,  y -3200..-2000
  Junction      3x3  x 1200..2400,  y -4400..-3200   N/W/E/S doors (tile 1)
  TW straight   5x1  x -800..1200,  y -4000..-3600   -> Camp
  Camp          3x3  x -2000..-800, y -4400..-3200   W door -> XW
  XW exit       2x1  x -2800..-2000,y -4000..-3600   gate at x -2400   (socket West)
  TE straight   7x1  x 2400..5200,  y -4000..-3600   gate at x 4800    (socket East)
  TS straight   1x4  x 1600..2000,  y -6000..-4400   gate at y -5600   (socket South)
"""
import math
import random
import unreal

H = 400                       # tunnel height
FT = "Rooms/Tunnels/"
LT = "Lighting/Tunnels"
MODULES = ("ClubThreshold", "T1", "Junction", "TW", "Camp", "XW", "TE", "TS")


def _clear():
    n = 0
    for a in _EAS.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith(FT) or fp.startswith(LT) or any(fp.startswith("Rooms/%s/" % m) or fp.startswith("Lighting/%s" % m) for m in MODULES):
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
    a = prop(name, loc, yaw=yaw, s=s, sxy=sxy, sz=sz, coll=coll, label=label or ("Tunnel_" + name))
    a.set_folder_path(folder)
    return a


# ------------------------------------------------------------------ shell
def tunnel_shell(name, x0, y0, tw, td, doors):
    """Concrete culvert: same non-overlapping rules as room_shell, 4 m high, rounded coves on door-less walls."""
    f = "Rooms/%s/Shell" % name
    x1, y1 = x0 + tw * TILE, y0 + td * TILE
    h = WALL_T / 2
    spans = {}
    for edge, rngs in doors.items():
        base = x0 if edge in ("S", "N") else y0
        spans[edge] = [(base + t0 * TILE, base + (t1 + 1) * TILE) for (t0, t1) in rngs]

    def ops(edge):
        return [(a, b, -50, H + 50) for (a, b) in spans.get(edge, [])]

    box("Floor", f, x0, x1, y0, y1, -20, 0, "Floor_Wet")
    wall("Wall_S", f, "x", y0, y0 + h, x0, x1, -20, H, ops("S"), "Concrete_Wet")
    wall("Wall_N", f, "x", y1 - h, y1, x0, x1, -20, H, ops("N"), "Concrete_Wet")
    wall("Wall_W", f, "y", x0, x0 + h, y0 + h, y1 - h, -20, H, ops("W"), "Concrete_Wet")
    wall("Wall_E", f, "y", x1 - h, x1, y0 + h, y1 - h, -20, H, ops("E"), "Concrete_Wet")
    box("Ceiling", f, x0, x1, y0, y1, H, H + 20, "Concrete_Wet", CEILING)
    ix0, ix1, iy0, iy1 = x0 + h, x1 - h, y0 + h, y1 - h
    R = 90
    if "S" not in spans:
        cove_x(f, iy0 + R, H - R, R, 90, 180, ix0, ix1, "Concrete_Wet")
    if "N" not in spans:
        cove_x(f, iy1 - R, H - R, R, 0, 90, ix0, ix1, "Concrete_Wet")
    if "W" not in spans:
        cove_y(f, ix0 + R, H - R, R, 90, 180, iy0, iy1, "Concrete_Wet")
    if "E" not in spans:
        cove_y(f, ix1 - R, H - R, R, 0, 90, iy0, iy1, "Concrete_Wet")
    return {"x0": ix0, "x1": ix1, "y0": iy0, "y1": iy1}


# ------------------------------------------------------------------ dressing
def dress_tunnel(name, x0, x1, y0, y1, axis, rng, lamps=True, lamp_every=800, skip_lamp_beyond=None, hw=180, walls=True):
    """Drain channel + grates, water stains, graffiti, pipes, puddles, trash, caged sodium lamps. axis = run direction.
    hw = interior half-width across the run (180 for a 1-tile tunnel); walls=False skips wall dressing (rooms with doors all round)."""
    f = "Rooms/%s/Dress" % name
    p = []
    L = x1 - x0 if axis == "x" else y1 - y0
    mid = (y0 + y1) / 2 if axis == "x" else (x0 + x1) / 2
    a0 = x0 if axis == "x" else y0

    def at(s, t, z, w, d, hgt, m, yaw=0, pitch=0):           # s along the run, t across
        if axis == "x":
            return B(f, a0 + s, mid + t, z, w, d, hgt, m, yaw=yaw, pitch=pitch)
        return B(f, mid + t, a0 + s, z, d, w, hgt, m, yaw=yaw, pitch=pitch)

    p.append(at(L / 2, 0, 0, L, 70, 0.5, "Water"))                                       # drain channel
    for s in range(200, int(L), 400):
        for k in range(5):
            p.append(at(s - 20 + k * 10, 0, 0.5, 3, 76, 1.2, "Steel"))                  # grates
    for side in ((-1, 1) if walls else ()):                                              # stains down the walls
        wt = side * (hw - 2)
        for k in range(int(L / 90)):
            s = rng.uniform(20, L - 20)
            hgt = rng.uniform(80, 330)
            p.append(at(s, wt - side * 0.6, H - hgt, rng.uniform(15, 70), 0.8, hgt, "Stain_Dark"))
        for k in range(int(L / 300)):                                                  # graffiti clusters
            s0 = rng.uniform(80, L - 80)
            col = rng.choice(["Paint_Pink", "Paint_Cyan", "Paint_Yellow", "Paint_White"])
            col2 = rng.choice(["Paint_Pink", "Paint_Cyan", "Paint_Yellow", "Paint_White"])
            z0 = rng.uniform(90, 160)
            for j in range(rng.randint(6, 12)):
                ss = s0 + rng.uniform(-90, 90)
                p.append(mesh("p", f, CUBE,
                              ((a0 + ss, mid + wt - side * 1.2, z0 + rng.uniform(0, 90)) if axis == "x"
                               else (mid + wt - side * 1.2, a0 + ss, z0 + rng.uniform(0, 90))),
                              ((rng.uniform(0.2, 0.7), 0.01, rng.uniform(0.04, 0.1)) if axis == "x"
                               else (0.01, rng.uniform(0.2, 0.7), rng.uniform(0.04, 0.1))),
                              rot=((rng.uniform(-50, 50), 0, 0) if axis == "x" else (0, 0, rng.uniform(-50, 50))),
                              material=col if j % 3 else col2, kind=DECOR))
    # pipes + brackets on one wall
    side = rng.choice((-1, 1))
    pipe_t = hw - 20
    for (zz, d, m) in (((330, 18, "Rust"), (300, 10, "Steel")) if walls else ()):
        if axis == "x":
            p.append(mesh("p", f, CYL, ((x0 + x1) / 2, mid + side * pipe_t, zz), (d / 100, d / 100, L / 100), rot=(90, 0, 0), material=m, kind=DECOR))
        else:
            p.append(mesh("p", f, CYL, (mid + side * pipe_t, (y0 + y1) / 2, zz), (d / 100, d / 100, L / 100), rot=(0, 0, 90), material=m, kind=DECOR))
    for s in (range(150, int(L), 300) if walls else ()):
        p.append(at(s, side * (hw - 8), 285, 4, 16, 60, "Rust"))
    for k in range(int(L / 250)):                                                      # puddles
        s = rng.uniform(40, L - 40)
        t = rng.uniform(-(hw - 50), hw - 50)
        if abs(t) < 50:
            continue
        loc = (a0 + s, mid + t, 0.3) if axis == "x" else (mid + t, a0 + s, 0.3)
        p.append(mesh("p", f, CYL, loc, (rng.uniform(0.6, 1.6), rng.uniform(0.4, 1.0), 0.004), rot=(0, rng.uniform(0, 180), 0),
                      material="Water", kind=DECOR))
    for k in range(int(L / 350)):                                                      # trash bags / cans
        s = rng.uniform(40, L - 40)
        t = rng.choice((-1, 1)) * rng.uniform(hw - 40, hw - 15)
        loc = (a0 + s, mid + t) if axis == "x" else (mid + t, a0 + s)
        if rng.random() < 0.6:
            p.append(S(f, loc[0], loc[1], 18, 40, "Trash_Black", 34))
        else:
            p.append(C(f, loc[0], loc[1], 0, 12, 7, rng.choice(["Fire_Red", "Steel", "Glass_Green"])))
    lamp_pts = []
    if lamps:
        for i, s in enumerate(range(lamp_every // 2, int(L), lamp_every)):
            if skip_lamp_beyond is not None and s > skip_lamp_beyond:
                continue
            sd = 1 if i % 2 else -1
            broken = rng.random() < 0.25
            t = sd * (hw - 6)
            if axis == "x":
                lx, ly = a0 + s, mid + t
            else:
                lx, ly = mid + t, a0 + s
            p.append(B(f, lx, ly, 300, 18 if axis == "x" else 8, 8 if axis == "x" else 18, 26, "Steel"))
            p.append(C(f, lx - (0 if axis == "x" else sd * 6), ly - (sd * 6 if axis == "x" else 0), 304, 18, 10,
                       "Counter_Black" if broken else "Sodium"))
            for k in (-1, 1):
                p.append(B(f, lx + (k * 6 if axis == "x" else -sd * 8), ly + (-sd * 8 if axis == "x" else k * 6), 302, 1, 1, 22, "Steel"))
            if not broken:
                lamp_pts.append((lx - (0 if axis == "x" else sd * 30), ly - (sd * 30 if axis == "x" else 0)))
    merge(p, "Tunnel_%sDress" % name, "Tunnel_%sDress" % name, f)
    for (lx, ly) in lamp_pts:
        light("point", "Tunnel_Lamp", "Lighting/%s" % name, (lx, ly, 300), (1, 0.48, 0.14), 22, 650)
    drain_fog(name, x0 + 10, y0 + 10, x1 - 10, y1 - 10)


def drain_fog(name, x0, y0, x1, y1, layers=((8, 0.55), (30, 0.3), (60, 0.15))):
    """Low drain mist: same fog sheets as the club but grey-sodium and barely self-lit (the club's fog glows purple)."""
    fog_cards(name, x0, y0, x1, y1, layers=layers)
    for (z, _) in layers:
        path = "%s/MI_CM_FloorFog_%s_L%d" % (MAT_DIR, name, z)
        if _EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
            MEL.set_material_instance_vector_parameter_value(mi, "FogColor", unreal.LinearColor(0.45, 0.42, 0.38, 1))
            MEL.set_material_instance_vector_parameter_value(mi, "GlowColor", unreal.LinearColor(0.5, 0.35, 0.2, 1))
            MEL.set_material_instance_scalar_parameter_value(mi, "Glow", 0.04)
            _EAL.save_loaded_asset(mi)


def gate(name, axis, c, a0, a1, direction):
    """Location socket: steel bar gate across the tunnel at c (x = c for axis 'y' gates across an X-run, etc.).
    axis 'x': gate spans X from a0..a1 at y = c.  axis 'y': gate spans Y from a0..a1 at x = c."""
    f = "Rooms/%s/Gate" % name
    lbl = "Gate_%s" % direction
    for k in range(int((a1 - a0) / 20) + 1):
        t = a0 + k * 20
        if axis == "x":
            box(lbl + "_Bar", f, t - 2, t + 2, c - 2, c + 2, 0, H, "Rust")
        else:
            box(lbl + "_Bar", f, c - 2, c + 2, t - 2, t + 2, 0, H, "Rust")
    for z in (30, 200, 370):
        if axis == "x":
            box(lbl + "_Rail", f, a0, a1, c - 4, c + 4, z, z + 8, "Rust")
        else:
            box(lbl + "_Rail", f, c - 4, c + 4, a0, a1, z, z + 8, "Rust")
    mid = (a0 + a1) / 2
    lock = [B(f, mid if axis == "x" else c - 6, c - 6 if axis == "x" else mid, 190, 10, 10, 14, "Steel"),
            C(f, mid if axis == "x" else c - 9, c - 9 if axis == "x" else mid, 176, 10, 6, "Rust")]
    # hazard plate on the bars + a red warning lamp on the ceiling just in front (near side), so a gate never sits in the dark
    near = {"West": 1, "East": -1, "South": 1, "North": -1}[direction]          # which side the player approaches from
    if axis == "x":
        lock += [B(f, mid, c + near * 6, 120, 90, 1, 45, "Hazard_Yellow")]
        for k in range(4):
            lock.append(mesh("p", f, CUBE, (mid - 33 + k * 22, c + near * 6.6, 142), (0.08, 0.006, 0.5), rot=(45, 0, 0), material="Trash_Black", kind=DECOR))
        lx, ly = mid, c + near * 60
    else:
        lock += [B(f, c + near * 6, mid, 120, 1, 90, 45, "Hazard_Yellow")]
        for k in range(4):
            lock.append(mesh("p", f, CUBE, (c + near * 6.6, mid - 33 + k * 22, 142), (0.006, 0.08, 0.5), rot=(0, 0, 45), material="Trash_Black", kind=DECOR))
        lx, ly = c + near * 60, mid
    lock += [B(f, lx, ly, H - 14, 16, 16, 12, "Steel"), C(f, lx, ly, H - 22, 8, 12, "LED_Red")]
    merge(lock, lbl + "_Lock", lbl + "_Lock", f)
    light("point", "Tunnel_GateWarning", "Lighting/%s" % name, (lx, ly, H - 40), (1, 0.08, 0.05), 18, 550)
    # the socket marker the generator reads (facing outward, through the gate)
    yaw = {"West": 180, "East": 0, "South": -90, "North": 90}[direction]
    loc = unreal.Vector(c if axis == "y" else mid, mid if axis == "y" else c, 100)
    m = _EAS.spawn_actor_from_class(unreal.TargetPoint, loc, unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    m.set_actor_label("LocationSocket_%s" % direction)
    m.set_folder_path("Rooms/%s/Socket" % name)
    m.set_editor_property("tags", [unreal.Name("LocationSocket"), unreal.Name(direction)])


def beyond(name, x0, x1, y0, y1, end_axis_dir):
    """The stretch past a gate: unlit, a far lamp, a black end wall veiled by vertical fog sheets so it reads as going on."""
    f = "Rooms/%s/Beyond" % name
    fogmi = fog_mi("%s_Veil" % name, 0.85, color=(0.35, 0.34, 0.32))
    MEL.set_material_instance_vector_parameter_value(fogmi, "GlowColor", unreal.LinearColor(0.4, 0.3, 0.2, 1))
    MEL.set_material_instance_scalar_parameter_value(fogmi, "Glow", 0.03)
    _EAL.save_loaded_asset(fogmi)
    (ax, sign) = end_axis_dir                                 # e.g. ("x", -1) = the tunnel continues toward -X
    for k, frac in enumerate((0.45, 0.7, 0.9)):
        if ax == "x":
            xx = (x0 if sign < 0 else x1) - sign * (1 - frac) * (x1 - x0) * 0.9
            a = mesh("Tunnel_Veil", f, "/Engine/BasicShapes/Plane", (xx, (y0 + y1) / 2, H / 2), (H / 100, (y1 - y0) / 100, 1),
                     rot=(90, 0, 0), kind=DECOR)
        else:
            yy = (y0 if sign < 0 else y1) - sign * (1 - frac) * (y1 - y0) * 0.9
            a = mesh("Tunnel_Veil", f, "/Engine/BasicShapes/Plane", ((x0 + x1) / 2, yy, H / 2), ((x1 - x0) / 100, H / 100, 1),
                     rot=(0, 0, 90), kind=DECOR)
        a.static_mesh_component.set_material(0, fogmi)
        a.static_mesh_component.set_editor_property("cast_shadow", False)
    # black end wall just inside the module end
    if ax == "x":
        ex = x0 + 21 if sign < 0 else x1 - 21
        box("Tunnel_VoidEnd", f, ex - 1, ex + 1, y0, y1, 0, H, "Void_Black", DECOR)
        lx, ly = (x0 + 60 if sign < 0 else x1 - 60), y0 + 30
    else:
        ey = y0 + 21 if sign < 0 else y1 - 21
        box("Tunnel_VoidEnd", f, x0, x1, ey - 1, ey + 1, 0, H, "Void_Black", DECOR)
        lx, ly = x0 + 30, (y0 + 60 if sign < 0 else y1 - 60)
    p = [B(f, lx, ly, 300, 14, 14, 20, "Steel"), C(f, lx, ly, 296, 6, 12, "Sodium")]
    merge(p, "Tunnel_FarLamp_%s" % name, "Tunnel_FarLamp_%s" % name, f)
    light("point", "Tunnel_FarLamp", "Lighting/%s" % name, (lx, ly, 290), (1, 0.45, 0.12), 6, 350)


def stencil_arrow(f, x, y, z, axis, sign, m="Paint_White"):
    """Spray-painted arrow on a wall pointing along the tunnel (decor)."""
    p = []
    if axis == "x":                                        # wall faces +-Y, arrow points toward sign*X
        p.append(B(f, x, y, z, 60, 0.6, 8, m))
        for r in (35, -35):
            p.append(mesh("p", f, CUBE, (x + sign * 25, y, z + 4 + (8 if r > 0 else -8) * 0), (0.25, 0.006, 0.07),
                          rot=(r * sign, 0, 0), material=m, kind=DECOR))
    else:
        p.append(B(f, x, y, z, 0.6, 60, 8, m))
        for r in (35, -35):
            p.append(mesh("p", f, CUBE, (x, y + sign * 25, z + 4), (0.006, 0.25, 0.07), rot=(0, 0, r * sign), material=m, kind=DECOR))
    return p


# ------------------------------------------------------------------ modules
def club_threshold():
    name = "ClubThreshold"
    room_shell(name, 1200, -2000, 2, 2, doors={"N": [(0, 1)], "S": [(1, 1)]}, wall_mat="Concrete_Wet", floor_mat="Floor_Wet",
               trim="Speaker_Black", portal="Neon_Magenta")                 # gritty concrete; the club's light spills out
    f = "Rooms/%s/Props" % name
    # big neon club sign over the entrance, on the Entry's south wall above the opening (faces -Y)
    sy, cx, cz = -1222, 1600, 500          # in front of this room's wall section above the opening (y -1220)
    box("Threshold_SignPlate", f, cx - 260, cx + 260, sy - 2, sy, 410, 590, "Counter_Black", DECOR)
    r = 70
    for (dx, dz, roll) in ((-r / 2, r / 2, 45), (r / 2, r / 2, -45), (-r / 2, -r / 2, -45), (r / 2, -r / 2, 45)):
        mesh("Threshold_Logo", f, CUBE, (cx + dx, sy - 3, cz + dz), (r * 1.414 / 100, 0.03, 0.05), rot=(45 if dx * dz < 0 else -45, 0, 0),
             material="Neon_Magenta", kind=DECOR)
    for (dx, dz, roll) in ((-r / 4, r / 4, 45), (r / 4, r / 4, -45), (-r / 4, -r / 4, -45), (r / 4, -r / 4, 45)):
        mesh("Threshold_Logo", f, CUBE, (cx + dx, sy - 4, cz + dz), (r * 0.707 / 100, 0.03, 0.04), rot=(45 if dx * dz < 0 else -45, 0, 0),
             material="Neon_Cyan", kind=DECOR)
    for zz in (424, 576):
        box("Threshold_SignLine", f, cx - 240, cx + 240, sy - 4, sy - 2, zz, zz + 4, "Neon_Gold", DECOR)
    # queue: crowd barriers along the west side, bouncer post by the door, posters on the walls
    for k in range(4):
        _prop("CrowdBarrier", (1330, -1850 + k * 130, 95 * 0.55), f, yaw=90, s=0.55, coll="pawn", label="Threshold_Barrier")
    _prop("BouncerPodium", (1860, -1330, 95 * 0.6), f, yaw=180, s=0.6, coll="pawn", label="Threshold_BouncerPost")
    rng = random.Random(5)
    p = []
    for (x, y, ax) in ((1222, -1700, "x"), (1222, -1450, "x"), (1978, -1650, "y")):
        col = rng.choice(["Art_Pink", "Art_Cyan", "Onyx_Amber"])
        if ax == "x":
            p += [B(f, x + 1, y, 140, 2, 80, 115, "Counter_Black"), B(f, x + 2.5, y, 145, 0.5, 70, 100, col)]
        else:
            p += [B(f, x - 1, y, 140, 2, 80, 115, "Counter_Black"), B(f, x - 2.5, y, 145, 0.5, 70, 100, col)]
    p += [B(f, 1600, -1240, 0.2, 760, 60, 1, "Fabric_Black")]                       # entrance mat
    for k in range(10):                                                             # cigarette butts / flyers
        p.append(B(f, rng.uniform(1300, 1900), rng.uniform(-1900, -1300), 0.3, 6, 4, 0.2, rng.choice(["Label_White", "Paint_Pink"]),
                   yaw=rng.uniform(0, 180)))
    merge(p, "Threshold_Bits", "Threshold_Bits", f)
    for (x, y) in ((1450, -1900), (1750, -1500)):
        p2 = [mesh("p", f, CYL, (x, y, 0.3), (1.2, 0.8, 0.004), rot=(0, 30, 0), material="Water", kind=DECOR)]
        merge(p2, "Threshold_Puddle", "Threshold_Puddle", f)
    L = "Lighting/%s" % name
    light("rect", "Threshold_SignLight", L, (1600, -1250, 500), (1, 0.1, 0.55), 30, 900, rot=(0, -90, 0), source_width=500.0, source_height=180.0)
    light("point", "Threshold_Sodium", L, (1600, -1800, 520), (1, 0.48, 0.14), 25, 700)
    drain_fog(name, 1220, -1980, 1980, -1220, layers=((8, 0.5), (30, 0.25)))
    spawn_marker(name, 1500, -1750)


def junction():
    name = "Junction"
    tunnel_shell(name, 1200, -4400, 3, 3, {"N": [(1, 1)], "W": [(1, 1)], "E": [(1, 1)], "S": [(1, 1)]})
    f = "Rooms/%s/Props" % name
    p = []
    # big central floor grate + standing water around it
    p.append(mesh("p", f, CYL, (1800, -3800, 0.3), (3.2, 3.2, 0.004), material="Water", kind=DECOR))
    for k in range(9):
        p.append(B(f, 1800, -3880 + k * 20, 0.6, 160, 4, 1.5, "Rust"))
    p.append(B(f, 1800, -3800, 0.4, 170, 170, 0.5, "Counter_Black"))
    # ladder up the NE corner wall to a sealed manhole in the ceiling (light leaking round the cover)
    lx, ly = 2300, -3300
    for k in range(13):
        p.append(B(f, lx, ly, 30 + k * 28, 50, 4, 3, "Rust"))
    for dx in (-25, 25):
        p.append(B(f, lx + dx, ly, 0, 4, 6, H, "Rust"))
    p.append(mesh("p", f, CYL, (lx, ly - 40, H - 1), (0.9, 0.9, 0.01), material="Leak_Cool", kind=DECOR))
    p.append(mesh("p", f, CYL, (lx, ly - 40, H - 1.6), (0.84, 0.84, 0.01), material="Rust", kind=DECOR))
    # concrete junction pillars at the four inner corners + stencilled direction arrows
    for (x, y) in ((1430, -3430), (2170, -3430), (1430, -4170), (2170, -4170)):
        p.append(B(f, x, y, 0, 60, 60, H, "Concrete_Wet"))
    rng = random.Random(9)
    p += stencil_arrow(f, 1222, -3450, 160, "y", 1)                 # west wall, pointing north (back to the club)
    p += [B(f, 1222, -3450, 180, 0.6, 40, 14, "Neon_Magenta")]      # a bit of club neon tape someone stuck up
    merge(p, "Tunnel_JunctionBits", "Tunnel_JunctionBits", f)
    for (x, y) in ((1430, -3430), (2170, -3430), (1430, -4170), (2170, -4170)):
        box("Tunnel_PillarCollider", f, x - 30, x + 30, y - 30, y + 30, 0, H, "Counter_Black").static_mesh_component.set_editor_property("visible", False)
    dress_tunnel(name, 1220, 2380, -4380, -3220, "x", random.Random(31), lamp_every=600, hw=580, walls=False)
    L = "Lighting/%s" % name
    light("spot", "Tunnel_ManholeBeam", L, (lx, ly - 40, H - 5), (0.75, 0.85, 1.0), 400, 600, rot=(-90, 0, 0),
          outer_cone_angle=14.0, inner_cone_angle=6.0, volumetric_scattering_intensity=6.0, shadows=True)
    spawn_marker(name, 1600, -4100)
    spawn_marker(name, 2100, -3550)


def camp():
    name = "Camp"
    tunnel_shell(name, -2000, -4400, 3, 3, {"E": [(1, 1)], "W": [(1, 1)]})
    f = "Rooms/%s/Props" % name
    rng = random.Random(44)
    p = []
    # mattresses + blankets along the north and south walls
    for (x, y, yaw) in ((-1700, -3320, 0), (-1350, -3330, 8), (-1650, -4280, -5)):
        p += [B(f, x, y, 0, 190, 90, 18, "Mattress", yaw=yaw), B(f, x + 20, y, 18, 120, 85, 6, rng.choice(["Velvet_Red", "Tarp_Blue", "Fabric_Grey"]), yaw=yaw + 6)]
        p.append(B(f, x - 70, y, 18, 35, 55, 14, "Label_White", yaw=yaw))                              # pillow
    # tarp shelter in the NW corner: two poles + sloped tarp
    p += [B(f, -1880, -3500, 0, 6, 6, 170, "Wood_Dark"), B(f, -1500, -3500, 0, 6, 6, 170, "Wood_Dark"),
          mesh("p", f, CUBE, (-1690, -3400, 190), (4.0, 2.4, 0.03), rot=(0, 0, 25), material="Tarp_Blue", kind=DECOR)]
    # fire barrel in the middle (flames + embers), crates as seats around it
    bx, by = -1400, -3800
    p += [C(f, bx, by, 0, 90, 60, "Rust"), C(f, bx, by, 88, 4, 62, "Rust"), C(f, bx, by, 80, 6, 54, "Counter_Black")]
    for k in range(5):
        a = k * 72
        p.append(mesh("p", f, CONE, (bx + 12 * math.cos(math.radians(a)), by + 12 * math.sin(math.radians(a)), 105),
                      (0.25, 0.25, rng.uniform(0.35, 0.6)), material="Fire", kind=DECOR))
    p.append(mesh("p", f, CONE, (bx, by, 112), (0.35, 0.35, 0.7), material="Fire", kind=DECOR))
    for k, a in enumerate((20, 140, 250)):
        p.append(B(f, bx + 130 * math.cos(math.radians(a)), by + 130 * math.sin(math.radians(a)), 0, 50, 40, 40,
                   "Wood_Crate", yaw=a))
    # shopping cart (wire frame) by the south wall
    cx, cy = -1050, -4230
    p += [B(f, cx, cy, 25, 90, 55, 2, "Chrome"), B(f, cx, cy - 27, 25, 90, 2, 60, "Chrome"), B(f, cx, cy + 27, 25, 90, 2, 60, "Chrome"),
          B(f, cx - 45, cy, 25, 2, 55, 60, "Chrome"), B(f, cx + 45, cy, 25, 2, 55, 65, "Chrome"), B(f, cx + 60, cy, 90, 4, 50, 3, "Fire_Red")]
    for (dx, dy) in ((-40, -24), (40, -24), (-40, 24), (40, 24)):
        p += [B(f, cx + dx, cy + dy, 8, 2, 2, 18, "Chrome"), C(f, cx + dx, cy + dy, 0, 8, 8, "Label_Black")]
    p += [B(f, cx, cy, 40, 70, 40, 30, rng.choice(["Trash_Black", "Cardboard"]))]
    # cardboard, bottles, a radio, more graffiti density
    for k in range(8):
        p.append(B(f, rng.uniform(-1950, -900), rng.uniform(-4350, -3250), 0.5, rng.uniform(40, 90), rng.uniform(30, 60), 1.2, "Cardboard",
                   yaw=rng.uniform(0, 180)))
    for k in range(10):
        ps, _ = bottle(f, rng.uniform(-1900, -1000), rng.uniform(-4300, -3300), 0, rng.choice(["wine", "vodka", "squat"]), rng, 1.0)
        p += ps
    p += [B(f, -1300, -3330, 18, 30, 12, 18, "Label_Black"), B(f, -1300, -3336, 22, 20, 1, 8, "Chrome")]
    merge(p, "Tunnel_CampBits", "Tunnel_CampBits", f)
    box("Tunnel_BarrelCollider", f, bx - 32, bx + 32, by - 32, by + 32, 0, 95, "Counter_Black").static_mesh_component.set_editor_property("visible", False)
    _pawn_only(box("Tunnel_CartCollider", f, cx - 48, cx + 48, cy - 30, cy + 30, 0, 90, "Counter_Black"))
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "Tunnel_CartCollider":
            a.static_mesh_component.set_editor_property("visible", False)
    dress_tunnel(name, -1980, -820, -4380, -3220, "x", random.Random(17), lamps=False, hw=580)
    L = "Lighting/%s" % name
    light("point", "Tunnel_FireBarrel", L, (bx, by, 150), (1, 0.45, 0.1), 60, 900, shadows=True)
    light("point", "Tunnel_FireBounce", L, (bx, by, 30), (1, 0.35, 0.08), 12, 400)
    spawn_marker(name, -1700, -3700)
    spawn_marker(name, -1150, -3450)


def build_tunnels():
    r = {"cleared": _clear()}
    club_threshold()
    # T1: threshold -> junction
    tunnel_shell("T1", 1600, -3200, 1, 3, {"N": [(0, 0)], "S": [(0, 0)]})
    dress_tunnel("T1", 1620, 1980, -3200, -2000, "y", random.Random(1))
    f = "Rooms/T1/Props"
    merge(stencil_arrow(f, 1622, -2600, 160, "y", 1, "Paint_Pink"), "Tunnel_T1Arrow", "Tunnel_T1Arrow", f)
    junction()
    # west: TW -> camp -> XW exit
    tunnel_shell("TW", -800, -4000, 5, 1, {"E": [(0, 0)], "W": [(0, 0)]})
    dress_tunnel("TW", -800, 1200, -3980, -3620, "x", random.Random(2))
    camp()
    tunnel_shell("XW", -2800, -4000, 2, 1, {"E": [(0, 0)]})
    dress_tunnel("XW", -2400, -2000, -3980, -3620, "x", random.Random(3), lamps=False)
    gate("XW", "y", -2400, -3980, -3620, "West")
    beyond("XW", -2800, -2400, -3980, -3620, ("x", -1))
    # east: TE with a gate at x 4800
    tunnel_shell("TE", 2400, -4000, 7, 1, {"W": [(0, 0)]})
    dress_tunnel("TE", 2400, 4800, -3980, -3620, "x", random.Random(4))
    gate("TE", "y", 4800, -3980, -3620, "East")
    beyond("TE", 4800, 5200, -3980, -3620, ("x", 1))
    # south: TS with a gate at y -5600
    tunnel_shell("TS", 1600, -6000, 1, 4, {"N": [(0, 0)]})
    dress_tunnel("TS", 1620, 1980, -5600, -4400, "y", random.Random(5))
    gate("TS", "x", -5600, 1620, 1980, "South")
    beyond("TS", 1620, 1980, -6000, -5600, ("y", -1))
    r["actors"] = len([a for a in _EAS.get_all_level_actors()
                       if any(str(a.get_folder_path()).startswith(("Rooms/%s" % m, "Lighting/%s" % m)) for m in MODULES)])
    r["sockets"] = sorted(a.get_actor_label() for a in _EAS.get_all_level_actors() if a.get_actor_label().startswith("LocationSocket"))
    return r
