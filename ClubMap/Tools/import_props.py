"""Import the Meshy-generated props (FBX + PBR texture PNGs) into /Game/ClubMap/Props/<Name>/
and give each one a proper PBR material instance (M_CM_GenPBR). Run inside the editor (python-execute).
Source files: C:/Users/Sebastian Ramos/AppData/Local/Temp/nc_gen/<Folder>/ (kept from the earlier prototype).
"""
import unreal

SRC = "C:/Users/Sebastian Ramos/AppData/Local/Temp/nc_gen"
SRC_1K = SRC + "_1k"   # same layout, textures downscaled to 1024 px (keeps the repo / LFS small)
DST = "/Game/ClubMap/Props"
MASTER = "/Game/ClubMap/Materials/M_CM_GenPBR"
# folder on disk -> asset name
PROPS = {"stool": "BarStool", "DJController": "DJController", "SpeakerStack": "SpeakerStack",
         "MovingHead": "MovingHead", "VIPBooth": "VIPBooth", "CocktailTable": "CocktailTable",
         "MirrorBall": "MirrorBall", "BeerTap": "BeerTap", "ChampagneBucket": "ChampagneBucket",
         "WhiskeyBottle": "WhiskeyBottle", "PendantLight": "PendantLight", "Chandelier": "Chandelier",
         "WallSpeaker": "WallSpeaker", "CrowdBarrier": "CrowdBarrier", "FireExitDoor": "FireExitDoor",
         "SecurityCam": "SecurityCam",
         "LoungeArmchair": "LoungeArmchair", "CoffeeTable": "CoffeeTable", "Hookah": "Hookah",
         "LiquorBottles": "LiquorBottles", 
         "BouncerPodium": "BouncerPodium", "Ashtray": "Ashtray"}

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def ensure_master():
    if EAL.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    m = AT.create_asset("M_CM_GenPBR", "/Game/ClubMap/Materials", unreal.Material, unreal.MaterialFactoryNew())
    specs = [("BaseColorMap", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialProperty.MP_BASE_COLOR, "RGB", -600, -300),
             ("NormalMap", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialProperty.MP_NORMAL, "RGB", -600, 0),
             ("RoughnessMap", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_ROUGHNESS, "R", -600, 300),
             ("MetallicMap", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_METALLIC, "R", -600, 600)]
    # default textures must match each sampler type or the material fails to compile
    defaults = {"NormalMap": "/Engine/EngineMaterials/DefaultNormal",
                "RoughnessMap": "/Game/ClubMap/Props/BarStool/T_Gen_BarStool_roughness",
                "MetallicMap": "/Game/ClubMap/Props/BarStool/T_Gen_BarStool_metallic"}
    for (name, st, prop, out, x, y) in specs:
        e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        e.set_editor_property("parameter_name", name)
        if name in defaults and EAL.does_asset_exist(defaults[name]):
            e.set_editor_property("texture", unreal.load_asset(defaults[name]))
        e.set_editor_property("sampler_type", st)
        MEL.connect_material_property(e, out, prop)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def import_files(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    AT.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        out += [str(p) for p in t.get_editor_property("imported_object_paths")]
    return out


FBX_EXTRAS = ["Image_0", "Image_1", "Image_2", "texture_0_metallic", "texture_0_roughness"]


def cleanup_fbx_extras(dest, name, mi):
    """The FBX import also brings in its embedded textures + a default material (~12 MB per prop, unused).
    Merge Material_001 into our MI, resave the mesh, then delete the extras and the leftover redirector."""
    m1 = dest + "/Material_001"
    if EAL.does_asset_exist(m1):
        EAL.consolidate_assets(mi, [unreal.load_asset(m1)])
    for a in FBX_EXTRAS:
        if EAL.does_asset_exist("%s/%s" % (dest, a)):
            EAL.delete_asset("%s/%s" % (dest, a))
    EAL.save_asset("%s/SM_Gen_%s" % (dest, name), only_if_is_dirty=False)
    if EAL.does_asset_exist(m1):
        EAL.delete_asset(m1)


def import_prop(folder, name):
    import os
    dest = "%s/%s" % (DST, name)
    base = "%s/%s" % (SRC, folder)
    tbase = "%s/%s" % (SRC_1K, folder)
    if not os.path.isdir(tbase):
        tbase = base
    mesh_paths = import_files(["%s/SM_Gen_%s.fbx" % (base, name)], dest)
    tex = {}
    for kind, param in (("base_color", "BaseColorMap"), ("normal", "NormalMap"), ("roughness", "RoughnessMap"), ("metallic", "MetallicMap")):
        p = import_files(["%s/T_Gen_%s_%s.png" % (tbase, name, kind)], dest)
        if p:
            t = unreal.load_asset(p[0].split(".")[0])
            if kind == "normal":
                t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                t.set_editor_property("srgb", False)
            elif kind in ("roughness", "metallic"):
                t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
                t.set_editor_property("srgb", False)
            EAL.save_loaded_asset(t)
            tex[param] = t
    mi = AT.create_asset("MI_%s" % name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, ensure_master())
    for param, t in tex.items():
        MEL.set_material_instance_texture_parameter_value(mi, param, t)
    EAL.save_loaded_asset(mi)
    sm = None
    for p in mesh_paths:
        a = unreal.load_asset(p.split(".")[0])
        if isinstance(a, unreal.StaticMesh):
            sm = a
    if sm:
        mats = sm.get_editor_property("static_materials")
        for m in mats:
            m.set_editor_property("material_interface", mi)
        sm.set_editor_property("static_materials", mats)
        EAL.save_loaded_asset(sm)
        cleanup_fbx_extras(dest, name, mi)
    return {"mesh": sm.get_path_name() if sm else None, "textures": len(tex), "slots": len(mats) if sm else 0}


def run(names=None):
    ensure_master()
    res = {}
    for folder, name in PROPS.items():
        if names and name not in names:
            continue
        try:
            res[name] = import_prop(folder, name)
        except Exception as e:
            res[name] = "ERROR: %s" % e
    return res
