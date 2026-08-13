// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 工具存档目录助手。
 *
 * 每个需要往硬盘存东西的工具，都应该在 ToolUserDataSave 下面再开一个
 * 以自己名字命名的子文件夹，把文件放在里面，这样多个工具之间不会互相覆盖。
 *
 * 另外这个文件夹是被 git 忽略的，别人 clone 下来时可能根本不存在，
 * 所以这里取目录时会顺手把文件夹建好（含中间目录），保证开箱即用。
 */
class FToolUserSave
{
public:
	/** 返回 <插件>/.../ToolUserDataSave/<ToolName>/ 并确保该文件夹已存在 */
	static FString GetToolSaveDir(const FString& ToolName);

	/**
	 * 一次性迁移：新位置 <ToolName>/<FileName> 还不存在、但旧的扁平 <FileName> 存在时，
	 * 把旧文件搬过去。用来兼容升级前直接把文件丢在 ToolUserDataSave 根目录的老数据。
	 */
	static void MigrateLegacyFile(const FString& ToolName, const FString& FileName);
};
