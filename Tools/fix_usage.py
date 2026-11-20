import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
log = []
for p in ("/Game/Models/M_Textured", "/Game/Models/M_TexMasked", "/Game/Models/M_ChickenTint"):
    m = eal.load_asset(p)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(m); eal.save_asset(p)
    log.append(p + " ok")
open("D:/Temp/usage.txt", "w").write("\n".join(log))
