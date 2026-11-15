# Procedural map generation

**Status:** first version implemented on branch `map-generation`, 2026-11-14. The `tunnels` family and the club map's socket maps (§5b) were added on branch `club-map`, 2026-11-15. It compiles in UE 5.7.4 (Linux), and the headless self-test (§7) passes on every family and on the club level. It has not yet been play-tested with a screen.
**Source:** adapted from a procedural-layout guide for a research environment (graph-first rooms and wave function collapse). Only what serves this game was kept. The rules come from PLAN.md §8 and the FPS-Agent contract §11; where the original guide differs, the contract wins.

## 1. What it does

```text
(family, level, seed)
  -> tile grid on FPS-Agent's 4 m grid (map format v1)    GJMap::GenerateGraph / GenerateWfc / GenerateTunnels
  -> keep the largest connected region, outer wall ring
  -> cover blocks in rooms (never disconnecting the map)  AddCover
  -> player start, facing, zones a / d                    PlaceStartAndZones
  -> validate + metrics + map format v1 text              Validate, ComputeMetrics, ToMapFormatV1
  -> merged boxes -> collision + instanced meshes         AGJMapGenerator (MergeRects)
```

- **Same seed, same map.** A layout is fully determined by `(family, level, seed)`. All randomness uses a splitmix64 PRNG, never `std::` distributions, whose output differs between compilers. The seed is logged on every play, and `gj.Map <family> <level> <seed>` rebuilds that exact map.
- **Contract scale.** Every tile is 4 m:
  - walls `#` are 6 m tall;
  - cover is `l` (120 cm) or `h` (250 cm);
  - rooms are at least 3 tiles (12 m) wide, and corridors are 1–2 tiles (4–8 m);
  - single floor level, open sky, simple box collision.
  This is the space the agents trained on (contract §11.1). The original guide's 0.5 m cells and 1 m walls do not apply.
- **Cheap.** Under 3 ms per layout at every level, so a new map per run (or per wave) costs nothing.

## 2. Families

### 2a. Graph-first rooms (`graph`)
1. Places non-overlapping rectangular rooms, with at least one wall tile between them.
2. Joins them with a minimum spanning tree, so every room is reachable.
3. Adds back the shortest non-tree edges as **loops**.
4. Routes corridors with Dijkstra over a cost map that penalises turns and passing through other rooms. This fixes the original guide's "corridors cut through rooms" issue, so the planned loop count is close to the real one.

| Parameter | Controls | Harder when |
|---|---|---|
| `Rows`, `Cols` | map size in tiles (x 4 m) | larger |
| `NumRooms` | rooms attempted; the placed count is logged | more |
| `RoomMin`, `RoomMax` | room side in tiles (min 3 = 12 m) | smaller, more uniform |
| `Corridor` | corridor width in tiles: 2 = 8 m (preferred), 1 = 4 m (contract minimum) | narrower |
| `LoopFrac` | extra edges beyond the tree, as a fraction of `NumRooms` | more loops: more flanking routes |
| `Cover` | fraction of room tiles that become cover | denser (capped at 12%) |

### 2b. Wave function collapse (`wfc`)
- **Modules.** The map is a grid of **5 x 5-tile modules (20 m)**. Each module owns its north row and west column as its border, so walls between modules are one tile thick, as in format v1.
- **Sockets per side:**
  - `0` wall;
  - `1` door, a 2-tile (8 m) opening;
  - `2` open room-to-room edge, which merges modules into halls and leaves a 1-tile pillar at the corners.
- **Module kinds:**
  - empty (solid);
  - corridor, 8 m wide, with sockets 0/1 only;
  - room, a 16 m interior with sockets 0/1/2.
- **Retries.** Collapse can contradict, or leave too little connected space. The generator then retries up to `MaxTries` times with the same seeded stream. It returns false if every try fails, and the actor then moves on to the next seed and logs it.

| Parameter | Controls | Harder when |
|---|---|---|
| `ModulesX`, `ModulesY` | map size in modules (x 20 m) | larger |
| `WRoom` | how common room modules are | lower |
| `WCorr` | how common corridor modules are | higher (maze-like networks) |
| `WOpen` | weight multiplier per open (2) edge | lower (small rooms in place of big halls) |
| `WEmpty` | how common solid modules are | higher (longer detours) |
| `MinKeep` | minimum share of open space the largest region must keep (0.85) | — |
| `bClearPillars` | clear the corner pillar where four open rooms meet | off by default: pillars are good cover |

