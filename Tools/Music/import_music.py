# Imports the club music stems into /Game/GJ/Audio/Music (MUSIC.md). Runs inside Unreal's Python:
#   GJ_MUSIC_DIR=<dir with MUS_Club_*.wav> UnrealEditor <abs>/GameJam.uproject -run=pythonscript \
#       -script=<abs>/Tools/Music/import_music.py -EnablePlugins=PythonScriptPlugin -unattended -nullrhi -nosplash
# Every stem is set up the way the music subsystem needs it:
# - Looping: stems loop forever on the shared beat clock;
# - Virtualization Mode = Play When Silent: a muted stem keeps playing, so it is still in sync when it fades back in;
# - high priority: other sounds never steal a stem's voice (a restarted stem would be out of sync).
import glob
import os
import sys

import unreal

DEST = "/Game/GJ/Audio/Music"
src = os.environ.get("GJ_MUSIC_DIR", "")
files = sorted(glob.glob(os.path.join(src, "MUS_Club_*.wav")))
if not files:
    unreal.log_error("import_music: no MUS_Club_*.wav in GJ_MUSIC_DIR=%r" % src)
    sys.exit(1)

tasks = []
for path in files:
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

ok = 0
for path in files:
    name = os.path.splitext(os.path.basename(path))[0]
    asset_path = "%s/%s" % (DEST, name)
    wave = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(wave, unreal.SoundWave):
        unreal.log_error("import_music: %s did not import" % asset_path)
        continue
    wave.set_editor_property("looping", True)
    wave.set_editor_property("virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    wave.set_editor_property("priority", 50.0)
    unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log("import_music: %s (%.2f s)" % (asset_path, wave.get_editor_property("duration")))
    ok += 1

unreal.log("import_music: %d / %d stems imported" % (ok, len(files)))
if ok != len(files):
    sys.exit(1)
