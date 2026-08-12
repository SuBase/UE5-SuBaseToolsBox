// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/Right-ClickOperationTool/AAU.h"

#include "AnimationModifier.h"
#include "AnimationModifiersAssetUserData.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ScopedTransaction.h"
#include "Animation/AnimSequence.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SAAU"
 
void SAAU::Construct(const FArguments& InArgs)
{
	RefreshAvailableModifiers();
 
	ChildSlot
	[
		// 1. 使用 SScrollBox 包裹，允许垂直滚动
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(0,5,0,20)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Title", "相关操作请在资产（或场景Actor）右键菜单中脚本操作中查看\n 若是没有请自行创建类继承自AssetAction和ActorAction"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]
		+ SScrollBox::Slot()
		.Padding(0,10,0,10)
		[
			
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddAnimationModify", "批量添加动画修饰符并应用"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddAnimationModifyDesc", "为选中的骨骼网格体批量应用动画修饰符"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 6))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox)
                	.MaxDesiredWidth(700.0f) 
                	.HAlign(HAlign_Left)
                	.Padding(10)
                	[
                		SNew(SVerticalBox)
                		
                		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                		[
                			SNew(SHorizontalBox)
                			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                			[ SNew(STextBlock).Text(LOCTEXT("SelectLabel", "选择: ")) ]
                			+ SHorizontalBox::Slot().AutoWidth()
                			[
                				SNew(SComboBox<TSharedPtr<FString>>)
                				.OptionsSource(&ModifierNames)
                				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) {
                					return SNew(STextBlock).Text(FText::FromString(*InItem));
                				})
                				.OnSelectionChanged(this, &SAAU::OnModifierComboChanged)
                				[
                					SNew(STextBlock).Text_Lambda([this]() {
                						return FText::FromString(CurrentSelectedName.IsValid() ? *CurrentSelectedName : TEXT("请选择..."));
                					})
                				]
                			]
                			+ SHorizontalBox::Slot().AutoWidth()
                			[
                				SNew(SButton)
                				.Text(LOCTEXT("AddBtn", "确认选择"))
                				.OnClicked(this, &SAAU::OnClickAddToList)
                			]
                		]
         
                		// --- 已选列表区 ---
                		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10)
                		[
                			SNew(SVerticalBox)
                			+ SVerticalBox::Slot().AutoHeight()
                			[ SNew(STextBlock).Text(LOCTEXT("StagedHeader", "（已选列表）")).Font(FAppStyle::GetFontStyle("BoldFont")) ]
                			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                			[
                				SNew(SBox).HeightOverride(150.0f) // 限制列表高度
                				[
                					SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                					[
                						SAssignNew(StagedListView, SListView<TSharedPtr<FString>>)
                						.ListItemsSource(&StagedModifierNames)
                						.OnGenerateRow(this, &SAAU::OnGenerateRowForList)
                					]
                				]
                			]
                			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                			[
                				SNew(SButton).Text(LOCTEXT("ClearStaged", "清空列表"))
                				.OnClicked(this, &SAAU::OnClickClearList)
                			]
                		]
         
                		// --- 操作按钮区 (紧凑排布) ---
                		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                		[
                			SNew(SHorizontalBox)
                			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
                			[
                				SNew(SButton).Text(LOCTEXT("ApplyOnly", "批量添加"))
                				.OnClicked_Raw(this, &SAAU::OnProcessModifiers, false)
                			]
                			+ SHorizontalBox::Slot().AutoWidth()
                			[
                				SNew(SButton).Text(LOCTEXT("ApplyFull", "批量添加并应用"))
                				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                				.OnClicked_Raw(this, &SAAU::OnProcessModifiers, true)
                			]
                		]
         
                		
                	]
				]
				
							
			]
			
		]
	];
}
 


void SAAU::RefreshAvailableModifiers()
{
	ModifierNames.Empty();
	AvailableModifierClasses.Empty();
 
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UAnimationModifier::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			AvailableModifierClasses.Add(*It);
			ModifierNames.Add(MakeShared<FString>(It->GetName()));
		}
	}
}
 
TArray<UAnimSequence*> SAAU::GetSelectedAnimSequences()
{
	TArray<UAnimSequence*> OutAnims;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
 
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			OutAnims.Add(Anim);
		}
	}
	return OutAnims;
}
 
FReply SAAU::OnProcessModifiers(bool bApplyImmediately)
{
	TArray<UAnimSequence*> TargetAnims = GetSelectedAnimSequences();
	if (TargetAnims.Num() == 0 || StagedModifierClasses.Num() == 0) return FReply::Handled();
 
	const FScopedTransaction Transaction(LOCTEXT("BatchModTx", "批量处理动画修饰符"));
 
	// 对应你源码中的 UE::Anim::FApplyModifiersScope
	// 这个 Scope 确保在应用期间禁用某些编辑器更新，防止数据不一致
	UE::Anim::FApplyModifiersScope Scope;
 
	for (UAnimSequence* Anim : TargetAnims)
	{
		Anim->Modify();
 
		for (UClass* ModClass : StagedModifierClasses)
		{
			// 使用静态方法添加类
			UAnimationModifiersAssetUserData::AddAnimationModifierOfClass(Anim, ModClass);
 
			if (bApplyImmediately)
			{
				UAnimationModifiersAssetUserData* UserData = Anim->GetAssetUserData<UAnimationModifiersAssetUserData>();
				if (UserData)
				{
					// 获取刚添加的实例
					for (UAnimationModifier* Instance : UserData->GetAnimationModifierInstances())
					{
						if (Instance && Instance->GetClass() == ModClass)
						{
							// 调用你发现的核心函数
							Instance->ApplyToAnimationSequence(Anim);
						}
					}
				}
			}
		}
		Anim->MarkPackageDirty();
	}
 
	return FReply::Handled();
}
 
TSharedRef<ITableRow> SAAU::OnGenerateRowForList(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*Item))
		];
}
 
void SAAU::OnModifierComboChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	CurrentSelectedName = NewSelection;
}
 
FReply SAAU::OnClickAddToList()
{
	if (CurrentSelectedName.IsValid())
	{
		for (UClass* Class : AvailableModifierClasses)
		{
			if (Class->GetName() == *CurrentSelectedName)
			{
				StagedModifierNames.Add(CurrentSelectedName);
				StagedModifierClasses.Add(Class);
				break;
			}
		}
		StagedListView->RequestListRefresh();
	}
	return FReply::Handled();
}
 
FReply SAAU::OnClickClearList()
{
	StagedModifierNames.Empty();
	StagedModifierClasses.Empty();
	StagedListView->RequestListRefresh();
	return FReply::Handled();
}
 
#undef LOCTEXT_NAMESPACE