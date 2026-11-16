# Main menu, starting sequence, pause and results

The game opens on the club level (`/Game/GJ/Lvl_Club`, the default map) with a title screen. **Play** runs a short, skippable intro that ends in the player's eyes, and then the run starts. **Escape** (or **P**) pauses. When the player dies, the run is over and a results screen offers a restart.

## Flow

| Phase | What happens |
|---|---|
| **Menu** | The camera circles the dance hall slowly. The club's music plays, muffled behind a low-pass. A panel on the left holds the title, the tagline and **Play / Settings / Quit**. No enemies spawn, and the HUD and the template's widgets are hidden. |
| **Intro** | About 10 s. The camera descends from the drains' junction, up the T1 tunnel and through the club threshold under its neon sign, into the Entry. It ends exactly at the player's eyes and blends into their view. The title card and tagline fade in and out over it, and the music opens up. **Any key or click skips it.** |
| **Playing** | The player has control, the HUD comes back, and the encounter starts: the first enemy arrives one spawn interval later. |
| **Paused** (Escape or P) | The game freezes, agents included. A dimmed screen shows **PAUSED**, the run so far (time, kills, wave) and **Resume / Settings / Restart / Main menu / Quit**. Escape or P again resumes. The music keeps playing, muffled. In the editor's play-in-editor, Escape stops the session, so use **P** there. |
| **Dead** | The player's death view plays for 2.5 s (the HUD's red fade), then the **results screen**: **YOU DIED**, time survived, kills and wave reached, the best time and most kills (or **NEW BEST TIME**), and **Restart / Main menu / Quit**. Nothing spawns any more, and the player doesn't respawn. |

The procedural tunnels at the club's sockets (MAPGEN.md §5b) are built when the level loads, so they are already there under the menu.

- **Restart** reloads the level with `?GJStart=play`: a new run starts at once, with new tunnel seeds and no menu or intro.
- **Main menu** reloads the level to the title screen.
- **The run's time** is game time from the start of play, so time spent paused doesn't count. **Kills** count every enemy that dies during the run. **Wave** is the encounter's current wave.
- **Records** (best time, most kills) are saved with the settings in `GameUserSettings.ini`.

## Settings

The menu's **Settings** panel has master volume, music volume, mouse sensitivity (0.1x to 3x) and fullscreen.
- The same panel opens from the main menu and the pause menu.
- Changes apply live, and are saved to `GameUserSettings.ini` (section `[GJ.UserSettings]`) on **Back**, **Play**, **Resume** or **Quit**.
- Master volume goes to the audio device, music volume to `gj.MusicVolume`, and sensitivity scales the player's look input (`AGJPlayerCharacter::DoAim`). Fullscreen uses borderless windowed fullscreen and is ignored in the editor.

## Where it lives

- **`UGJRunSubsystem`** (`Source/GameJam/GJ/GJRunSubsystem.*`) is the run state (Waiting, Menu, Intro, Playing, Dead), pause, the run's stats, the Slate menus (`SGJMenu`: title, pause and results screens share it) and title card, and the camera. Other systems ask it:
  - `IsPlaying()`: the encounter spawns only during the run (not before it, not after death);
  - `HidesGameHud()`: `AGJHUD` draws nothing under the title screen, the intro and the results screen;
  - `EndsRunOnDeath()`: with the front end, `AGJHUD` drops its "respawning" text, and the run subsystem cancels the template's respawn (`AGJPlayerCharacter::CancelRespawn`).
- **Music** stems are UI sounds, so they keep playing while the game is paused and stay on the beat clock, which never pauses.
- **`UGJFrontEndSettings`** (Project Settings > Game > **GJ Front End**) holds the title, the tagline, the intro length and blend, the menu orbit (time per lap, radius, height) and the menu's music cutoff.
- **`FGJUserSettings`** (`GJUserSettings.*`) holds the player's settings.
- **Markers in `Lvl_Club`** (folder `FrontEnd`, made by `Tools/MapGen/make_club_level.py`):
  - `FrontEnd_MenuOrbit`, a TargetPoint tagged `GJMenuOrbit`: the centre the menu camera circles (the dance hall);
  - `FrontEnd_Intro_0..4`, TargetPoints tagged `GJIntro` plus their order number (`"0"`, `"1"`, ...): the intro's path. Each marker's rotation is where the camera looks. Move them in the editor to reshape the shot; the path always ends at the player's eyes.

A level shows the front end only if it has a `GJMenuOrbit` marker. Other levels (`Lvl_MapGen`, the template map) start the run at once and keep the template's respawn, with no pause menu or results screen, as before. `-nomenu` on the command line skips it everywhere.

## Console

- `gj.Play`: Play (queued if the menu isn't up yet, so it works in `-ExecCmds`)
- `gj.SkipIntro`: skip the intro (or the menu) and start the run
- `gj.RunInfo`: print the phase
- `gj.Pause`: pause or resume (as Escape / P)
- `gj.Restart`, `gj.MainMenu`: as the buttons
- `gj.KillPlayer`: lethal damage to the player, to test the results screen
- `gj.DelayedExec <seconds> <command>`: run a console command later, for scripted captures. For example, `-ExecCmds="gj.DelayedExec 25 shot showui,gj.DelayedExec 30 gj.Play,gj.DelayedExec 35 shot showui,gj.DelayedExec 45 quit"` writes screenshots, including the UI, to `Saved/Screenshots/`. The first windowed launch on a machine spends about 25 minutes compiling shaders.

Headless check:
```bash
~/Downloads/unreal_engine/Engine/Binaries/Linux/UnrealEditor $PWD/GameJam/GameJam.uproject /Game/GJ/Lvl_Club -game -nullrhi -nosound \
  -unattended -nosplash -stdout -ExecCmds="gj.Play,gj.MapSelfTest 18 quit"
```
`Saved/Logs/GameJam.log` should show `Run: EGJRunPhase::Menu`, then `Intro`, then `Playing` 10 s later, then enemies arriving. For the rest of the flow: `-ExecCmds="gj.Enemies 0,gj.SkipIntro,gj.DelayedExec 5 gj.Pause,gj.DelayedExec 8 gj.Pause,gj.DelayedExec 10 gj.KillPlayer,gj.DelayedExec 16 gj.Restart,gj.DelayedExec 24 quit"` logs `paused`, `resumed`, `died after ... s`, `results shown`, `reloading the level (GJStart=play)` and `Playing` again.

## Open

- **Title.** The game is **Night Clubs Descent** (`UGJFrontEndSettings::Title`, also the window title through `ProjectDisplayedTitle` in DefaultGame.ini). The tagline paraphrases the brief, and no lore is added.
- **Kills are untested in-game**: the scripted runs didn't kill any enemies. They count `AEnemyBean::OnAnyEnemyDied` during the run.
- **Run rules are open** (PLAN §13): a run ends at the first death, with no lives or checkpoints.
- **Look.** It uses Slate's default font and flat colour panels in the club palette. A designed UMG menu can replace the Slate widgets without changing the run state.
