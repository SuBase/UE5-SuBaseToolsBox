// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/AutoPrefix/AutoPrefixHook.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "Factories/Factory.h"
#include "Tools/AutoPrefix/DveloperSetting_AutoPrefix.h"

#define LOCTEXT_NAMESPACE "AutoPrefix"

FDelegateHandle FAutoPrefixHook::NewAssetCreatedHandle;

void FAutoPrefixHook::Register()
{
	// 只要还没挂过，就挂上"新建资产"的监听
	if (!NewAssetCreatedHandle.IsValid())
	{
		NewAssetCreatedHandle = FEditorDelegates::OnNewAssetCreated.AddStatic(&FAutoPrefixHook::HandleNewAssetCreated);
	}
}

void FAutoPrefixHook::Unregister()
{
	if (NewAssetCreatedHandle.IsValid())
	{
		FEditorDelegates::OnNewAssetCreated.Remove(NewAssetCreatedHandle);
		NewAssetCreatedHandle.Reset();
	}
}

UClass* FAutoPrefixHook::ResolveClassForPrefix(UFactory* Factory)
{
	if (!Factory)
	{
		return nullptr;
	}

	// 蓝图工厂（普通蓝图 / UMG / 动画蓝图……）里都藏着一个 ParentClass 字段，存的是"父类"。
	// 用反射去读它，就能同时覆盖这些工厂，而不用挨个 include 对应的编辑器模块。
	if (FClassProperty* ParentClassProp = FindFProperty<FClassProperty>(Factory->GetClass(), TEXT("ParentClass")))
	{
		if (UObject* Value = ParentClassProp->GetPropertyValue_InContainer(Factory))
		{
			if (UClass* ParentClass = Cast<UClass>(Value))
			{
				return ParentClass;
			}
		}
	}

	// 读不到父类（比如材质、贴图这种资产），就退回用工厂自己支持的类
	return Factory->GetSupportedClass();
}

void FAutoPrefixHook::HandleNewAssetCreated(UFactory* Factory)
{
	if (!Factory)
	{
		return;
	}

	UClass* SupportedClass = Factory->GetSupportedClass();
	if (!SupportedClass)
	{
		return;
	}

	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>(TEXT("AssetTools"));
	if (!AssetToolsModule)
	{
		return;
	}
	IAssetTools& AssetTools = AssetToolsModule->Get();

	const UDveloperSetting_AutoPrefix* Settings = GetDefault<UDveloperSetting_AutoPrefix>();

	// 关掉开关、或者没匹配到规则时，都传一个空串。
	// 空串会清掉上一次注册的默认名，让引擎回落到它自己的默认命名。
	if (!Settings || !Settings->bEnableAutoPrefix)
	{
		AssetTools.RegisterDefaultAssetNameForClass(SupportedClass, FString());
		return;
	}

	// 蓝图优先按"父类"匹配；父类没命中，再退回按"资产本身的类"匹配
	//（比如 UWidgetBlueprint 这种只在资产类上有规则的情况）。
	UClass* LookupClass = ResolveClassForPrefix(Factory);
	FString Prefix = Settings->FindPrefixForClass(LookupClass);
	if (Prefix.IsEmpty() && LookupClass != SupportedClass)
	{
		Prefix = Settings->FindPrefixForClass(SupportedClass);
	}

	AssetTools.RegisterDefaultAssetNameForClass(SupportedClass, Prefix);
}

#undef LOCTEXT_NAMESPACE
