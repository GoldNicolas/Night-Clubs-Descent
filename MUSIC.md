# Adaptive club music

The soundtrack is one original track that loops for the whole run and changes with the fight, so it doesn't wear out.

**Status:** it builds, is imported, and works in a headless run. The beat clock, the stem sync and the tier changes were checked in the log. **Nobody has listened to it yet:** the mix balance, the sound itself and the loop points were only checked numerically.

## 1. Direction

- **References (from the team):** Hotline Miami 2's "Divide", Katana ZERO's "Hit the Floor" and Gesaffelstein's "Pursuit". That means dark synth / dark techno, with gritty driven analog-style bass, four-on-the-floor drums, minor keys and a fast club tempo.
- **This track:** F minor, 125 BPM, 4/4. The progression is `Fm | Db | Eb | C | Fm | Db | Bbm | C`, two bars per chord over 16 bars. The melodies and riffs are original; nothing is taken from the references.
- PLAN.md lists "music style and acquisition" as open. The team picked this style, and the first stems are synthesized by `Tools/Music/clubsynth`. Stems made in a DAW can replace them later with no code change (§4).

## 2. Stems

There are 11 stems, each a seamless loop of whole bars. They all start on the same sample and stay locked together: at 48 kHz and 125 BPM a beat is exactly 23 040 samples long.

| Stem | Bars | What it is |
|---|---|---|
| Kick | 8 | Four-on-the-floor, with a turnaround hit at the end of the phrase |
| Hats | 8 | 16th closed hats with an open hat on the off-beat and a 32nd roll at the phrase end |
| Perc | 8 | Claps on 2 and 4, metallic "pipe" ticks, a snare build at the phrase end |
| BassA | 16 | Rolling gallop bass (3 sixteenths per beat, off the kick), driven saws |
| BassB | 16 | Meaner off-beat acid bass with a resonant sweep across each chord |
| Sub | 16 | Clean sine an octave under the bass, for calm moments and breakdowns |
| Pad | 32 | Wide detuned-saw chords; the second 16 bars are voiced higher and brighter |
| Drone | 32 | Tunnel ambience: low detuned saws with filtered air noise |
| Arp | 16 | 16th chord arpeggio with a dotted-8th delay; the filter breathes every 4 bars |
| LeadA | 16 | The hook: a long singing synth line |
| LeadB | 16 | A hard, driven chord-tone riff |

The pad, bass, sub, drone and arp have a sidechain pump baked in on every beat, so the groove pumps even while the kick is muted. Each stem is mixed to a target loudness. One shared master gain keeps the loudest possible combination (everything, with the louder of each A/B pair) under -1 dBFS, so the stems stay balanced at in-game volume 1.

## 3. How the game mixes them

The mixer is `UGJMusicSubsystem` (`Source/GameJam/GJ/`), which runs in every game/PIE world. It starts every stem on one Quartz clock and mixes by fading stems in and out. Changes are planned on beat 4 so that the fades land on the next downbeat.

| Tier | When | Plays |
|---|---|---|
| **Prowl** | No threat | Drone, pad, sub, light hats; sometimes a soft arp |
| **Hunt** | An enemy within 15 m, or visible (in front, clear line) within 40 m, in the last 6 s | Kick, hats, one bass, arp; pad and drone pulled back |
| **Fight** | The player fired or was hit in the last 7 s | Adds claps/percussion and a lead; the arp ducks under the lead |

- **Rising and falling.** The music rises at the next bar. It falls one tier at a time, only on a 4-bar line, and only after holding the tier for 12 s, so it doesn't flap.
- **Variation.** At every 16-bar phrase the bass may swap A/B (40%). The lead rests for some phrases (30%) and may swap variant; a new fight always brings the lead in. The calm tier sometimes adds the arp (35%). In Hunt and Fight, half the 32-bar cycles end in a 4-bar **breakdown**: the kick, bass and percussion cut out, the sub and pad swell, then everything drops back in on the next cycle.
- **Health.** Below 40% health the music closes in through a low-pass filter, down to 700 Hz near death and 450 Hz while dead. It opens again as health regenerates.
- **Agents.** The music is presentation only. Agents never hear it (their hearing is the player's footstep and gunshot events, contract §7.4), and nothing here reads or changes agent state.

Settings are in Project Settings > Game > **GJ Music**: stems and their roles, BPM, volume, and the intensity distances and timings.

Console commands:
- `gj.Music 0|1`
- `gj.MusicVolume <0-1>`
- `gj.MusicTier -1|0|1|2` forces a tier, for listening to each one
- `gj.MusicInfo` prints the tier, the bar, the low-pass cutoff and the level of each stem

## 4. Making and replacing stems

```
Tools/Music/build.sh /tmp/clubsynth && /tmp/clubsynth Tools/Music/out
GJ_MUSIC_DIR=$PWD/Tools/Music/out ~/Downloads/unreal_engine/Engine/Binaries/Linux/UnrealEditor $PWD/GameJam/GameJam.uproject \
  -run=pythonscript -script=$PWD/Tools/Music/import_music.py -EnablePlugins=PythonScriptPlugin -unattended -nullrhi -nosplash
```

- `clubsynth` is plain C++17 and deterministic. It takes about 15 s and writes `MUS_Club_*.wav` plus `Preview_Prowl/Hunt/Fight/Demo.wav`, mixed the way the game mixes them. **Listen to `Preview_Demo.wav` first.** `Tools/Music/out/` is git-ignored; the imported assets are in `Content/GJ/Audio/Music/`.
- `import_music.py` imports each stem with **Looping** on, **Virtualization Mode = Play When Silent** (a muted stem keeps playing, so it is still in sync when it fades back in) and **priority 50** (other sounds can't steal its voice and restart it out of sync). Running it headless logs a harmless `Decoder for AudioFormat 'BINKA' not found` ensure; the import still succeeds.
- **Stems from a DAW** can replace the synthesized ones:
  - Export at the settings' BPM, 48 kHz.
  - Make each stem a whole number of bars. 8, 16 or 32 is best, so the phrase logic lines up.
  - Make each stem a seamless loop, including reverb and delay tails: render the loop twice and keep the second pass.
  - All stems start at bar 1 together.
  - Import them with the same three settings, then set them in GJ Music.
  - A track at a different tempo or key just needs its own full set of stems and a new BPM.

## 5. Next steps

- Listen in the editor (`gj.MusicTier 0`, `1`, `2`) and adjust the per-stem `Volume` trims in GJ Music. The balance was set by loudness targets, not by ear.
- Check that the music sits under the player's footsteps and gunfire. Those are gameplay cues: walking silently matters (contract §7.4).
- Optional, from PLAN §11: kill-streak stingers, a filter sweep or riser before a breakdown, a second track for variety across runs.
- The default 32 voices: the music uses 11 at high priority, which leaves 21 for gameplay sound. Raise Max Channels if gunfire starts dropping out.
