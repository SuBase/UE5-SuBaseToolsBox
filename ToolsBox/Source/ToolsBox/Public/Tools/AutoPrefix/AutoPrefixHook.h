// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UFactory;


class FAutoPrefixHook
{
public:
	/** 插件启动时装上挂钩 */
	static void Register();

	/** 插件关闭时拆掉挂钩 */
	static void Unregister();

private:
	/** 收到"新建资产"事件时，改写这次的默认名前缀 */
	static void HandleNewAssetCreated(UFactory* Factory);

	/** 拿到该用哪个类去匹配前缀：蓝图用父类，其它资产用资产本身的类 */
	static UClass* ResolveClassForPrefix(UFactory* Factory);

	/** 记录我们的委托句柄，拆挂钩时用 */
	static FDelegateHandle NewAssetCreatedHandle;
};
