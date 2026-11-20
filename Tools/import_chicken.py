import unreal
src = unreal.Paths.project_content_dir() + "Models/Source/"
tasks = []
for f, dest in (("Texture_1.png", "/Game/Models"), ("Chicken_001.fbx", "/Game/Models")):
    t = unreal.AssetImportTask()
    t.filename = src + f
    t.destination_path = dest
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    unreal.log("IMPORTED " + t.filename + " -> " + str([str(p) for p in t.imported_object_paths]))
for p in unreal.EditorAssetLibrary.list_assets("/Game/Models", recursive=True):
    a = unreal.EditorAssetLibrary.load_asset(p)
    unreal.log("ASSET " + p + " : " + a.get_class().get_name())
