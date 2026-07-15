// Copyright enaDyne. Phase D milestone 1 renderer.

using UnrealBuildTool;

public class MyProject : ModuleRules
{
	public MyProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Json/JsonUtilities: read the replay file. Engine: actors, DrawDebug.
		// No gameplay/combat modules by design - this module renders recorded
		// events only; it computes nothing about the fight.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"Niagara",
			"InputCore"   // EKeys/FKey for the showcase-browser key bindings (Step 3b)
		});
	}
}
