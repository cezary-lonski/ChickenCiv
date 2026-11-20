import unreal
eal = unreal.EditorAssetLibrary
log = []
for n in ("Ps1Tree1Texture", "Ps1TreeTrunk1", "RockMaterial1"):
    mi = eal.load_asset("/Game/Models/Forest/scene/Materials/" + n)
    log.append(n + " tex=" + str([(str(v.parameter_info.name), v.parameter_value.get_name() if v.parameter_value else None) for v in mi.texture_parameter_values]) + " vec=" + str([(str(v.parameter_info.name), str(v.parameter_value)) for v in mi.vector_parameter_values]) + " scal=" + str([(str(v.parameter_info.name), v.parameter_value) for v in mi.scalar_parameter_values]))
open("D:/Temp/mi.txt", "w").write("\n".join(log))
