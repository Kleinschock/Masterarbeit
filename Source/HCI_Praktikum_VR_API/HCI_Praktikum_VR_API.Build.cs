// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class HCI_Praktikum_VR_API : ModuleRules
{
    public HCI_Praktikum_VR_API(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // HeadMountedDisplay is needed for HMD functions
        // UMG is needed for WidgetComponent and UserWidget
        // Niagara for particle systems
        // SlateCore might be needed by UMG indirectly or for other UI elements
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "XRBase",
            "HeadMountedDisplay",
            "Niagara",
            "UMG",
            "AudioAnalysisTools",
            "RuntimeAudioImporter",
            "Json",
            "JsonUtilities",
            "SlateCore",
            "InputDevice" // <--- HIER HINZUGEFÜGT
        });


        PrivateDependencyModuleNames.AddRange(new string[] {
            // No need for HeadMountedDisplay here if it's public
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}