using UnrealBuildTool;

public class ChickenCiv : ModuleRules
{
	public ChickenCiv(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "HTTP", "ApplicationCore", "Media", "MediaAssets", "ProceduralMeshComponent" });
	}
}
