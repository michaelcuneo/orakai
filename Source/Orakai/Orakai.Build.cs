// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Orakai : ModuleRules
{
	public Orakai(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Orakai",
			"Orakai/Variant_Platforming",
			"Orakai/Variant_Platforming/Animation",
			"Orakai/Variant_Combat",
			"Orakai/Variant_Combat/AI",
			"Orakai/Variant_Combat/Animation",
			"Orakai/Variant_Combat/Gameplay",
			"Orakai/Variant_Combat/Interfaces",
			"Orakai/Variant_Combat/UI",
			"Orakai/Variant_SideScrolling",
			"Orakai/Variant_SideScrolling/AI",
			"Orakai/Variant_SideScrolling/Gameplay",
			"Orakai/Variant_SideScrolling/Interfaces",
			"Orakai/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