### 2c. Storm-drain tunnels (`tunnels`)
The graph family with tunnel settings, built to attach to a hand-built level (§5b):
- **Chambers** of 3–5 tiles (12–20 m) joined by **1-tile (4 m) drains**, with loops for flanking routes.
- **A mouth:** one opening in the south wall (the last row) at column `EntryC`, where the network continues a tunnel outside the map. A 1-tile entry "room" sits just inside it, so the corridor tree always reaches it. The start is that tile, facing north. `Validate` allows this one gap in the outer wall.
- `TunnelPreset(Level)` sets the size (12–20 tiles), chambers (3–9), loops (0.3–0.6) and cover (4–12%). `GenerateTunnels(Params, EntryC, Seed)` takes any size and mouth column, which is how a socket map shrinks to fit.
- On its own (a standalone map, or `gj.Map tunnels` in `Lvl_MapGen`) the mouth is closed by a wall one tile past the map.

## 3. Levels (measured)
The `Level` (1–5) presets are in `GraphPreset` / `WfcPreset` / `TunnelPreset`. **A fractional level blends the two neighbouring presets** (2.5 is halfway between L2 and L3). Use it to ramp the map smoothly over a run, rather than jumping between five fixed settings.

The table shows 30 seeds per level, from `Tools/MapGen/mapgen_test stats`. "Farthest" is the walking distance from the start to the farthest reachable tile. "Boxes" is the number of merged colliders.

| Family | Level | Side (m) | Free area (m²) | Farthest walk (m) | Boxes | Cover tiles | Rooms | Planned loops |
|---|---|---|---|---|---|---|---|---|
| graph | L1 | 64 | 1565 ± 311 | 62 ± 14 | 15 | 4 | 3.5 | 0 |
| graph | L2 | 80 | 2500 ± 236 | 89 ± 14 | 27 | 8 | 5.7 | 1 |
| graph | L3 | 96 | 3357 ± 269 | 117 ± 28 | 39 | 13 | 9 | 2 |
| graph | L4 | 112 | 3422 ± 172 | 155 ± 31 | 59 | 18 | 12 | 4 |
| graph | L5 | 128 | 4645 ± 246 | 162 ± 32 | 83 | 29 | 16 | 8 |
| wfc | L1 | 64 | 2428 ± 282 | 69 ± 14 | 17 | 5 | — | — |
| wfc | L2 | 84 | 3313 ± 647 | 97 ± 30 | 28 | 10 | — | — |
| wfc | L3 | 104 | 3998 ± 1080 | 118 ± 29 | 37 | 13 | — | — |
| wfc | L4 | 124 | 4929 ± 1040 | 158 ± 33 | 44 | 13 | — | — |
| wfc | L5 | 144 | 5873 ± 1252 | 200 ± 46 | 45 | 11 | — | — |
| tunnels | L1 | 48 | 735 ± 84 | 47 ± 6 | 13 | 1 | 3 | 1 |
| tunnels | L2 | 56 | 1042 ± 77 | 59 ± 5 | 19 | 3 | 4 | 2 |
| tunnels | L3 | 64 | 1511 ± 146 | 70 ± 8 | 25 | 6 | 5 | 3 |
| tunnels | L4 | 72 | 1929 ± 126 | 85 ± 7 | 33 | 10 | 6.6 | 4 |
| tunnels | L5 | 80 | 2405 ± 166 | 98 ± 13 | 45 | 15 | 8.7 | 5 |

- **Graph levels:**
  - L1 has 4 large rooms joined by 8 m corridors, with no loops.
  - L5 has 16 small rooms joined by 4 m corridors, with about 8 loops and the most cover.
- **WFC levels:**
  - L1 is mostly merged halls.
  - L5 is mostly a corridor network with small rooms and solid blocks.
- **Tunnel levels** are a third of a graph map's area or less, since several of them hang off one club (§5b).
- **The families aren't equivalent.** WFC L3 isn't "as hard as" graph L3.
- **Map size and encounters.** The reference map `map01` is 120 x 120 m, and engagements stay within about 40 m (contract §7.5). L4–L5 maps are large for a survival arena. Tune encounters on L1–L3 first.

## 4. Gameplay layer (replaces the original "foraging" layer)
- **Start.**
  - The start is a random room tile, facing the nearest way out of the room.
  - `AGJMapGenerator` replaces the level's PlayerStarts with one there, so respawns land on it too, and moves any player pawn already spawned.
- **Zones.** Each zone covers 12 m of walking around its tile, kept inside that tile's room. They are exported for FPS-Agent and available for encounter design (PLAN §8.1). The live spawner doesn't use them; it picks tiles by walking distance from the player.
  - `a` surrounds the start.
  - `d` surrounds the tile farthest from the start.
- **Enemy spawns.**
  - `UGJEncounterSubsystem` asks the map for floor tiles within 5–60 m of **walking** distance from the player. It probes random points on them and keeps its existing rules: ≥ 5 m from the player and ≤ 40 m in a straight line, ≥ 4 m between agents, capsule clearance, out of view preferred, facing away from the player (contract §12.5).
  - Enemies can therefore never spawn in a sealed-off pocket.
  - The encounter director knows the player's position; the policy doesn't (PLAN §10).
