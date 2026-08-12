// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "DveloperSetting_AutoPrefix.generated.h"

/**
 * 一条规则：某个类 -> 新建时默认带上的前缀。
 *
 * 怎么匹配？沿继承链就近挑一条：
 *   - 蓝图：拿"父类"来比（父类是 Actor，就命中 BP_）
 *   - 其它资产：拿"资产类型"来比（材质 -> M_）
 * 比如一个类既是 Actor 又命中某条更具体的规则，就取更具体的那条。
 */
USTRUCT()
struct FAutoPrefixRule
{
	GENERATED_BODY()

	/** 要匹配的类（蓝图填父类，其它资产填资产类型） */
	UPROPERTY(config, EditAnywhere, Category = "AutoPrefix", meta = (MetaClass = "/Script/CoreUObject.Object"))
	FSoftClassPath TargetClass;

	/** 命中后加在名字前面的前缀，比如 BP_ / M_ / T_ */
	UPROPERTY(config, EditAnywhere, Category = "AutoPrefix")
	FString Prefix;

	FAutoPrefixRule() = default;

	FAutoPrefixRule(const FString& InClassPath, const FString& InPrefix)
		: TargetClass(InClassPath), Prefix(InPrefix)
	{
	}
};

/**
 * 自动前缀的项目设置。
 *
 * 这个类标记了 config = Game + defaultconfig，
 * 所以在"项目设置"里改完会写进 <项目>/Config/DefaultGame.ini。
 * 工具面板上的"应用到项目设置"按钮，写的也就是这里。
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "自动前缀 (AutoPrefix)"))
class TOOLSBOX_API UDveloperSetting_AutoPrefix : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDveloperSetting_AutoPrefix();

	/** 总开关：关掉后新建东西就用引擎原来的默认名字 */
	UPROPERTY(config, EditAnywhere, Category = "常规")
	bool bEnableAutoPrefix = true;

	/** 当前用着哪套前缀（由工具面板写进来，主要起个记录作用） */
	UPROPERTY(config, VisibleAnywhere, Category = "常规")
	FString ActivePrefixSetName;

	/** 前缀规则表 */
	UPROPERTY(config, EditAnywhere, Category = "前缀规则", meta = (TitleProperty = "Prefix"))
	TArray<FAutoPrefixRule> PrefixRules;

	/** 沿继承链找最近的一条规则，返回它的前缀；没命中就返回空串 */
	FString FindPrefixForClass(const UClass* InClass) const;
};
