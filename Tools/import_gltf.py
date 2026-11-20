import unreal
base = unreal.Paths.project_content_dir() + "Models/Source/"
eal = unreal.EditorAssetLibrary
log = []
def imp(path, dest):
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = dest
    t.automated = True; t.save = True; t.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    log.append("IMPORTED " + path + " -> " + str(len(t.imported_object_paths)))
try:
    imp(base + "F/scene.gltf", "/Game/Models/Forest")
    imp(base + "H/hill.glb", "/Game/Models/Hill")
    for d in ("/Game/Models/Forest", "/Game/Models/Hill"):
        for p in eal.list_assets(d, recursive=True):
            a = eal.load_asset(p); c = a.get_class().get_name(); s = p + " : " + c
            if c == "StaticMesh":
                b = a.get_bounds(); s += " ext=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) slots=%d" % (b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, len(a.static_materials))
            log.append(s)
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/gltf.txt", "w", encoding="utf-8").write("\n".join(log))
