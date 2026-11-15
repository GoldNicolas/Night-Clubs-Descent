# Map Design: Underground Club

**Owner:** Sebastian (map design) · **Branch:** `map-design` · **Status:** Draft for team review
**Written:** November 15, 2026
**Related:** [PLAN.md](../PLAN.md) (game plan) · [FPS-Agent-Constraints.md](../FPS-Agent-Constraints.md) (AI level rules, which are binding)

This document describes the look, layout, and building rules for the club map, and the assets it needs.

**How the team splits the work.** Each teammate owns a separate workstream:
- **Map design (this doc):** designs and fleshes out the club map until it's complete, in its own Unreal project, **`ClubMap/`** (see §8).
- **Procedural generation:** the layout generator on branch `map-generation` (see `MAPGEN.md` there), inside the team's `GameJam` project.
- **AI agent:** the enemy runtime in the `GameJam` project.

The map is built as **modular, grid-snapped rooms** that follow the AI's level rules. That way it can later be moved into `GameJam`, and its look and rooms can be reused by the generator (§8.4). The generator itself is not part of this document.

---

## 1. References

The reference images are stored locally in `ref images of nightclubs/`. They can be added to the repo later if the team wants them.

| Image | What we take from it |
|---|---|
| `blueprint layout.jpg` | **Layout.** A real club floor plan: central dance floor with DJ, island bar, perimeter bar, VIP lounge, private rooms, billiards, sports bar with a second dance floor and stage, two restroom blocks, storage, artist's lounge, front desk entry. |
| `imag3.jpg`, `night_club#1.jpg` | Blue LED-outlined bar counters, backlit bottle walls, hanging geometric neon frames, glossy floors. |
| `image 2.jpg` | Glowing floor tiles, hexagonal neon ceiling frames, a "Tron-grid" energy. |
| `images (1).jpg` | Pink LED-panel bar front, wet reflective floor, dark booths, a mural behind the bar. |
| `images (2).jpg` | A **neon portal doorway**: stacked magenta and blue light frames around an opening. |
| `images.jpg` | Mixed pink and teal zones, lounge sofas, screens, vertical neon signage. |
| `night_club#2.jpg`, `night_club#3.jpg`, `night_club#4.jpg` | Crowd density, layered club spaces, gold luxury accents, a dark overall exposure. |
| `outside_nightclub#1.webp` | Exterior: oversized neon sign, graffiti, posters, steam, grime. |

> Several references are screenshots from an existing game. They are used **only for general mood** (palette, LED-line lighting, reflective floors). We do **not** copy any specific venue, sign, logo, or room from that game.

---

## 2. Art direction

**One-line pitch:** a mob-run club hidden in storm-drain tunnels, with grimy raw concrete outside the club zones and sharp cyberpunk neon inside them.

### 2.1 Palette
- **Base:** deep black and blue-black. Most of every frame stays dark.
- **Primary light:** cyan / electric blue.
- **Accent light:** magenta / hot pink.
- **Rare accent:** warm gold, for VIP and luxury spots only.
- **Avoid:** flat purple haze over everything. Each zone should have a dominant color (a blue bar, a pink VIP, and so on) so players can tell where they are.

### 2.2 Lighting rules
1. **Light comes from architecture, not lamps.** Use LED lines tracing edges (bar fronts, stair edges, wall bases, ceiling frames), hanging geometric neon frames (rectangles, hexagons), backlit shelves, video walls, and glowing floor tiles.
2. **Neon portals mark doorways** between zones, as in `images (2).jpg`. They help the player find their way.
3. **Every glowing strip is paired with a matching colored light** (a rect or point light) so it lights the room around it.
4. **Keep counts low.** A few strong neon features per room beat many weak ones. (We learned this on the previous build.)
5. **Light haze only.** Use height fog close to the floor. It must not wash out the whole scene.
6. **Moving lights** (sweeping moving heads, a pulsing LED wall and dance floor) only in the dance areas.

### 2.3 Materials
- **Floors:** glossy black (polished concrete or black marble tile) with strong reflections. Wet concrete in the tunnels.
- **Walls (club):** dark 3D textured panels, dark wood with brass trim, black acoustic panels, studded upholstery **inside private rooms only**.
- **Walls (tunnels / back-of-house):** raw and stained concrete, brick, pipes, cables, graffiti and leak decals.
- **Furniture:** dark leather, black metal, glass-look tabletops (solid collision, see §3.4).
- **Corners:** rounded coves where walls meet ceilings, and rounded vertical corners in club rooms.

