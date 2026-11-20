import unreal, os
log = []
lib = unreal.EditorStaticMeshLibrary
eal = unreal.EditorAssetLibrary
base = unreal.Paths.project_content_dir() + "Models/Source/P/"
try:
    # farma: import z materialami z FBX
    for f in sorted(os.listdir(base)):
        if not f.endswith(".fbx"): continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True); ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("import_materials", True); ui.set_editor_property("import_textures", True)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        t = unreal.AssetImportTask(); t.filename = base + f; t.destination_path = "/Game/Models/Farm"; t.destination_name = "SM_" + f.replace(".fbx", "")
        t.automated = True; t.save = True; t.replace_existing = True; t.options = ui
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    def reduce(path, pct):
        m = eal.load_asset(path)
        before = lib.get_number_verts(m, 0)
        opts = unreal.EditorScriptingMeshReductionOptions()
        opts.auto_compute_lod_screen_size = False
        s = unreal.EditorScriptingMeshReductionSettings(); s.percent_triangles = pct; s.screen_size = 1.0
        opts.reduction_settings = [s]
        lib.set_lods(m, opts)
        eal.save_asset(path)
        log.append("%s verts %d -> %d" % (path, before, lib.get_number_verts(m, 0)))
    reduce("/Game/Models/Mountain/SM_Mountain", 0.01)
    reduce("/Game/Models/Station/SM_Station", 0.08)
    for p in eal.list_assets("/Game/Models/Farm"):
        a = eal.load_asset(p); c = a.get_class().get_name(); s = p + " : " + c
        if c == "StaticMesh":
            b = a.get_bounds(); s += " ext=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) verts=%d slots=%d" % (b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, lib.get_number_verts(a, 0), len(a.static_materials))
        log.append(s)
    h = eal.load_asset("/Game/Models/Hill/hill/StaticMeshes/hill")
    log.append("hill verts %d" % lib.get_number_verts(h, 0))
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/decim.txt", "w").write("\n".join(log))
