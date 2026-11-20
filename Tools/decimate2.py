import unreal
log = []
eal = unreal.EditorAssetLibrary
def reduce(path, pct):
    m = eal.load_asset(path)
    try:
        models = m.get_editor_property("source_models")
        rs = models[0].get_editor_property("reduction_settings")
        rs.set_editor_property("percent_triangles", pct)
        models[0].set_editor_property("reduction_settings", rs)
        m.set_editor_property("source_models", models)
        m.post_edit_change()
        eal.save_asset(path)
        log.append(path + " reduced via source_models")
    except Exception as e:
        log.append(path + " ERR " + repr(e))
reduce("/Game/Models/Mountain/SM_Mountain", 0.01)
reduce("/Game/Models/Station/SM_Station", 0.08)
open("D:/Temp/decim2.txt", "w").write("\n".join(log))