- **Walking-distance field.** `GJMap::WalkDistance` uses 8 neighbours with no corner cutting. It is meant for spawn selection, pickup and perk placement, and kill-streak or score normalisation later.

## 5. Unreal integration
- **`Source/GameJam/MapGen/GJLayoutGen.*`** is the generator: plain C++17 with no Unreal types, so it is built and tested outside the engine.
- **`Source/GameJam/MapGen/GJMapGenerator.*`** is `AGJMapGenerator`, the actor:
  - **Collision.** One `UBoxComponent` per merged rectangle, plus one floor slab. They use `BlockAll`: object type WorldStatic, blocking every channel, including `Weapon` and `AgentPawn`. Agents perceive WorldStatic, so line of sight and bullets agree (contract §11.2).
  - **Looks.** Instanced meshes with **no collision**. They use the template's `SM_Cube`, scaled from its bounds, and the prototype-grid materials. Swap them for the tunnel/club art kit later; collision stays boxes.
  - **Placement.** The actor's location is the map's **south-west corner at floor level**. Row 0 is north (+X), and column 0 is west (+Y points east), as in format v1.
  - **On play** it draws a random seed (when `bRandomSeedOnPlay`), builds the map, places the player and writes `Saved/MapGen/<family>-L<level>-<seed>.txt` in map format v1.
  - **In the editor** the **Generate** and **Generate New Seed** buttons preview a layout.
- **Console** (every generator in the level):
  - `gj.Map <graph|wfc|tunnels> <level> [seed]` rebuilds the maps while playing; enemies are removed and, on a standalone map, the player moves to the new start. With several generators, map *i* gets seed `seed + 7919 i`, so one number reproduces the whole level. Socket maps are always `tunnels`.
  - `gj.MapInfo` prints each map's tuple and metrics.
- **Test level:** `/Game/GJ/Lvl_MapGen`. It holds a sun, a sky light, a sky atmosphere, fog, a placeholder PlayerStart and `GJMapGenerator`. Its game mode is `BP_FirstPersonGameMode` (GJ player, controller and HUD). It is a plain level, not World Partition.

### 5b. The club map: tunnels at location sockets
The club is Sebastian's hand-built map (branch `map-design`, `docs/MAP_DESIGN.md` there). Its storm drains end at three gated **location sockets**, West, East and South: TargetPoints tagged `LocationSocket` + the direction, at the gate, facing out. Past each gate the tunnel fakes going on (fog veils, a black end wall) for 4 m, to the end of its module.

