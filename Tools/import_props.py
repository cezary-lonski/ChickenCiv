import unreal, os
base = unreal.Paths.project_content_dir() + "Models/Source/"
tasks = []
def add(path, dest):
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = dest
    t.automated = True; t.save = True; t.replace_existing = True; tasks.append(t)
add(base + "W/RPG_Weapons_Texture_01.png", "/Game/Models/Weapons")
for f in sorted(os.listdir(base + "W")):
    if f.endswith(".fbx"): add(base + "W/" + f, "/Game/Models/Weapons")
add(base + "pirateship.png", "/Game/Models/Ship")
add(base + "pirateship.fbx", "/Game/Models/Ship")
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for p in unreal.EditorAssetLibrary.list_assets("/Game/Models", recursive=True):
    a = unreal.EditorAssetLibrary.load_asset(p)
    c = a.get_class().get_name()
    extra = ""
    if c in ("StaticMesh", "SkeletalMesh"):
        b = a.get_bounds(); extra = " ext=" + str(b.box_extent) + " mats=" + str([str(m.material_slot_name) for m in (a.static_materials if c=="StaticMesh" else a.materials)])
    unreal.log("ASSETX " + p + " : " + c + extra)
sk = unreal.EditorAssetLibrary.load_asset("/Game/Models/Chicken_001_Skeleton")
unreal.log("BONES " + str([str(sk.get_bone_name(i)) for i in range(sk.get_num_bones())]) if hasattr(sk, "get_num_bones") else "NOBONEAPI")
