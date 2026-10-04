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
				// No GameplayFramework dependency, on purpose: DaProcGen is engine math + PCG, and nothing
				// in it includes a framework header. Add it back only when a file actually needs one.
				// ADaProcGenActor replicates its run seed with plain DOREPLIFETIME, which in 5.x still
				// compiles against the push-model registration machinery declared in this module.
				"NetCore",
			}
			);
	}
}
