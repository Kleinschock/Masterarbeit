// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class HCI_Praktikum_VR_APITarget : TargetRules
{
	public HCI_Praktikum_VR_APITarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		ExtraModuleNames.AddRange( new string[] { "HCI_Praktikum_VR_API" } );
	}
}
