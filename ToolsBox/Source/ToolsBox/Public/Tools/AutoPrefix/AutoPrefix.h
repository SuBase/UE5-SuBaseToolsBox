// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

class SVerticalBox;
class SMultiLineEditableText;

/** 面板里的一行规则：一个类 + 它的前缀 */
struct FAutoPrefixRuleItem
{
	/** 要匹配的类（蓝图填父类，其它资产填资产类型） */
	TWeakObjectPtr<UClass> TargetClass;

	/** 前缀，比如 BP_ / M_ / T_ */
	FString Prefix;
};

/** 一整"套"前缀（可以存好几套，用下拉框切换） */
struct FAutoPrefixSetItem
{
	/** 这套的名字，也是 JSON 里的唯一标识 */
	FString SetName;

	/** 规则列表 */
	TArray<TSharedPtr<FAutoPrefixRuleItem>> Rules;

	/** 有没有写进过 JSON：没保存过的新建/默认套装，保存时直接原地命名，不重复加一套 */
	bool bIsSaved = false;
};

/**
 * 自动前缀工具面板。
 *
 * 在这里维护多套前缀方案，保存成 JSON（都追加在同一个文件里）；
 * 想让编辑器真正用起来，就点"应用到项目设置"，把当前这套写进 DefaultGame.ini。
 */
class SAutoPrefix : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAutoPrefix) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ---------- 数据 ----------
	TArray<TSharedPtr<FAutoPrefixSetItem>> PrefixSets;   // 所有保存过/新建的套装
	TSharedPtr<FAutoPrefixSetItem> CurrentSet;           // 现在正在编辑的这套
	FString EditingSetName;                              // 套装名称输入框里的内容

	// ---------- 控件 ----------
	TSharedPtr<SComboBox<TSharedPtr<FAutoPrefixSetItem>>> SetComboBox;
	TSharedPtr<SVerticalBox> RuleContainer;
	TSharedPtr<SMultiLineEditableText> LogBox;
	FString LogText;

	// ---------- 日志 ----------
	void AppendLog(const FString& Message);

	// ---------- 存档路径 ----------
	FString GetSaveDirectory() const;
	FString GetFullConfigPath() const;

	// ---------- JSON 读写 ----------
	void LoadAllSetsFromJson();                 // 从 JSON 读出所有套装
	bool WriteAllSetsToJson();                  // 把所有套装整体写回 JSON
	void SaveCurrentSetToJson();                // 保存当前套装：同名覆盖，不同名追加
	void DeleteSet(TSharedPtr<FAutoPrefixSetItem> Target);
	TArray<TSharedPtr<FAutoPrefixRuleItem>> DeepCopyRules(const TArray<TSharedPtr<FAutoPrefixRuleItem>>& InRules) const;

	// ---------- 和项目设置互通 ----------
	void ApplyCurrentSetToProjectSettings();    // 把当前套装写进 DefaultGame.ini
	void PullFromProjectSettings();             // 把项目设置里的配置读回面板

	// ---------- 规则列表界面 ----------
	void RefreshRuleUI();
	void AddRuleRow();
	TSharedRef<SWidget> CreateRuleRow(TSharedPtr<FAutoPrefixRuleItem> Item);

	// ---------- 套装下拉框 ----------
	TSharedRef<SWidget> OnGenerateSetComboWidget(TSharedPtr<FAutoPrefixSetItem> InItem);
	void OnSetSelectionChanged(TSharedPtr<FAutoPrefixSetItem> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetCurrentSetNameText() const;
	void SwitchToSet(TSharedPtr<FAutoPrefixSetItem> Target);

	// ---------- 默认值 ----------
	void SeedDefaultSet();                      // 第一次用，先给一套常用默认
	TArray<TSharedPtr<FAutoPrefixRuleItem>> BuildDefaultRules() const;  // 造一批内置常见前缀
	void ResetCurrentSetToDefaults();          // 把当前套装的规则恢复成内置默认（仅内存）
};
