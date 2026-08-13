// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/AutoPrefix/DveloperSetting_AutoPrefix.h"

UDveloperSetting_AutoPrefix::UDveloperSetting_AutoPrefix()
{
	// 让它出现在"项目设置 -> 插件 -> 自动前缀"下面
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("AutoPrefix");
}

FString UDveloperSetting_AutoPrefix::FindPrefixForClass(const UClass* InClass) const
{
	if (!InClass)
	{
		return FString();
	}

	// 在所有规则里挑"离 InClass 最近"的那条。
	// 比如 InClass 是 Character，规则表里有 Actor(BP_) 也有 Character(CH_)，
	// 我们肯定想要更具体的 CH_，所以这里记录最小继承距离。
	const FAutoPrefixRule* BestRule = nullptr;
	int32 BestDepth = MAX_int32;

	for (const FAutoPrefixRule& Rule : PrefixRules)
	{
		if (Rule.Prefix.IsEmpty() || Rule.TargetClass.IsNull())
		{
			continue;
		}

		// 先试着用已经加载的类，避免每次都去同步加载（慢）
		UClass* RuleClass = Rule.TargetClass.ResolveClass();
		if (!RuleClass)
		{
			RuleClass = Rule.TargetClass.TryLoadClass<UObject>();
		}

		// 只有 InClass 是 RuleClass 的子类（含自身）才命中
		if (!RuleClass || !InClass->IsChildOf(RuleClass))
		{
			continue;
		}

		// 数一下隔了几层继承，距离越近越具体
		int32 Depth = 0;
		for (const UClass* Cursor = InClass; Cursor && Cursor != RuleClass; Cursor = Cursor->GetSuperClass())
		{
			++Depth;
		}

		if (Depth < BestDepth)
		{
			BestDepth = Depth;
			BestRule = &Rule;
		}
	}

	return BestRule ? BestRule->Prefix : FString();
}
