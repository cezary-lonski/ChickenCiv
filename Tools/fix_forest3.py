import unreal
eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = []
try:
    mp = "/Game/Models/M_TexMasked"
    m = eal.load_asset(mp)
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0)
    ts.set_editor_property("parameter_name", "BaseTex")
    ts.set_editor_property("texture", eal.load_asset("/Game/Models/Village/T_Village"))
    des = mel.create_material_expression(m, unreal.MaterialExpressionDesaturation, -550, 200)
    k = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -550, 330); k.set_editor_property("r", 40.0)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 250)
    sat = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -150, 250)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 450); rough.set_editor_property("r", 0.95)
    mel.connect_material_expressions(ts, "RGB", des, "")
    mel.connect_material_expressions(des, "", mul, "A")
    mel.connect_material_expressions(k, "", mul, "B")
    mel.connect_material_expressions(mul, "", sat, "")
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_asset(mp)
    log.append("ok")
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/forest3.txt", "w").write("\n".join(log))
