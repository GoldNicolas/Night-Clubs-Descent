# Builds /Game/GJ/Lvl_Club: the club map (Sebastian's /Game/ClubMap/Maps/Lvl_ClubMap, kept unchanged) as an always-loaded
# sublevel, plus one procedural tunnel map per location socket (MAPGEN.md §5b) and the front end's camera markers (FRONTEND.md). Runs inside Unreal's Python:
#   UnrealEditor <abs>/GameJam.uproject -run=pythonscript -script=<abs>/Tools/MapGen/make_club_level.py \
#       -EnablePlugins=PythonScriptPlugin -unattended -nullrhi -nosplash
# Re-running replaces Lvl_Club. A newer club map only needs /Game/ClubMap copied over again; this level picks it up as is.
import unreal

LEVEL = "/Game/GJ/Lvl_Club"
CLUB = "/Game/ClubMap/Maps/Lvl_ClubMap"
MAT = "/Game/ClubMap/Materials/"
# The sockets the club map has (TargetPoints tagged LocationSocket + direction), with a rough spot for each generator in the
# editor; each moves onto its socket when play starts.
SOCKETS = {"West": (-2800, -3800), "East": (5200, -3800), "South": (1800, -6000)}
LEVEL_PRESET = 2.0  # tunnels L2: about 56 x 56 m per socket, before shrinking to fit


def mat(name):
    m = unreal.load_asset(MAT + name)
    if not m:
        raise RuntimeError("missing material " + MAT + name)
    return m


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
streaming = unreal.EditorLevelUtils.add_level_to_world(world, CLUB, unreal.LevelStreamingAlwaysLoaded)
if not streaming:
    raise RuntimeError("could not add " + CLUB + " as a sublevel")
# New actors go into the persistent level (ours), never into the club map.
if not les.set_current_level_by_name(LEVEL.split("/")[-1]):
    raise RuntimeError("could not make the persistent level current")

for direction, (x, y) in SOCKETS.items():
    gen = eas.spawn_actor_from_class(unreal.GJMapGenerator, unreal.Vector(x, y, 0))
    if not gen.get_path_name().startswith(LEVEL + "."):
        raise RuntimeError("generator landed in the wrong level: " + gen.get_path_name())
    gen.set_actor_label("MapGen_" + direction)
    gen.set_folder_path("MapGen")
    gen.set_editor_property("socket", direction)
    gen.set_editor_property("family", unreal.GJMapFamily.TUNNELS)
    gen.set_editor_property("level", LEVEL_PRESET)
    gen.set_editor_property("roof", True)
    gen.set_editor_property("lamps", True)
    gen.set_editor_property("floor_material", mat("MI_CM_Floor_Wet"))
    gen.set_editor_property("wall_material", mat("MI_CM_Concrete_Wet"))
    gen.set_editor_property("roof_material", mat("MI_CM_Concrete_Wet"))
    gen.set_editor_property("low_cover_material", mat("MI_CM_Wall_Concrete"))
    gen.set_editor_property("high_cover_material", mat("MI_CM_Wall_Concrete"))
    gen.set_editor_property("lamp_material", mat("MI_CM_Sodium"))

# Front end (FRONTEND.md): the menu camera circles the dance hall; the intro descends from the drains' junction, up T1 and
# through the club threshold, then into the player's eyes at the Entry (added at run time). Club grid: +Y is north.
orbit = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(1600, 1600, 0))
orbit.set_actor_label("FrontEnd_MenuOrbit")
orbit.set_folder_path("FrontEnd")
orbit.set_editor_property("tags", [unreal.Name("GJMenuOrbit")])
INTRO = [  # (x, y, z, yaw, pitch)
    (1800, -4250, 160, 90, 4),    # drains junction, looking up the tunnel toward the club
    (1800, -3500, 175, 90, 2),
    (1800, -2500, 190, 90, 0),    # T1
    (1720, -1850, 210, 92, 6),    # club threshold: the neon sign
    (1600, -1350, 190, 90, -2),   # into the Entry
]
for i, (x, y, z, yaw, pitch) in enumerate(INTRO):
    m = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
    m.set_actor_label("FrontEnd_Intro_%d" % i)
    m.set_folder_path("FrontEnd")
    m.set_editor_property("tags", [unreal.Name("GJIntro"), unreal.Name(str(i))])

les.save_current_level()
unreal.log("make_club_level: saved %s with %s as a sublevel and %d socket maps" % (LEVEL, CLUB, len(SOCKETS)))
