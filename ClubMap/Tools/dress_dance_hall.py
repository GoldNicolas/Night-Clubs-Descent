"""Dress the Main Dance Hall with the imported props. Run with include=[cm_kit.py, this file], then dress().
Collision per the AI contract:
  cover (speakers, booths): keep the simple box placeholders for collision, hide their render
  clutter (stools, cocktail tables): block pawns only (PhysicsBody object type, ignores Weapon / visibility)
  decor (hanging, wall-mounted, tabletop): no collision
"""
import unreal

P = "/Game/ClubMap/Props/%s/SM_Gen_%s"
F = "Rooms/DanceHall/Props"
_EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def prop(name, loc, yaw=0.0, s=1.0, sxy=None, sz=None, roll=0.0, coll="decor", label=None):
    sx = sxy if sxy else s
    a = _EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(pitch=0, yaw=yaw, roll=roll))
    a.set_actor_label(label or name)
    a.set_folder_path(F)
    a.set_actor_scale3d(unreal.Vector(sx, sx, sz if sz else s))
    c = a.static_mesh_component
    c.set_static_mesh(unreal.load_asset(P % (name, name)))
    if coll == "pawn":
        c.set_collision_profile_name("Custom")
        c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        c.set_collision_object_type(unreal.CollisionChannel.ECC_PHYSICS_BODY)
        c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponseType.ECR_BLOCK)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_AGENT_PAWN, unreal.CollisionResponseType.ECR_BLOCK)
    else:
        c.set_collision_profile_name("NoCollision")
    return a


def hide_placeholders(labels):
    n = 0
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() in labels:
            c = a.static_mesh_component
            c.set_editor_property("visible", False)
            c.set_editor_property("cast_shadow", False)
            n += 1
    return n


def dress():
    r = {}
    # remove the floating speaker neon (it sat on the placeholder box face)
    for a in _EAS.get_all_level_actors():
        if a.get_actor_label() == "Speaker_Neon":
            _EAS.destroy_actor(a)
    r["hidden"] = hide_placeholders({"Speaker_Stack", "VIP_Booth_Seat", "VIP_Booth_Back", "Booth_Seat", "Booth_Back"})

    # DJ stage
    prop("DJController", (1600, 2810, 165 + 12 * 0.42), yaw=0, s=0.42)
    for x in (750, 2450):
        prop("SpeakerStack", (x, 3000, 95 * 1.3), yaw=180, s=1.3)            # grille faces the room (-Y)
        light("spot", "Speaker_Accent", "Lighting/DanceHall", (x, 2600, 580), (0.15, 0.75, 1.0), 400, 900,
              rot=(-60, 90, 0), outer_cone_angle=25.0, inner_cone_angle=10.0)
    # booths: VIP (opening faces -X), entrance (opening faces +Y)
    for y in (325, 795):
        prop("VIPBooth", (3072, y, 45 + 54 * 1.1), yaw=90, sxy=1.25, sz=1.1)
        prop("ChampagneBucket", (2860, y, 100 + 78 * 0.26), s=0.26)
    for x in (480, 870, 2350):
        prop("VIPBooth", (x, 128, 54 * 1.1), yaw=0, sxy=1.25, sz=1.1)
    # dance floor: mirror ball + moving heads hanging from the ceiling
    prop("MirrorBall", (1600, 1600, 470), s=0.63)
    for (x, y, yaw) in ((1000, 1000, 45), (2200, 1000, 135), (1000, 2200, -45), (2200, 2200, -135)):
        prop("MovingHead", (x, y, 600 - 20 - 95 * 0.32), yaw=yaw, s=0.32, roll=180)
    # bar
    prop("BeerTap", (310, 1100, 120 + 95 * 0.24), yaw=90, s=0.24)
    for y in (650, 1550):
        prop("WhiskeyBottle", (300, y, 120 + 95 * 0.16), yaw=y % 90, s=0.16)
    for z in (190, 260):
        for i, y in enumerate(range(520, 1720, 150)):
            prop("WhiskeyBottle", (45, y, z + 95 * 0.12), yaw=(i * 37) % 360, s=0.12)
    for i, y in enumerate(range(500, 1800, 200)):
        prop("BarStool", (460, y, 95 * 0.42), yaw=(i * 41) % 360, s=0.42, coll="pawn")
    for y in (600, 1100, 1600):
        prop("PendantLight", (310, y, 600 - 20 - 95 * 0.38), s=0.38)
        light("point", "Bar_Pendant", "Lighting/DanceHall", (310, y, 470), (1, 0.6, 0.25), 25, 500)
    # VIP chandelier
    prop("Chandelier", (2890, 560, 600 - 20 - 95 * 0.55), s=0.55)
    # cocktail tables on the open floor (pawn-blocking clutter)
    for (x, y) in ((950, 550), (2250, 550), (2850, 2350), (2850, 2900)):
        prop("CocktailTable", (x, y, 94 * 0.58), sxy=0.4, sz=0.58, coll="pawn")
    # wall speakers (front faces the room) and security cameras (lens faces the room)
    for (x, y, yaw) in ((3158, 700, 90), (3158, 2600, 90), (42, 1100, -90), (42, 3000, -90), (600, 42, 0), (2600, 42, 0)):
        prop("WallSpeaker", (x, y, 430), yaw=yaw, s=0.32)
    prop("SecurityCam", (45, 45, 560), yaw=-45, s=0.16)
    prop("SecurityCam", (3155, 45, 560), yaw=45, s=0.16)
    r["props"] = len([a for a in _EAS.get_all_level_actors() if str(a.get_folder_path()) == F])
    return r
