import unreal
log = []
try:
    eal = unreal.EditorAssetLibrary
    for p in eal.list_assets("/Game/Models/Ship", recursive=True):
        if not p.startswith("/Game/Models/Ship/pirateship.pirateship"):
            eal.delete_asset(p)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.filename = unreal.Paths.project_content_dir() + "Models/Source/pirateship.fbx"
    task.destination_path = "/Game/Models/Ship"; task.destination_name = "SM_PirateShip"
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log.append("SHIPIMPORT " + str([str(p) for p in task.imported_object_paths]))
    for p in eal.list_assets("/Game/Models/Ship", recursive=True):
        a = eal.load_asset(p)
        s = p + " : " + a.get_class().get_name()
        if a.get_class().get_name() == "StaticMesh":
            s += " ext=%s mats=%s" % (a.get_bounds().box_extent, [str(m.material_slot_name) for m in a.static_materials])
        log.append("SHIPASSET " + s)
except Exception as e:
    log.append("SHIP ERR " + repr(e))
open("D:/Temp/ship.txt", "w", encoding="utf-8").write("\n".join(log))
