// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Maths_T1 : ModuleRules
{
	public Maths_T1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Maths_T1",
			"Maths_T1/Variant_Strategy",
			"Maths_T1/Variant_Strategy/UI",
			"Maths_T1/Variant_TwinStick",
			"Maths_T1/Variant_TwinStick/AI",
			"Maths_T1/Variant_TwinStick/Gameplay",
			"Maths_T1/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
