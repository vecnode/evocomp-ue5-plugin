using UnrealBuildTool;

public class EvoCompPluginEditor : ModuleRules
{
	public EvoCompPluginEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EvoCompPlugin"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"PropertyEditor",
				"ToolMenus",
				"LevelEditor",
				"UnrealEd"
			}
		);
	}
}
