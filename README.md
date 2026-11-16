# Night Clubs Descent

*Beneath Las Vegas, the party never ends.*

A first-person run-and-gun set in an underground club. The enemies aren't scripted: they are reinforcement-learning agents trained in a separate project (FPS-Agent) and run in the game through Unreal's Learning Agents plugin. Made in Unreal Engine 5.7 for a game jam.

## Play (Windows)

1. Download **NightClubsDescent-Win64.zip** from [Releases](https://github.com/GoldNicolas/Night-Clubs-Descent/releases/latest).
2. Unzip it anywhere, keeping the folder together.
3. Run **GameJam.exe**.

You need Windows 10 or 11 (64-bit) and a DirectX 12 graphics card. The club uses Lumen lighting, so 8 GB of video memory is recommended.

## A run

- The title screen circles the club's dance hall. **Play** starts a short intro (any key skips it) that takes you from the storm drains into the club.
- Enemies come in waves, from the club's rooms and from the storm-drain tunnels around it. The tunnels are generated, and change every run.
- The music follows the fight, and the crowd runs from gunfire.
- You have one life. When you die, the results screen shows how long you survived, your kills, the wave you reached and your best records.

## Controls

| Action | Key |
|---|---|
| Move | W A S D |
| Look | Mouse |
| Shoot | Left mouse button |
| Reload | R |
| Jump | Space |
| Crouch | Left Ctrl or C |
| Walk (quiet footsteps) | Hold Left Shift |
| Pause | Esc or P |

The main menu and the pause menu have the settings: master volume, music volume, mouse sensitivity and fullscreen.

## Build from source

You need Unreal Engine 5.7, Visual Studio 2022 with the workloads in `GameJam/.vsconfig`, and Git LFS (the `.uasset` and `.umap` files are stored in LFS).

```
git lfs install
git clone https://github.com/GoldNicolas/Night-Clubs-Descent.git
```

- **Editor:** open `GameJam/GameJam.uproject`. The C++ compiles on the first open. The game starts on `/Game/GJ/Lvl_Club`.
- **Packaged game:** close the editor, then run `BuildGame.bat` from the repo root.
  - `BuildGame.bat` makes a Development build in `Builds\GameJam-Development\Windows\`. It checks that everything the editor shows made it into the build, then smoke-tests the packaged game.
  - `BuildGame.bat -Config Shipping -Zip` makes the release build, zipped to `Builds\GameJam-Shipping.zip`.

## Branches

- `development` is where everyone's work comes together.
- `main` follows `development`.

## Docs

| File | What it covers |
|---|---|
| [PLAN.md](PLAN.md) | Game design, architecture, milestones and open decisions |
| [FPS-Agent-Constraints.md](FPS-Agent-Constraints.md) + [v1.2 addendum](FPS-Agent-Constraints-v1.2-addendum.md) | The contract with FPS-Agent: what the enemies observe and do, their body, rifle and senses |
| [FRONTEND.md](FRONTEND.md) | Main menu, intro, pause and results screens |
| [MAPGEN.md](MAPGEN.md) | The procedural map generator and the club's tunnels |
| [MUSIC.md](MUSIC.md) | The adaptive club music |
| [docs/MAP_DESIGN.md](docs/MAP_DESIGN.md) | The club map's design |
| [REPO-WARNINGS.md](REPO-WARNINGS.md) | Repo pitfalls |
| [CLAUDE.md](CLAUDE.md) | Working notes for Claude Code |