### 2.4 Stylization (to confirm with team)
PLAN.md calls for a **stylized, SIFU-inspired** look. The references lean **neon cyberpunk, semi-realistic**. Proposed blend: **simplified, chunky shapes and bold silhouettes** (the stylized part) with **cyberpunk neon lighting and reflective surfaces** (from the references), using scanned textures sparingly and darkened. → *Open question Q2.*

---

## 3. Building rules (from the AI constraints, mandatory)

These come from `FPS-Agent-Constraints.md` §11–12. Breaking them makes the AI enemies behave badly.

### 3.1 Grid and dimensions
| Rule | Value |
|---|---|
| Tile grid | **4 m × 4 m**. Every room footprint and wall snaps to it. |
| Wall height | **6 m** |
| Floor levels | **One walkable level per tile.** No balconies, walkways, mezzanines, or second storeys above combat areas. |
| Raised platforms | Steps of **≤ 45 cm** (one step). 1 m level changes only by **ramps of ≤ 14°** (1 m rise over one 4 m tile). A 1 m ledge can't be jumped. |
| Corridors / gaps | **≥ 4 m wide** (one tile). 2–4 m is borderline; avoid it. |
| Open areas | **10–40 m** across. |
| Low cover | **120 cm** (stand and shoot over it; hides a crouching body). |
| High cover | **250 cm** |
| Collision | **Simple box collision** everywhere (no complex/per-poly). |

### 3.2 Replacing balconies and mezzanines
The references show layered, multi-level clubs. We get that feeling **without a second floor**:
- **Low neon-topped walls (120 cm)** instead of balcony railings. They frame spaces and act as low cover.
- **Raised platforms up to 45 cm** for the DJ booth, VIP area, and stage, with a single step and an LED strip on the edge.
- **Ramped zones (≤ 14°)** for a 1 m height change between areas (for example, the sunken dance floor).
- **Tall backdrops** (video walls, bottle walls, neon signage) for vertical interest without walkable height.

### 3.3 Ceilings
Indoor ceilings confuse the AI's upward sensing. Following PLAN.md, ceilings and roofs go on a **custom collision object channel** that blocks pawns and the `Weapon` channel but is **ignored by AI perception rays**. This is only valid because nothing is walkable above them. Hanging decor (neon frames, trusses, pendants) has **no query collision**.

`ClubMap` defines this channel as **`Ceiling` = `ECC_GameTraceChannel4`** (object channel, default response Block). The team's `GameJam` project doesn't have it yet. See §8.2 and open question Q5.

