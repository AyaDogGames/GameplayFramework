// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayFramework : ModuleRules
{
	public GameplayFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"CoreUObject", 
				"Engine", 
				"InputCore", 
				"GameplayAbilities", 
				"EnhancedInput", 
				"GameplayTasks", 
				"DeveloperSettings", 
				"GameplayTags", 
				"AIModule", 
				"CommonUI",
				"CommonInput"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"UMG", 
				"Niagara",
				"NavigationSystem",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
