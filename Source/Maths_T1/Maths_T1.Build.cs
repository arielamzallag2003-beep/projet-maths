// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Maths_T1 : ModuleRules
{
	public Maths_T1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Chaque .cpp est compilé séparément (pas de « unity build »). Sinon un #define posé dans un
		// .cpp (ex. #define GetAngle ...) déborde sur les fichiers regroupés après lui, y compris les
		// en-têtes du moteur, et casse la compilation de façon difficile à diagnostiquer.
		bUseUnity = false;

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
			"Slate",
			"SlateCore",
			"WebBrowser" // SWebBrowser, pour le panneau de la scène de test (GeomUI)
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Maths_T1",
			"Maths_T1/Variant_Strategy",
			"Maths_T1/Variant_Strategy/UI",
			"Maths_T1/Variant_TwinStick",
			"Maths_T1/Variant_TwinStick/AI",
			"Maths_T1/Variant_TwinStick/Gameplay",
			"Maths_T1/Variant_TwinStick/UI",
			"Maths_T1/GeomUI",
			"Maths_T1/Voronoi"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
