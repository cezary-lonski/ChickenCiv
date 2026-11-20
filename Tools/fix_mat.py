import unreal
m = unreal.EditorAssetLibrary.load_asset("/Game/Models/M_ChickenTint")
m.set_editor_property("used_with_skeletal_mesh", True)
unreal.MaterialEditingLibrary.recompile_material(m)
unreal.EditorAssetLibrary.save_asset("/Game/Models/M_ChickenTint")
open("D:/Temp/fixmat.txt", "w").write("ok")
