using UnrealBuildTool;

public class ChickenCivTarget : TargetRules
{
	public ChickenCivTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChickenCiv");
	}
}
