// Copyright 2026 SuBase. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Slate_Assist/FIconStyle.h" 
#include "Framework/Commands/Commands.h" 


class FOpenToolsBox_Command : public TCommands<FOpenToolsBox_Command>
{
public:
	FOpenToolsBox_Command()
	: TCommands<FOpenToolsBox_Command>
	(
		TEXT("ToolsBox"), // 要跟图标匹配
		NSLOCTEXT("Contexts", "SuBaseToolsBox_PluginCommand", "SuBaseToolsBox_PluginCommand"), 
		NAME_None,
		FIconStyle::Get_IconsName()
	) {}

	
	virtual void RegisterCommands() override;

	
	TSharedPtr<FUICommandInfo> ToolsBox_OpenToolsBox;
};