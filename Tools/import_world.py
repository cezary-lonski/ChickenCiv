import unreal, os
base = unreal.Paths.project_content_dir() + "Models/Source/"
eal = unreal.EditorAssetLibrary
log = []
def tex(path, dest, name=None):
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = dest
    if name: t.destination_name = name
    t.automated = True; t.save = True; t.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
def mesh(path, dest, name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = dest; t.destination_name = name
    t.automated = True; t.save = True; t.replace_existing = True; t.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
try:
    tex(base + "V/rpgpp_lt_tex_a.tga", "/Game/Models/Village", "T_Village")
    tex(base + "C/Atlas_color.png", "/Game/Models/Castle", "T_Castle")
    tex(base + "A/StarSparrow_White.png", "/Game/Models/Air", "T_SparrowWhite")
    tex(base + "A/StarSparrow_Grey.png", "/Game/Models/Air", "T_SparrowGrey")
    for f in sorted(os.listdir(base + "V")):
        if f.endswith(".fbx"): mesh(base + "V/" + f, "/Game/Models/Village", "SM_" + f.replace("rpgpp_lt_", "").replace(".fbx", ""))
    mesh(base + "C/CastleRuins.fbx", "/Game/Models/Castle", "SM_CastleRuins")
    for f in sorted(os.listdir(base + "A")):
        if f.endswith(".fbx"): mesh(base + "A/" + f, "/Game/Models/Air", "SM_" + f.replace(".fbx", ""))
    # material tekstury (bez barwienia) + instancje
    at = unreal.AssetToolsHelpers.get_asset_tools(); mel = unreal.MaterialEditingLibrary
    mp = "/Game/Models/M_Textured"
    if eal.does_asset_exist(mp): eal.delete_asset(mp)
    m = at.create_asset("M_Textured", "/Game/Models", unreal.Material, unreal.MaterialFactoryNew())
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    ts.set_editor_property("parameter_name", "BaseTex"); ts.set_editor_property("texture", eal.load_asset("/Game/Models/Village/T_Village"))
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250); rough.set_editor_property("r", 0.9)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_asset(mp)
    for name, texpath, parent in (("MI_Village", "/Game/Models/Village/T_Village", mp), ("MI_Castle", "/Game/Models/Castle/T_Castle", mp)):
        ip = "/Game/Models/" + name
        if eal.does_asset_exist(ip): eal.delete_asset(ip)
        mi = at.create_asset(name, "/Game/Models", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", eal.load_asset(parent))
        mel.set_material_instance_texture_parameter_value(mi, "BaseTex", eal.load_asset(texpath))
        eal.save_asset(ip)
    for p in eal.list_assets("/Game/Models/Village", recursive=True) + eal.list_assets("/Game/Models/Castle", recursive=True) + eal.list_assets("/Game/Models/Air", recursive=True):
        a = eal.load_asset(p); c = a.get_class().get_name(); s = p + " : " + c
        if c == "StaticMesh":
            b = a.get_bounds(); s += " ext=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) slots=%d" % (b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, len(a.static_materials))
        log.append(s)
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/world.txt", "w", encoding="utf-8").write("\n".join(log))
