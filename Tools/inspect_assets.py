import unreal
out = []
for p in unreal.EditorAssetLibrary.list_assets("/Game/Models", recursive=True):
    a = unreal.EditorAssetLibrary.load_asset(p)
    c = a.get_class().get_name()
    line = p + " : " + c
    try:
        if c in ("StaticMesh", "SkeletalMesh"):
            b = a.get_bounds()
            mats = a.static_materials if c == "StaticMesh" else a.materials
            line += " ext=%s mats=%s" % (b.box_extent, [str(m.material_slot_name) for m in mats])
    except Exception as e:
        line += " ERR " + str(e)
    out.append(line)
try:
    sk = unreal.EditorAssetLibrary.load_asset("/Game/Models/Chicken_001_Skeleton")
    out.append("BONES " + str([str(n) for n in sk.get_editor_property("bone_tree")]) if False else "BONES-API " + str([m for m in dir(sk) if "bone" in m.lower()]))
    sm = unreal.EditorAssetLibrary.load_asset("/Game/Models/Chicken_001")
    out.append("SMBONES " + str([m for m in dir(sm) if "bone" in m.lower() or "socket" in m.lower()]))
except Exception as e:
    out.append("ERR2 " + str(e))
open("D:/Temp/inspect.txt", "w", encoding="utf-8").write("\n".join(out))
