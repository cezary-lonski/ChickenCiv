import unreal
eal = unreal.EditorAssetLibrary
log = []
try:
    for p in eal.list_assets("/Game/Models/Forest/scene/Materials"):
        mi = eal.load_asset(p)
        bpo = mi.get_editor_property("base_property_overrides")
        bpo.set_editor_property("override_two_sided", True)
        bpo.set_editor_property("two_sided", True)
        mi.set_editor_property("base_property_overrides", bpo)
        eal.save_asset(p)
        try:
            log.append(p + " blend=" + str(bpo.get_editor_property("blend_mode")) + " parent=" + mi.get_editor_property("parent").get_name())
        except Exception as e:
            log.append(p + " ok")
    # material wzgorza z tekstura koloru
    at = unreal.AssetToolsHelpers.get_asset_tools()
    ip = "/Game/Models/MI_Hill"
    if eal.does_asset_exist(ip): eal.delete_asset(ip)
    mi = at.create_asset("MI_Hill", "/Game/Models", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", eal.load_asset("/Game/Models/M_Textured"))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseTex", eal.load_asset("/Game/Models/Hill/hill/Textures/render_map"))
    eal.save_asset(ip)
    log.append("MI_Hill ok")
except Exception as e:
    log.append("ERR " + repr(e))
open("D:/Temp/forest.txt", "w").write("\n".join(log))
