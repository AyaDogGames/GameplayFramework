// Copyright Dream Awake Solutions LLC. All Rights Reserved.

using UnrealBuildTool;

public class DaProcGen : ModuleRules
{
	public DaProcGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GameplayFramework",
			}
			);
	}
}
