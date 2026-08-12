// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/ToolUserSaveHelper.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace
{
	/** ToolUserDataSave 的根目录（在插件目录下，且被 git 忽略） */
	FString GetBaseSaveDir()
	{
		return FPaths::ProjectPluginsDir() + TEXT("ToolsBox/Source/ToolsBox/Public/Tools/ToolUserDataSave/");
	}
}

FString FToolUserSave::GetToolSaveDir(const FString& ToolName)
{
	FString Dir = GetBaseSaveDir() + ToolName + TEXT("/");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		// CreateDirectoryTree 会把中间缺失的目录（包括 ToolUserDataSave 本身）一并建好
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	return Dir;
}

void FToolUserSave::MigrateLegacyFile(const FString& ToolName, const FString& FileName)
{
	const FString NewPath = GetToolSaveDir(ToolName) + FileName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 已经在新位置了，说明迁过了，跳过
	if (PlatformFile.FileExists(*NewPath))
	{
		return;
	}

	// 旧的扁平文件还在根目录，就把它搬到新的工具子目录里
	const FString LegacyPath = GetBaseSaveDir() + FileName;
	if (PlatformFile.FileExists(*LegacyPath))
	{
		PlatformFile.MoveFile(*NewPath, *LegacyPath);
	}
}