- **Content.** `/Game/ClubMap/` is a copy of `ClubMap/Content/ClubMap/` from `map-design`, unchanged; it needs nothing outside that folder. To take a newer club map, copy the folder over again.
- **Level.** `/Game/GJ/Lvl_Club` loads `/Game/ClubMap/Maps/Lvl_ClubMap` as an always-loaded sublevel and holds one `AGJMapGenerator` per socket (`Socket` = West/East/South, tunnels L2, roof and lamps on, the club's wet-concrete and sodium-lamp materials). `Tools/MapGen/make_club_level.py` rebuilds it (command in the script).
- **On play,** each socket map:
  1. finds its socket and the tunnel floor under it;
  2. **hides the gate and the fake end:** every mesh or light lying wholly between 75 cm behind the marker and the end of the tunnel, within its 4 m width (37 actors per socket: bars, rails, lock, warning lamp, veils, end wall and its coves, far lamp). The tunnel's floor, side walls and ceiling run past the marker, so they stay. This goes by geometry, because actor labels and outliner folders don't exist in a packaged game;
  3. picks a size and mouth column: the preset size first, mouth columns in a seeded order, shrinking 2 tiles at a time until the map plus `SocketClearanceCm` (1 m) is clear of anything in the level that blocks a pawn and of the other socket maps;
  4. moves itself there and builds: the mouth tile starts where the socket's tunnel ends;
  5. leaves the player start alone (the club's own, in the Entry).
- **Under a roof** (`bRoof`): a 20 cm ceiling slab at `RoofHeightCm` (400 cm, the club's tunnel height) on the **`Ceiling`** object channel (`ECC_GameTraceChannel4`, same slot and name as in `ClubMap`). It blocks pawns and bullets, but agent perception queries WorldStatic only, so agents don't see it (contract §11.2; nothing is walkable above it). Walls stop at the roof.
- **Lamps** (`bLamps`): unshadowed point lights under the roof, about 3 tiles apart (greedy along the walk from the mouth, at most 40 per map), each with a small emissive fitting.
- **Enemies.** In a level with `EnemySpawn`-tagged markers or socket maps, the encounter subsystem picks from the club's markers plus every socket map's floor tiles, with the usual rules (≥ 5 m from the player, ≤ 40 m, ≥ 4 m apart, out of view preferred). Its floor probe starts 1.5 m above the candidate, under any ceiling.
- **Socket contract notes for the club map:**
  - A tunnel's own **end wall** past the gate has to go too (MAP_DESIGN's §4.4 lists only the gates and `Beyond`); socket maps hide it.
  - The marker stands in the gate's middle bar, so the floor is probed 1.5 m back toward the club.
  - The club's tunnels are 3.6 m wide inside (20 cm walls inside the tile); generated drains are a full 4 m, so there is a 20 cm step in the wall plane at each mouth.

## 6. Rules the generator must keep (from the contract)
- **No ceilings, bridges or stacked floors.** One walkable floor per tile. Tunnel ceilings are art only, on the special ceiling collision channel (PLAN §8).
- **Cover never disconnects the map.** It never touches a doorway or corridor opening, and it stays at or under 12% of the walkable area (the S1 training density).
- **Collision is real level geometry only.** Decoration, dancers, pickups and ragdolls must not block WorldStatic/WorldDynamic queries or the `Weapon` channel (PLAN §9).
- **Corridors are ≥ 4 m, rooms 12–40 m.** Steps need ramps; nothing must be jumped.

## 7. Testing
```bash
Tools/MapGen/build.sh /tmp/mapgen_test                # strict warnings (-Wall -Wextra -Wshadow -Wconversion -Werror)
/tmp/mapgen_test stats 30                             # every family x level: validity, determinism, metrics, timing; exit 1 on a problem
/tmp/mapgen_test show graph 2.5 42                    # ASCII view (@ start, a/d zones, # wall, l/h cover) + metrics
/tmp/mapgen_test dump /tmp/maps 3                     # map format v1 files
```
Run `stats` after any change to the generator, and refresh the table in §3 when the numbers move.

**In the game, headless** (needs the editor build, see CLAUDE.md):
```bash
cd GameJam && ~/Downloads/unreal_engine/Engine/Binaries/Linux/UnrealEditor $PWD/GameJam.uproject /Game/GJ/Lvl_MapGen \
  -game -nullrhi -nosound -unattended -nosplash -stdout \
  -ExecCmds="gj.Enemies 4, gj.SpawnInterval 1, gj.MapSelfTest 15 quit" | grep -E 'MapCheck|MapSelfTest'
```
- **`gj.MapCheck`** checks the built map against its grid:
  - from every 3rd floor tile, in 4 directions, at 60 and 200 cm, Weapon-channel and WorldStatic/WorldDynamic traces must stop exactly where the grid has a wall or cover;
  - the player must stand on a floor tile;
  - every living enemy must be on a floor tile the player can walk to.
  - on a socket map, the player and enemies elsewhere in the level are skipped; instead a standing capsule must pass from 1.5 m inside the club's tunnel, through the mouth, to the tile inside it, and the two floors must meet within 5 cm.
- **`gj.MapSelfTest [delay] [quit]`** checks every map in the level, rebuilds them (a standalone map as the other family, socket maps with new seeds), checks again (the player must now be on the new start), waits, checks once more and logs `MapSelfTest: PASS`. With `quit`, it exits with code 1 on failure.
- **The club level:** the same command with `/Game/GJ/Lvl_Club` and `-ExecCmds="gj.MapSelfTest 10 quit"`. The `MapGen: socket ...` lines in `Saved/Logs/GameJam.log` give each socket's placement and the number of hidden actors.

## 8. Not done yet / next steps
- **Heights and ramps** (`heights` 0–9, ramp tiles `^ v > <`). The format supports them, but the generators emit level 0 only. Ramps must rise 1 m per 4 m tile; drops are one-way.
- **Tunnel dressing.** Socket maps use the club's wet-concrete materials and sodium lamps, but none of its drain dressing (water channel, grates, pipes, graffiti, stains). Dressing must follow §6.
- **Socket maps that branch.** A socket map could offer sockets of its own (more locations further out); not built.
- **Club and room dressing** on standalone maps.
  - Tag rooms by size: large halls as dance floors (crowd, lights, DJ booth), small rooms as back rooms, corridors as tunnels.
  - Per-room wall colours or landmark props help the player find their way.
  - Decoration must follow §6.
- **Exact export syntax.** The exact text syntax of FPS-Agent's map format v1 file isn't in this repo. `ToMapFormatV1` writes the three grids plus the `tile_cm` / `level_cm` / `wall_cm` keys. Check it against a real `map01` file before handing maps to FPS-Agent.
- **Not play-tested with a screen.** Look, lighting and feel are unchecked; the headless self-test covers collision, the start and spawns.
