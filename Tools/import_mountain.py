import unreal
base = unreal.Paths.project_content_dir() + "Models/Source/M/"
eal = unreal.EditorAssetLibrary
log = []
try:
    t = unreal.AssetImportTask(); t.filename = base + "Mountain_Diffuse.png"; t.destination_path = "/Game/Models/Mountain"; t.destination_name = "T_Mountain"
    t.automated = True; t.save = True; t.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True); ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False); ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    m = unreal.AssetImportTask(); m.filename = base + "mountain2.fbx"; m.destination_path = "/Game/Models/Mountain"; m.destination_name = "SM_Mountain"
    m.automated = True; m.save = True; m.replace_existing = True; m.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([m])
    ip = "/Game/Models/MI_Mountain"
    if eal.does_asset_exist(ip): eal.delete_asset(ip)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_Mountain", "/Game/Models", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", eal.load_asset("/Game/Models/M_Textured"))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseTex", eal.load_asset("/Game/Models/Mountain/T_Mountain"))
    eal.save_asset(ip)
    for p in eal.list_assets("/Game/Models/Mountain"):
        a = eal.load_asset(p); s = p + " : " + a.get_class().get_name()
        if a.get_class().get_name() == "StaticMesh":
            b = a.get_bounds(); s += " ext=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) tris=%s" % (b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, a.get_num_triangles(0) if hasattr(a, "get_num_triangles") else "?")
        log.append(s)
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/mountain.txt", "w", encoding="utf-8").write("\n".join(log))
