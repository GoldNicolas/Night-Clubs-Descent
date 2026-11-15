"""Main Dance Hall interior (8x8 tiles at origin). Run with include=[cm_kit.py] after room_shell("DanceHall", ...).
Cover heights follow the AI contract: low cover 120 cm, high cover 250 cm, platforms 45 cm (one step).
"""
import unreal

N = "DanceHall"
F = "Rooms/%s/" % N
MAG, CYAN, BLUE, GOLD, WHITE = (1, .1, .55), (.15, .75, 1), (.1, .25, 1), (1, .6, .25), (1, .95, .9)


def build():
    # ---------------- dance floor: 4x4 glowing tiles (X/Y 800..2400) + gold edge
    f = F + "DanceFloor"
    cols = ["Tile_Magenta", "Tile_Cyan", "Tile_Blue"]
    for i in range(4):
        for j in range(4):
            x, y = 800 + i * 400, 800 + j * 400
            box("DanceTile", f, x + 8, x + 392, y + 8, y + 392, 0, 2, cols[(i + j) % 3], DECOR)
    for (a, b, c, d) in ((790, 2410, 790, 800), (790, 2410, 2400, 2410), (790, 800, 800, 2400), (2400, 2410, 800, 2400)):
        box("DanceFloor_Edge", f, a, b, c, d, 0, 3, "Neon_Gold", DECOR)
    # four pillars at the dance-floor corners (structure / full cover) with neon on the inner faces
    for (px, py, sx, sy) in ((800, 800, 1, 1), (2400, 800, -1, 1), (800, 2400, 1, -1), (2400, 2400, -1, -1)):
        box("Pillar", f, px - 40, px + 40, py - 40, py + 40, 0, WALL_H, "Wall_Panel")
        box("Pillar_Neon", f, px + sx * 40 - 2, px + sx * 40 + 2, py - 4, py + 4, 20, 560, "Neon_Magenta", DECOR)
        box("Pillar_Neon", f, px - 4, px + 4, py + sy * 40 - 2, py + sy * 40 + 2, 20, 560, "Neon_Cyan", DECOR)

    # ---------------- DJ stage (north): platform 45 cm, desk = low cover, speakers = high cover, LED wall
    d = F + "DJ"
    box("DJ_Platform", d, 1000, 2200, 2600, 3180, 0, 45, "Platform_Dark")
    box("DJ_Platform_Edge", d, 1000, 2200, 2596, 2600, 33, 40, "Neon_Cyan", DECOR)
    box("DJ_Desk", d, 1300, 1900, 2760, 2860, 45, 165, "Counter_Black")          # 120 cm above the platform
    box("DJ_Desk_Neon", d, 1300, 1900, 2756, 2760, 140, 148, "Neon_Magenta", DECOR)
    for sx0 in (650, 2350):
        box("Speaker_Stack", d, sx0, sx0 + 200, 2850, 3150, 0, 250, "Speaker_Black")   # high cover
        box("Speaker_Neon", d, sx0, sx0 + 200, 2846, 2850, 235, 242, "Neon_Cyan", DECOR)
    scr = ["Screen_Blue", "Screen_Pink", "Screen_Cyan"]
    for i in range(6):
        for k in range(2):
            x0 = 800 + i * 270
            box("LED_Panel", d, x0 + 6, x0 + 264, 3172, 3178, 180 + k * 190, 364 + k * 190, scr[(i + k) % 3], DECOR)

    # ---------------- long bar (west): counter = low cover, back bar with glowing bottle wall
    b = F + "Bar"
    box("Bar_Counter", b, 250, 370, 400, 1800, 0, 112, "Counter_Black")
    box("Bar_Top", b, 240, 380, 390, 1810, 112, 120, "Chrome")                      # 120 cm total
    box("Bar_Neon", b, 370, 374, 400, 1800, 10, 16, "Neon_Magenta", DECOR)
    box("Bar_Neon", b, 370, 374, 400, 1800, 100, 106, "Neon_Magenta", DECOR)
    box("BackBar", b, 20, 90, 400, 1800, 0, 110, "Counter_Black")
    for (y0, y1) in ((380, 400), (1800, 1820)):   # close the 1.6 m bartender aisle (no AI dead-end)
        box("Bar_Return", b, 90, 250, y0, y1, 0, 112, "Counter_Black")
        box("Bar_Return_Top", b, 90, 250, y0 - 5, y1 + 5, 112, 120, "Chrome")
    box("BackBar_Glow", b, 20, 24, 450, 1750, 140, 330, "Bottles_Glow", DECOR)
    for z in (190, 260):
        box("BackBar_Shelf", b, 24, 70, 450, 1750, z - 3, z, "Chrome", DECOR)

    # ---------------- raised VIP corner (south-east): 45 cm platform, low neon walls, booths
    v = F + "VIP"
    box("VIP_Platform", v, 2600, 3180, 20, 1100, 0, 45, "Platform_Dark")
    box("VIP_LowWall", v, 2600, 2630, 420, 1100, 0, 120, "Wall_Panel")              # 4 m entry gap y 20..420 (AI gap rule)
    box("VIP_LowWall", v, 2630, 3180, 1070, 1100, 0, 120, "Wall_Panel")
    box("VIP_LowWall_Neon", v, 2596, 2634, 420, 1100, 120, 126, "Neon_Gold", DECOR)
    box("VIP_LowWall_Neon", v, 2630, 3180, 1066, 1104, 120, 126, "Neon_Gold", DECOR)
    for (y0, y1) in ((150, 500), (620, 970)):
        box("VIP_Booth_Seat", v, 3000, 3180, y0, y1, 45, 90, "Leather_Wine")
        box("VIP_Booth_Back", v, 3120, 3180, y0, y1, 90, 165, "Leather_Wine")
        cyl("VIP_Table", v, 2860, (y0 + y1) / 2, 45, 100, 70, "Counter_Black")

    # ---------------- booths either side of the south entrance (low cover)
    s = F + "Booths"
    for (x0, x1) in ((300, 1050), (2150, 2550)):
        box("Booth_Seat", s, x0, x1, 20, 180, 0, 45, "Leather_Wine")
        box("Booth_Back", s, x0, x1, 20, 60, 45, 120, "Leather_Wine")
        cyl("Booth_Table", s, (x0 + x1) / 2, 300, 0, 75, 80, "Counter_Black")

    # ---------------- hanging neon frames over the dance floor (decor)
    h = F + "Ceiling"
    for (inset, z, m) in ((100, 520, "Neon_Magenta"), (260, 480, "Neon_Cyan"), (420, 440, "Neon_Magenta")):
        x0, x1, y0, y1 = 800 + inset, 2400 - inset, 800 + inset, 2400 - inset
        for (a, bb, c, dd) in ((x0, x1, y0, y0 + 6), (x0, x1, y1 - 6, y1), (x0, x0 + 6, y0, y1), (x1 - 6, x1, y0, y1)):
            box("Neon_Frame", h, a, bb, c, dd, z, z + 6, m, DECOR)

    # ---------------- lights: every neon feature gets a matching light; a few beams
    L = "Lighting/%s" % N
    light("rect", "Frame_Light", L, (1600, 1600, 590), MAG, 120, 1800, rot=(-90, 0, 0), shadows=True,
          source_width=1200.0, source_height=1200.0)
    light("rect", "LEDWall_Light", L, (1600, 3150, 370), BLUE, 140, 2200, rot=(0, -90, 0), source_width=1600.0, source_height=380.0)
    light("rect", "BackBar_Light", L, (40, 1100, 240), CYAN, 80, 900, rot=(0, 0, 0), source_width=1300.0, source_height=190.0)
    light("point", "DanceFloor_Uplight", L, (1600, 1600, 60), (0.5, 0.15, 1.0), 40, 1200)
    light("spot", "DJ_Key", L, (1600, 2700, 580), WHITE, 300, 900, rot=(-80, 90, 0), shadows=True, outer_cone_angle=28.0)
    light("point", "VIP_Gold", L, (2900, 560, 400), GOLD, 50, 900)
    for (x, y, yaw, c) in ((100, 100, 45, MAG), (3100, 100, 135, CYAN), (100, 3100, -45, CYAN), (3100, 3100, -135, MAG)):
        light("spot", "Beam", L, (x, y, 580), c, 1500, 2600, rot=(-35, yaw, 0), outer_cone_angle=10.0,
              inner_cone_angle=3.0, volumetric_scattering_intensity=3.0)
    for (x, y) in ((1600, 60), (3140, 1600), (60, 2400)):
        light("point", "DoorPortal_Glow", L, (x, y, 380), MAG, 30, 600)

    # ---------------- enemy spawn markers (>= 4 m apart, on open floor)
    for (x, y) in ((500, 2400), (2900, 2000), (1600, 500), (2900, 1500), (600, 600), (2000, 2000)):
        spawn_marker(N, x, y)
