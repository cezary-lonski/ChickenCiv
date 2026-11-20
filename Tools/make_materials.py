import unreal
log = []
try:
    at = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    path = "/Game/Models/M_ChickenTint"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = at.create_asset("M_ChickenTint", "/Game/Models", unreal.Material, unreal.MaterialFactoryNew())
    def ex(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)
    tex = ex(unreal.MaterialExpressionTextureSampleParameter2D, -1000, 0)
    tex.set_editor_property("parameter_name", "BaseTex")
    tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Models/Texture_1"))
    desat = ex(unreal.MaterialExpressionDesaturation, -750, 150)
    dist = ex(unreal.MaterialExpressionDistance, -550, 150)
    k = ex(unreal.MaterialExpressionScalarParameter, -550, 300)
    k.set_editor_property("parameter_name", "ChromaK"); k.set_editor_property("default_value", 4.0)
    mulk = ex(unreal.MaterialExpressionMultiply, -350, 200)
    sat = ex(unreal.MaterialExpressionSaturate, -150, 200)
    one = ex(unreal.MaterialExpressionOneMinus, -250, 200)
    tint = ex(unreal.MaterialExpressionVectorParameter, -800, -250)
    tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mult = ex(unreal.MaterialExpressionMultiply, -500, -100)
    lerp = ex(unreal.MaterialExpressionLinearInterpolate, 100, 0)
    rough = ex(unreal.MaterialExpressionConstant, 100, 250); rough.set_editor_property("r", 0.85)
    mel.connect_material_expressions(tex, "RGB", desat, "")
    mel.connect_material_expressions(tex, "RGB", dist, "A")
    mel.connect_material_expressions(desat, "", dist, "B")
    mel.connect_material_expressions(dist, "", mulk, "A")
    mel.connect_material_expressions(k, "", mulk, "B")
    mel.connect_material_expressions(mulk, "", sat, "")
    mel.connect_material_expressions(sat, "", one, "")
    mel.connect_material_expressions(tex, "RGB", mult, "A")
    mel.connect_material_expressions(tint, "", mult, "B")
    mel.connect_material_expressions(tex, "RGB", lerp, "A")
    mel.connect_material_expressions(mult, "", lerp, "B")
    mel.connect_material_expressions(one, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    log.append("MATERIAL OK")
except Exception as e:
    log.append("MATERIAL ERR " + repr(e))
try:
    t = unreal.EditorAssetLibrary.load_asset("/Game/Models/Ship/pirateship")
    log.append("SHIPTEX %s x %s" % (t.blueprint_get_size_x(), t.blueprint_get_size_y()))
except Exception as e:
    log.append("SHIPTEX ERR " + repr(e))
try:
    # ponowny import statku z wlasnymi opcjami (log bledow)
    task = unreal.AssetImportTask()
    task.filename = unreal.Paths.project_content_dir() + "Models/Source/pirateship.fbx"
    task.destination_path = "/Game/Models/Ship"; task.automated = True; task.save = True; task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log.append("SHIPIMPORT " + str([str(p) for p in task.imported_object_paths]))
    for p in unreal.EditorAssetLibrary.list_assets("/Game/Models/Ship", recursive=True):
        a = unreal.EditorAssetLibrary.load_asset(p)
        s = p + " : " + a.get_class().get_name()
        if a.get_class().get_name() == "StaticMesh":
            s += " ext=%s mats=%s" % (a.get_bounds().box_extent, [str(m.material_slot_name) for m in a.static_materials])
        log.append("SHIPASSET " + s)
except Exception as e:
    log.append("SHIP ERR " + repr(e))
open("D:/Temp/mat.txt", "w", encoding="utf-8").write("\n".join(log))
