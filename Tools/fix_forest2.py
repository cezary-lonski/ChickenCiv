import unreal
eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = []
try:
    mp = "/Game/Models/M_TexMasked"
    if eal.does_asset_exist(mp): eal.delete_asset(mp)
    m = at.create_asset("M_TexMasked", "/Game/Models", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    ts.set_editor_property("parameter_name", "BaseTex")
    ts.set_editor_property("texture", eal.load_asset("/Game/Models/Village/T_Village"))
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250); rough.set_editor_property("r", 0.95)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_asset(mp)
    cache = {}
    n = 0
    for p in eal.list_assets("/Game/Models/Forest/scene/StaticMeshes"):
        sm = eal.load_asset(p)
        old = sm.static_materials[0].material_interface
        if old is None: continue
        name = old.get_name()
        if name not in cache:
            tex = None
            for v in old.texture_parameter_values:
                if str(v.parameter_info.name) == "BaseColorTexture": tex = v.parameter_value
            ip = "/Game/Models/Forest/MI_TM_" + name
            if eal.does_asset_exist(ip): eal.delete_asset(ip)
            mi = at.create_asset("MI_TM_" + name, "/Game/Models/Forest", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property("parent", eal.load_asset(mp))
            if tex: mel.set_material_instance_texture_parameter_value(mi, "BaseTex", tex)
            eal.save_asset(ip)
            cache[name] = mi
        sm.set_material(0, cache[name])
        eal.save_asset(p)
        n += 1
    log.append("meshes updated %d, materials %d" % (n, len(cache)))
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/forest2.txt", "w").write("\n".join(log))
