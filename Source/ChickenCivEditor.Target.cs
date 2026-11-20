using UnrealBuildTool;

public class ChickenCivEditorTarget : TargetRules
{
	public ChickenCivEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChickenCiv");
	}
}
