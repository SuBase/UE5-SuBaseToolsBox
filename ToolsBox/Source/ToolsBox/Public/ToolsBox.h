// Copyright 2026 SuBase. All Rights Reserved.
#pragma once


#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class FUICommandList;

class FToolsBoxModule : public IModuleInterface
{
public:
	virtual void StartupModule() override; 
	virtual void ShutdownModule() override;

	void RegisterMenu(); 
	
	void OnButtonClick();


	TSharedRef<SDockTab> OnSpawnToolsBoxTab(const FSpawnTabArgs& SpawnTabArgs); 

	TSharedPtr<FUICommandList> OpenToolsBoxDockTab_CommandList; 

	TSharedRef<SDockTab> OnSpawnToolTab(const FSpawnTabArgs& SpawnTabArgs, TFunction<TSharedRef<SWidget>()> NewToolTab);
	
};
