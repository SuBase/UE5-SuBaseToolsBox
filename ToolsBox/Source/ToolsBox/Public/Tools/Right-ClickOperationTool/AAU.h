// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"


class  SAAU : public SCompoundWidget
{
public:
	
	
	SLATE_BEGIN_ARGS(SAAU) {}
	SLATE_END_ARGS()

	
	void Construct(const FArguments& InArgs);
	
	// 存储可用修饰符类
	TArray<UClass*> AvailableModifierClasses;
	TArray<TSharedPtr<FString>> ModifierNames;
	TSharedPtr<FString> CurrentSelectedName;
 
	// 待处理列表
	TArray<TSharedPtr<FString>> StagedModifierNames;
	TArray<UClass*> StagedModifierClasses;
 
	TSharedPtr<SListView<TSharedPtr<FString>>> StagedListView;
 
	// 逻辑函数
	void RefreshAvailableModifiers();
	TArray<class UAnimSequence*> GetSelectedAnimSequences();
	FReply OnProcessModifiers(bool bApplyImmediately);
 
	// UI 回调
	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnModifierComboChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FReply OnClickAddToList();
	FReply OnClickClearList();
};