### 3.4 What blocks the AI's vision
| Object | Collision |
|---|---|
| Walls, cover blocks, bar counters, booths, stage, pillars | **WorldStatic + blocks `Weapon`** (blocks sight **and** bullets) |
| Clutter: bottles, glasses, cables, litter, small props, decals, pickups | **Custom object type / no query collision.** Must not block AI vision. |
| Glass (tabletops, partitions) | **No see-through bulletproof glass.** Either fully solid and opaque, or no collision. |
| Thin fences / grates | Pick one: block both sight and bullets, or neither. |
| Doors | Allowed, but rare (moving geometry wasn't trained). Prefer open archways. |
| Not allowed | Ladders, jump pads, water, ziplines, vehicles, teleporters. |

### 3.5 Spawns
Each room provides **enemy spawn markers** at least **4 m** apart, on valid floor, with no overlap. The run logic picks ones that are **≥ 5 m from the player and out of view**.

---

## 4. Room kit (modules for the map and the generator)

### 4.1 Module rules
- **Each room is its own Level Instance** (a self-contained mini-level).
- **Naming:** `LI_<Room>_<WxD tiles>_<Variant>`, for example `LI_DanceHall_8x8_A`.
- **Origin:** the room's **south-west corner** on the grid. Floor top at **z = 0**. +X = east, +Y = north.
- **Doorway sockets:** openings are **centered on a tile edge** and **4 m tall**. Default width is **two tiles (8 m)**, which matches the generator's WFC door socket (`MAPGEN.md` §2b). One-tile (4 m) openings are the minimum, for back-of-house only. Each module lists its sockets (edge + tile index) so rooms can be connected by hand or by the generator.
- **Self-contained:** each module includes its own walls, ceiling (on the ceiling channel), lights, props, spawn markers, and a neon portal on each doorway.

### 4.2 Club modules (adapted from the blueprint)
| Module | Size (tiles) | Key contents | Gameplay role |
|---|---|---|---|
| Entry / Front Desk | 4×3 (built; 4 wide so the 8 m doors can be centered) | Desk, small bar, coat check, bouncer rope | Start / chokepoint |
| Main Dance Hall | 8×8 | Sunken dance floor (ramped 1 m), raised DJ platform (45 cm), island bar (3×3, square with chamfered neon outline), perimeter seating | Large open fight, 32 m across |
| Long Bar | 2×5 (built; west of the Dance Hall) | Bar counter (low cover), bottle wall, stools | Lane with cover |
| VIP Lounge | 4×5 (built; north of Billiards) | Raised 45 cm, curved booths (box collision), low neon walls, gold accents | Elevated-feel flank |
| Private Room | 2×2 (×2 variants) | Booth, table, champagne, studded walls | Small clearable room |
| Billiards | 4×3 (built; east of the Dance Hall, N door to the VIP) | 2–3 pool tables (low cover), cue racks, pendant lights | Mid-size cover maze |
| Sports Bar | 6×5 | Bar, table rows, TVs, small dance floor (2×2), stage (2×2, 45 cm) | Second large space |
| Side Bar | 2×2 | Compact bar | Connector |
| Restrooms | 2×3 (×2) | Stalls (high cover), sinks, mirrors | Tight close-quarters |
| Artist's Lounge | 2×2 | Sofa, mirror lights, clothes rack | Side room |
| Storage | 3×3 | Shelving, kegs, crates (high and low cover) | Back-of-house cover |
| Service Corridor | 1×N | Concrete, pipes, fluorescent lights | Back-of-house loop |

### 4.3 Tunnel modules (the Las Vegas storm-drain theme from PLAN.md)
| Module | Size (tiles) | Contents |
|---|---|---|
| Tunnel straight | 1×2, 1×3 | Wet concrete, graffiti, pipes, water stains, cable runs |
| Tunnel corner / T / cross | 1×1 to 3×3 | Same, with a junction light |
| Tunnel camp | 3×3 | Makeshift shelter, mattresses, fire barrel, shopping cart |
| Club threshold | 2×2 | Tunnel-to-club transition: neon portal, bouncer post, barrier |

### 4.4 Hand-assembled map (first milestone)
One complete map built from the modules above, following the blueprint's flow:
**Entry → Main Dance Hall (center) → Long Bar / VIP / Private Rooms (north) → Billiards / Side Bar (east) → Sports Bar + Stage (south) → Restrooms / Storage / Artist's Lounge / Service Corridor (back-of-house loop)**, with tunnel modules linking the club to two or three outer entry points.

Target footprint: about **16 × 18 tiles (64 × 72 m)** for the club, plus tunnels. The angled walls in the blueprint become square grid walls. The angles survive as neon outlines and furniture placement.

---

## 5. Asset list

### 5.1 Already have (from the earlier UE 5.8 prototype)
These can be **re-imported into the team's UE 5.7.4 project for free**. 5.8 assets can't be opened in 5.7, so they come in from source instead: the Meshy FBX files still exist, and Megascans/Fab items are re-added from your Fab library to the 5.7.4 project.

| Category | Assets |
|---|---|
| **Generated with Meshy (16)** | DJ controller, PA speaker stack, moving-head light, VIP booth, cocktail table, mirror ball, beer tap tower, champagne bucket, whiskey bottle, pendant light, chandelier, wall speaker, crowd barrier, fire exit door, coat rack, security camera, bar stool |
| **Megascans surfaces (13)** | Black marble tiles, dark wood planks, black wood planks, carpet, concrete floor, furniture fabric ×2, studded fabric, painted plaster, rough subway tiles, soundproof panels, brushed metal ×2 |
| **Megascans / Fab packs** | Stained, rough, and damaged concrete walls; dusty concrete floor; leak, grime, and grease decals; road cracks and debris decals; concrete barriers, barricades, cones, drums, pallets, cardboard |
| **Fab props** | Toilet, steel shelving, double shelf unit, cleaning supplies, office chair, chairs, ceiling and fluorescent lights, water bottles, velvet rope barrier, utility boxes, LED blade sign |
| **Neighborhood Market pack** | Dumpsters, AC units, drain pipes, electrical panels, exit signs, fire extinguishers, mop bucket, wet-floor sign, roll cage, pallet stacks, crates, cardboard boxes, vending machine, wine bottles, wall lamps |
| **Built in-engine (no asset needed)** | All walls, floors, ceilings, rounded coves, neon strips, geometric neon frames, neon portals, LED walls, glowing dance tiles, bar counters, platforms, low cover walls, rain, fog, lightning |

### 5.2 Still needed
| Asset | Use | Source |
|---|---|---|
| Wet / stained concrete tunnel surfaces, graffiti decals | Tunnels | **Megascans** (free) |
| Pipes and cable runs kit | Tunnels, back-of-house | Megascans / Fab free / build in-engine |
| 3D textured wall panel surface | Club walls | Megascans / ambientCG (free) |
| Pool table | Billiards | **Meshy** |
| Slot machine | Vegas flavor (VIP, side bar) | **Meshy** |
| Card / poker table | Back room, VIP | **Meshy** |
| Leather sofa and lounge armchair | VIP, artist's lounge, private rooms | **Meshy** |
| Mattress, tarp shelter, shopping cart | Tunnel camp | **Meshy** / Megascans |
| Fire barrel | Tunnel camp | Oil drum (have) + fire VFX |
| Wall-mounted TV / video panel | Sports bar, VIP | Build in-engine (emissive panel) / Meshy |
| Cue rack, glass rack, ice well | Billiards / bars | Meshy (optional) |
| Music, crowd noise, thunder | Atmosphere | **WeLoveIndies** (free, non-commercial) |
| Crowd / dancers | Ambient NPCs | Team decision (stylized characters per PLAN.md) |

### 5.3 Budget
- **Meshy:** about **470 free credits** left, which is about **15 props** at 30 credits each. Priority: pool table, slot machine, card table, sofa, armchair, then tunnel-camp items. **No paid credits.** We stop when the free credits run out.
- Everything else comes from free sources (Megascans, Fab free, ambientCG, Poly Haven, WeLoveIndies).

---

## 6. Open questions for the team
| # | Question | Why it matters |
|---|---|---|
| ~~Q1~~ | ~~Engine version~~ **Resolved: the team uses UE 5.7.4** (C++ project, matching PLAN.md and the AI requirements). | All rooms are built in 5.7.4. Assets from the earlier 5.8 prototype are re-imported from source files, not migrated. |
| Q2 | **Style blend:** stylized SIFU-like shapes + cyberpunk neon (proposed), or another direction? | Decides materials and prop style. |
| Q3 | **Tunnels vs. club balance:** how much of the map is tunnel vs. club? | Decides the module count and map size. |
| Q4 | **How the generator reuses this work** (with the generator owner): (a) the club's look replaces its grey boxes, (b) some rooms are built as 20 × 20 m WFC modules, or both. See §8.4. | Decides whether some rooms must be exactly 5 × 5 tiles with 8 m door sockets. |
| Q5 | **Ceiling channel:** adopt `Ceiling` = `ECC_GameTraceChannel4` in `GameJam` too, and exclude it from agent perception queries (AI constraints §11.2). | `ClubMap` already uses it. The same slot and name in `GameJam` let the map move over with working ceilings. |
| Q6 | **Crowd / NPC** source and style. | Affects the dance areas and the performance budget. |

---

## 7. Build order
1. ~~Set up the `ClubMap` project~~ **Done:** UE 5.7.4, collision channels, 4 m grid, default level (§8). Still to resolve with the team: Q2, Q4, Q5.
2. Create the room-kit template (grid, wall, ceiling channel, doorway socket, neon portal, spawn markers).
3. Build the **Main Dance Hall** first, as the visual and gameplay benchmark. Review it with the team.
4. Build the remaining club modules, then the tunnel modules.
5. Assemble the full hand-built map. Playtest movement, sightlines, and cover heights.
6. Hand the modules and socket list to the generator owner.

---

## 8. The `ClubMap` project and integration

### 8.1 Project
| Item | Value |
|---|---|
| Location in repo | `ClubMap/ClubMap.uproject` (branch `map-design`) |
| Engine / type | UE **5.7.4**, **Blueprint** project (no C++ build needed to open it) |
| Content root | Everything lives under **`/Game/ClubMap/`**: `Maps`, `Rooms`, `Materials` (+ `WorldAligned`), `Textures`, `Meshes`, `Props`, `Lighting`, `FX`, `Blueprints`, `Audio`. One root folder keeps the move into `GameJam` clean. |
| Main level | `/Game/ClubMap/Maps/Lvl_ClubMap` (editor startup map and game default map) |
| Editor grid | **400 cm (4 m)** snap option added and set as the active snap |
| Editor-only helpers | `Editor_WorkLight` (a directional light, hidden in game) and `GridReference_Floor`. Both are in the `Setup` outliner folder and are removed or hidden for the final map. |
| Rendering | Maximum quality preset (high-end features available). Low-end players use Unreal's scalability levels (Low…Epic) through an in-game graphics menu added later. The map must still read well with Lumen off (Low). `r.LumenScene.SurfaceCache.AtlasSize=6144` is set to avoid surface-cache oversubscription. |

### 8.2 Collision channels (match `GameJam`)
| Slot | Name | Kind | Default | Notes |
|---|---|---|---|---|
| `ECC_GameTraceChannel1` | Projectile | object | Block | Same as `GameJam` (template) |
| `ECC_GameTraceChannel2` | **Weapon** | trace | Block | Same as `GameJam`. Hitscan for player and agents. |
| `ECC_GameTraceChannel3` | **AgentPawn** | object | Block | Same as `GameJam`. Agent capsules. |
| `ECC_GameTraceChannel4` | **Ceiling** | object | Block | **New, proposed for `GameJam` (Q5).** Ceilings and roofs. Blocks pawns and Weapon, ignored by agent perception. |

Level geometry rules (§3.4): walls, cover, and counters are **WorldStatic** and block Weapon. Clutter, decals, and hanging decor have **no query collision**. Ceilings use the **Ceiling** object type.

### 8.3 Moving the map into `GameJam`
1. In `ClubMap`, right-click `/Game/ClubMap`, choose **Asset Actions → Migrate**, and select `GameJam/Content`. Unreal copies the level and every asset it references.
2. In `GameJam`, the `Ceiling` channel must exist in the same slot (Q5) before the migrated level is opened.
3. Open the level in `GameJam` and press Play. The player (`BP_GJPlayer`) and the agent encounter system work in any level there (CLAUDE.md, `GameJam` branch).
4. Run the map checks (`gj.MapCheck`-style traces: Weapon and WorldStatic must agree at walls and cover).
5. Merge through a pull request.

A **trial migration** happens partway through building, so problems show up early.

### 8.4 How the generator can reuse this work (Q4)
The generator (`MAPGEN.md`) builds layouts on the same 4 m grid. It uses box collision plus **collision-free instanced meshes** for looks, currently grey placeholder cubes. Two compatible routes:
- **(a) Art kit:** the club's materials, wall and floor styles, neon trims, ceilings (art only), and room dressing sets (dance hall, bar, VIP, back room, tunnel) replace its placeholder cubes. Dressing is placed by room size. This needs no change to the generator's layout logic.
- **(b) WFC room modules:** selected rooms are built exactly to the generator's WFC module format (**5 × 5 tiles = 20 m**, each module owning its north row and west column as border, sockets `0` wall / `1` 8 m door / `2` open edge), so it can stitch them directly.

The hand-built `Lvl_ClubMap` also stays usable as a fixed showcase level.

### 8.5 Editor automation (for Claude Code)
- `ClubMap` is controlled through the community **Unreal-MCP** plugin (the same one used for `GameJam`), installed with `npx unreal-mcp-cli install-plugin ./ClubMap --with-server` and set to **local-only mode** with `configure --mode Custom --host http://localhost:8080 --auth none`. The plugin folder (`ClubMap/Plugins/UnrealMCP/`), `.env`, and `.mcp.json` are git-ignored and machine-specific.
- On Windows, compiling the plugin needs Visual Studio with the **.NET Framework 4.8.1 SDK** and the **MSVC v143 (14.44)** toolset.
