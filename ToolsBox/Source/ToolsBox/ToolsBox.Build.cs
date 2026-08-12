// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ToolsBox : ModuleRules
{
	public ToolsBox(ReadOnlyTargetRules Target) : base(Target)
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
				"Slate",
				"SlateCore",
				"ToolMenus",
				"EditorStyle",
				"Projects",
				"UnrealEd",
				"LevelEditor",
				"AssetTools", 
				"AssetRegistry", 
				"MaterialEditor", 
				"DesktopPlatform",
				"PropertyEditor",
				"ContentBrowser",
				"EditorScriptingUtilities",
				"BlueprintEditorLibrary",
				"GeometryFramework",     
				"GeometryScriptingCore" ,
				"Blutility", 
				"PhysicsCore",
				"Chaos",
				"ChaosCore",
				"BlueprintGraph",
				"StaticMeshEditor",
				"AnimationBlueprintLibrary",
				"AnimationModifiers",  // 必须：提供便捷的操作 API
				"InputCore",
				"PropertyEditor", 
				"Json", 
				"JsonUtilities" ,
				"EditorWidgets",
				"DeveloperSettings",
				"EditorFramework",          // FEditorModeInfo / FEditorModeRegistry（注册物理摆放生成模式）
		
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				
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
