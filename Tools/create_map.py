# Tworzy pusta mape /Game/Maps/Main z trybem gry ChickenGameMode (mapa jest budowana w C++ przy starcie).
# Uruchomienie: UnrealEditor-Cmd ChickenCiv.uproject -run=pythonscript "-script=<pelna sciezka>\Tools\create_map.py" -unattended -nosplash -nullrhi
import unreal

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
game_mode = unreal.load_class(None, "/Script/ChickenCiv.ChickenGameMode")
world.get_world_settings().set_editor_property("default_game_mode", game_mode)
ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/Main")
unreal.log("MAPA ZAPISANA Main: %s (tryb gry: %s)" % (ok, game_mode))
