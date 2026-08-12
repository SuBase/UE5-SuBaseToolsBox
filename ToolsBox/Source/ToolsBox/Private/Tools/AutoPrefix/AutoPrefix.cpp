// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/AutoPrefix/AutoPrefix.h"

#include "PropertyCustomizationHelpers.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Tools/AutoPrefix/AutoPrefixHook.h"
#include "Tools/AutoPrefix/DveloperSetting_AutoPrefix.h"
#include "Tools/ToolUserSaveHelper.h"

#define LOCTEXT_NAMESPACE "AutoPrefixTool"

namespace
{
	/** JSON 文件名：所有配置都追加在同一个文件里 */
	const TCHAR* AutoPrefixJsonFileName = TEXT("AutoPrefixSettings.json");

	/**
	 * 根据类的路径字符串拿到 UClass 对象。
	 * 类已经加载过就直接取；没加载过就试着加载一次；都拿不到就返回空（说明这次它没在内存里）。
	 */
	UClass* ResolveClassByPath(const FString& ClassPath)
	{
		FSoftClassPath SoftPath(ClassPath);
		if (UClass* Loaded = SoftPath.ResolveClass())
		{
			return Loaded;
		}
		return SoftPath.TryLoadClass<UObject>();
	}
}

void SAutoPrefix::Construct(const FArguments& InArgs)
{
	// 先试着读 JSON；一个配置都没有就先给一套常用默认
	LoadAllSetsFromJson();
	if (PrefixSets.Num() == 0)
	{
		SeedDefaultSet();
	}
	CurrentSet = PrefixSets.Num() > 0 ? PrefixSets[0] : nullptr;
	EditingSetName = CurrentSet.IsValid() ? CurrentSet->SetName : TEXT("默认");

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---------- 1. 说明 + 配置管理 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("AutoPrefixHelp",
						"使用方法：\n"
						"  1. 在下方规则表里选择 目标类 及其 前缀\n"
						"  2. 填好表后可点击 保存配置 保存当前配置\n"
						"  3. 点 应用到项目设置 把当前这套写进 DefaultGame.ini 并生效\n"
						"  4. 之后新建蓝图/资产时，重命名输入框的默认值就会变成对应前缀\n"
						"  5. 改乱了可点 重置默认值 ，把当前配置恢复为默认前缀（需再点 应用到项目设置 才生效）\n"
						"  6. 本插件只接管蓝图/资产的前缀；C++ 类保持引擎原生"))
				]

				// 配置下拉框
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("SetLabel", "前缀配置: ")).MinDesiredWidth(100)
					]

					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(SetComboBox, SComboBox<TSharedPtr<FAutoPrefixSetItem>>)
						.OptionsSource(&PrefixSets)
						.OnGenerateWidget(this, &SAutoPrefix::OnGenerateSetComboWidget)
						.OnSelectionChanged(this, &SAutoPrefix::OnSetSelectionChanged)
						[
							SNew(STextBlock).Text(this, &SAutoPrefix::GetCurrentSetNameText)
						]
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("NewSetBtn", "新建空配置"))
						.ToolTipText(LOCTEXT("NewSetBtnTip", "清空当前规则表，从零开始配置一套新的前缀"))
						.OnClicked_Lambda([this]()
						{
							TSharedPtr<FAutoPrefixSetItem> NewSet = MakeShared<FAutoPrefixSetItem>();
							NewSet->SetName = TEXT("新配置");
							NewSet->bIsSaved = false;
							PrefixSets.Add(NewSet);
							if (SetComboBox.IsValid()) { SetComboBox->RefreshOptions(); }
							SwitchToSet(NewSet);
							AppendLog(TEXT("已新建空配置，配置完成后记得点 保存配置"));
							return FReply::Handled();
						})
					]
				]

				// 配置名称
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("SetNameLabel", "配置名称: ")).MinDesiredWidth(100)
					]

					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
						.HintText(LOCTEXT("SetNameHint", "这套前缀保存时使用的名字"))
						.Text_Lambda([this]() { return FText::FromString(EditingSetName); })
						.OnTextChanged_Lambda([this](const FText& InText) { EditingSetName = InText.ToString(); })
					]
				]

			]
		]

		// ---------- 2. 规则表 ----------
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5, 5, 5, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.65f)
					[ SNew(STextBlock).Text(LOCTEXT("ColClass", "目标类（蓝图填父类 / 资产填资产类型）")) ]
					+ SHorizontalBox::Slot().FillWidth(0.35f)
					[ SNew(STextBlock).Text(LOCTEXT("ColPrefix", "前缀")) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[ SNew(STextBlock).Text(LOCTEXT("ColOp", "  操作")) ]
				]

				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(5, 0)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(RuleContainer, SVerticalBox)
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddRuleBtn", "+ 添加规则"))
					.OnClicked_Lambda([this]() { AddRuleRow(); return FReply::Handled(); })
				]
			]
		]

		// ---------- 3. 操作按钮 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveSetBtn", "保存配置"))
				.ToolTipText(LOCTEXT("SaveSetBtnTip", "把当前配置追加保存到 JSON，同名则覆盖该配置"))
				.OnClicked_Lambda([this]() { SaveCurrentSetToJson(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ReloadBtn", "重新加载"))
				.ToolTipText(LOCTEXT("ReloadBtnTip", "丢弃当前修改，从 JSON 重新读取全部配置"))
				.OnClicked_Lambda([this]()
				{
					LoadAllSetsFromJson();
					if (SetComboBox.IsValid()) { SetComboBox->RefreshOptions(); }
					SwitchToSet(PrefixSets.Num() > 0 ? PrefixSets[0] : nullptr);
					AppendLog(FString::Printf(TEXT("已重新加载 %d 套前缀"), PrefixSets.Num()));
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetBtn", "重置默认值"))
				.ToolTipText(LOCTEXT("ResetBtnTip", "把当前配置的规则恢复为内置默认（常见 UE 命名约定），仅在内存中生效，点 保存配置 可持久化"))
				.OnClicked_Lambda([this]() { ResetCurrentSetToDefaults(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ApplyBtn", "应用到项目设置"))
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton") 
				.ToolTipText(LOCTEXT("ApplyBtnTip", "把当前配置写入 DefaultGame.ini，更新 项目设置 -> 插件 -> AutoPrefix "))
				.OnClicked_Lambda([this]() { ApplyCurrentSetToProjectSettings(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("PullBtn", "从项目设置读取"))
				.ToolTipText(LOCTEXT("PullBtnTip", "把项目设置里现有的配置反向读回面板"))
				.OnClicked_Lambda([this]() { PullFromProjectSettings(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenFolderBtn", "打开配置文件夹"))
				.OnClicked_Lambda([this]()
				{
					FString Dir = GetSaveDirectory();
					IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
					if (!PF.DirectoryExists(*Dir)) { PF.CreateDirectoryTree(*Dir); }
					// 转成 Windows 的反斜杠路径，否则系统打开文件夹对正斜杠会静默失败
					const FString NativeDir = FPaths::ConvertRelativePathToFull(Dir).Replace(TEXT("/"), TEXT("\\"));
					FPlatformProcess::ExploreFolder(*NativeDir);
					AppendLog(TEXT("已打开配置文件夹: ") + NativeDir);
					return FReply::Handled();
				})
			]
		]

		// ---------- 4. 日志 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5, 10, 10)
		[
			SNew(SBox).HeightOverride(110.f)
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(LogBox, SMultiLineEditableText)
						.IsReadOnly(true)
						.Text(FText::GetEmpty())
					]
				]
			]
		]
	];

	RefreshRuleUI();

	if (SetComboBox.IsValid() && CurrentSet.IsValid())
	{
		SetComboBox->SetSelectedItem(CurrentSet);
	}

	AppendLog(FString::Printf(TEXT("已载入 %d 套前缀，当前: %s"),
		PrefixSets.Num(),
		CurrentSet.IsValid() ? *CurrentSet->SetName : TEXT("无")));
}

// ============================ 日志 ============================

void SAutoPrefix::AppendLog(const FString& Message)
{
	// 每条日志前面加个时间，拼成一整段文本显示
	const FString Line = FString::Printf(TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Message);

	LogText = LogText.IsEmpty() ? Line : LogText + LINE_TERMINATOR + Line;

	if (LogBox.IsValid())
	{
		LogBox->SetText(FText::FromString(LogText));
	}
}

// ============================ 路径 ============================

FString SAutoPrefix::GetSaveDirectory() const
{
	// 每个工具在自己的子目录里存文件，互不干扰；目录不存在会被自动建好
	return FToolUserSave::GetToolSaveDir(TEXT("AutoPrefix"));
}

FString SAutoPrefix::GetFullConfigPath() const
{
	return GetSaveDirectory() + AutoPrefixJsonFileName;
}

// ============================ JSON 读写 ============================

void SAutoPrefix::LoadAllSetsFromJson()
{
	// 升级兼容：把老版本直接放在根目录的 AutoPrefixSettings.json 搬到新的工具子目录
	FToolUserSave::MigrateLegacyFile(TEXT("AutoPrefix"), AutoPrefixJsonFileName);

	PrefixSets.Empty();

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GetFullConfigPath()))
	{
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* SetsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("Sets"), SetsArray) || !SetsArray)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& SetValue : *SetsArray)
	{
		const TSharedPtr<FJsonObject>* SetObject = nullptr;
		if (!SetValue.IsValid() || !SetValue->TryGetObject(SetObject) || !SetObject)
		{
			continue;
		}

		TSharedPtr<FAutoPrefixSetItem> NewSet = MakeShared<FAutoPrefixSetItem>();
		NewSet->SetName = (*SetObject)->GetStringField(TEXT("Name"));
		NewSet->bIsSaved = true;   // 能从 JSON 读出来的，说明之前已经存过了

		const TArray<TSharedPtr<FJsonValue>>* RulesArray = nullptr;
		if ((*SetObject)->TryGetArrayField(TEXT("Rules"), RulesArray) && RulesArray)
		{
			for (const TSharedPtr<FJsonValue>& RuleValue : *RulesArray)
			{
				const TSharedPtr<FJsonObject>* RuleObject = nullptr;
				if (!RuleValue.IsValid() || !RuleValue->TryGetObject(RuleObject) || !RuleObject)
				{
					continue;
				}

				TSharedPtr<FAutoPrefixRuleItem> NewRule = MakeShared<FAutoPrefixRuleItem>();
				NewRule->Prefix = (*RuleObject)->GetStringField(TEXT("Prefix"));

				const FString ClassPath = (*RuleObject)->GetStringField(TEXT("Class"));
				if (!ClassPath.IsEmpty())
				{
					NewRule->TargetClass = ResolveClassByPath(ClassPath);
				}

				NewSet->Rules.Add(NewRule);
			}
		}

		PrefixSets.Add(NewSet);
	}
}

bool SAutoPrefix::WriteAllSetsToJson()
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SetsArray;

	for (const TSharedPtr<FAutoPrefixSetItem>& Set : PrefixSets)
	{
		if (!Set.IsValid())
		{
			continue;
		}

		const TSharedRef<FJsonObject> SetObject = MakeShared<FJsonObject>();
		SetObject->SetStringField(TEXT("Name"), Set->SetName);

		TArray<TSharedPtr<FJsonValue>> RulesArray;
		for (const TSharedPtr<FAutoPrefixRuleItem>& Rule : Set->Rules)
		{
			// 类没选上的规则不写进文件
			if (!Rule.IsValid() || !Rule->TargetClass.IsValid())
			{
				continue;
			}

			const TSharedRef<FJsonObject> RuleObject = MakeShared<FJsonObject>();
			RuleObject->SetStringField(TEXT("Class"), FSoftClassPath(Rule->TargetClass.Get()).ToString());
			RuleObject->SetStringField(TEXT("Prefix"), Rule->Prefix);
			RulesArray.Add(MakeShared<FJsonValueObject>(RuleObject));
		}
		SetObject->SetArrayField(TEXT("Rules"), RulesArray);

		SetsArray.Add(MakeShared<FJsonValueObject>(SetObject));
	}

	RootObject->SetArrayField(TEXT("Sets"), SetsArray);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutputString, *GetFullConfigPath());
}

void SAutoPrefix::SaveCurrentSetToJson()
{
	if (!CurrentSet.IsValid())
	{
		AppendLog(TEXT("保存失败: 当前没有可保存的配置"));
		return;
	}

	const FString TrimmedName = EditingSetName.TrimStartAndEnd();
	if (TrimmedName.IsEmpty())
	{
		AppendLog(TEXT("保存失败: 配置名称不能为空"));
		return;
	}

	// 看看是不是已经有一套同名（不区分大小写）的
	TSharedPtr<FAutoPrefixSetItem> Existing;
	for (const TSharedPtr<FAutoPrefixSetItem>& Set : PrefixSets)
	{
		if (Set.IsValid() && Set->SetName.Equals(TrimmedName, ESearchCase::IgnoreCase))
		{
			Existing = Set;
			break;
		}
	}

	if (Existing.IsValid())
	{
		// 同名：直接覆盖这套的内容（保留它这个对象，不产生重复项）
		Existing->Rules = DeepCopyRules(CurrentSet->Rules);
		Existing->bIsSaved = true;

		// 如果当前正在编的是另一套"还没存过"的（比如默认/新建空配置），
		// 那它已经被合并进 Existing 了，把它从列表里拿掉，免得留个空壳
		if (CurrentSet != Existing && !CurrentSet->bIsSaved)
		{
			PrefixSets.Remove(CurrentSet);
		}
		CurrentSet = Existing;
	}
	else if (!CurrentSet->bIsSaved)
	{
		// 当前是"还没存过"的新建/默认配置：直接给它起这个名字存，不重复追加
		CurrentSet->SetName = TrimmedName;
		CurrentSet->bIsSaved = true;
		if (!PrefixSets.Contains(CurrentSet))
		{
			PrefixSets.Add(CurrentSet);
		}
	}
	else
	{
		// 当前这套已经存过、但这次换了个新名字：就在文件里另存一套
		// 这里深拷贝一份规则，免得和新对象共用同一批规则项
		TSharedPtr<FAutoPrefixSetItem> NewSet = MakeShared<FAutoPrefixSetItem>();
		NewSet->SetName = TrimmedName;
		NewSet->Rules = DeepCopyRules(CurrentSet->Rules);
		NewSet->bIsSaved = true;
		PrefixSets.Add(NewSet);
		CurrentSet = NewSet;
	}

	if (SetComboBox.IsValid())
	{
		SetComboBox->RefreshOptions();
	}
	SwitchToSet(CurrentSet);

	if (WriteAllSetsToJson())
	{
		AppendLog(FString::Printf(TEXT("已保存配置 %s ，文件内共 %d 套 -> %s"),
			*CurrentSet->SetName, PrefixSets.Num(), *GetFullConfigPath()));
	}
	else
	{
		AppendLog(TEXT("保存失败: 无法写入 ") + GetFullConfigPath());
	}
}

TArray<TSharedPtr<FAutoPrefixRuleItem>> SAutoPrefix::DeepCopyRules(const TArray<TSharedPtr<FAutoPrefixRuleItem>>& InRules) const
{
	// 重新造一批规则对象，避免保存后新旧两套共用同一批指针
	TArray<TSharedPtr<FAutoPrefixRuleItem>> Out;
	for (const TSharedPtr<FAutoPrefixRuleItem>& Rule : InRules)
	{
		if (!Rule.IsValid())
		{
			continue;
		}
		TSharedPtr<FAutoPrefixRuleItem> Cloned = MakeShared<FAutoPrefixRuleItem>();
		Cloned->TargetClass = Rule->TargetClass;
		Cloned->Prefix = Rule->Prefix;
		Out.Add(Cloned);
	}
	return Out;
}

void SAutoPrefix::DeleteSet(TSharedPtr<FAutoPrefixSetItem> Target)
{
	if (!Target.IsValid())
	{
		return;
	}

	const FString RemovedName = Target->SetName;
	PrefixSets.Remove(Target);

	// 删的要是正在编的这套，就自动切到第一套
	if (CurrentSet == Target)
	{
		SwitchToSet(PrefixSets.Num() > 0 ? PrefixSets[0] : nullptr);
	}

	if (WriteAllSetsToJson())
	{
		AppendLog(FString::Printf(TEXT("已删除配置 %s ，剩余 %d 套"), *RemovedName, PrefixSets.Num()));
	}
	else
	{
		AppendLog(TEXT("删除后写回 JSON 失败"));
	}

	if (SetComboBox.IsValid())
	{
		SetComboBox->RefreshOptions();
		if (CurrentSet.IsValid())
		{
			SetComboBox->SetSelectedItem(CurrentSet);
		}
	}
}

// ============================ 和项目设置互通 ============================

void SAutoPrefix::ApplyCurrentSetToProjectSettings()
{
	if (!CurrentSet.IsValid())
	{
		AppendLog(TEXT("应用失败: 当前没有选中的配置"));
		return;
	}

	UDveloperSetting_AutoPrefix* Settings = GetMutableDefault<UDveloperSetting_AutoPrefix>();
	if (!Settings)
	{
		AppendLog(TEXT("应用失败: 拿不到 AutoPrefix 设置对象"));
		return;
	}

	// 把面板上的规则一条条搬进项目设置里
	Settings->PrefixRules.Empty();
	for (const TSharedPtr<FAutoPrefixRuleItem>& Rule : CurrentSet->Rules)
	{
		if (!Rule.IsValid() || !Rule->TargetClass.IsValid() || Rule->Prefix.IsEmpty())
		{
			continue;
		}

		FAutoPrefixRule NewRule;
		NewRule.TargetClass = FSoftClassPath(Rule->TargetClass.Get());
		NewRule.Prefix = Rule->Prefix;
		Settings->PrefixRules.Add(NewRule);
	}

	Settings->ActivePrefixSetName = CurrentSet->SetName;

	// config = Game + defaultconfig，这一句就会把改动写进 <项目>/Config/DefaultGame.ini
	Settings->TryUpdateDefaultConfigFile();

	AppendLog(FString::Printf(TEXT("已把配置 %s (%d 条规则) 写入 DefaultGame.ini"),
		*CurrentSet->SetName, Settings->PrefixRules.Num()));
}

void SAutoPrefix::PullFromProjectSettings()
{
	const UDveloperSetting_AutoPrefix* Settings = GetDefault<UDveloperSetting_AutoPrefix>();
	if (!Settings)
	{
		AppendLog(TEXT("读取失败: 拿不到 AutoPrefix 设置对象"));
		return;
	}

	TSharedPtr<FAutoPrefixSetItem> NewSet = MakeShared<FAutoPrefixSetItem>();
	NewSet->SetName = Settings->ActivePrefixSetName.IsEmpty() ? TEXT("来自项目设置") : Settings->ActivePrefixSetName;

	for (const FAutoPrefixRule& Rule : Settings->PrefixRules)
	{
		UClass* Resolved = ResolveClassByPath(Rule.TargetClass.ToString());
		if (!Resolved)
		{
			continue;
		}

		TSharedPtr<FAutoPrefixRuleItem> NewItem = MakeShared<FAutoPrefixRuleItem>();
		NewItem->TargetClass = Resolved;
		NewItem->Prefix = Rule.Prefix;
		NewSet->Rules.Add(NewItem);
	}

	PrefixSets.Add(NewSet);
	if (SetComboBox.IsValid())
	{
		SetComboBox->RefreshOptions();
	}
	SwitchToSet(NewSet);

	AppendLog(FString::Printf(TEXT("已从项目设置读回 %d 条规则（尚未保存到 JSON）"), NewSet->Rules.Num()));
}

// ============================ 规则列表界面 ============================

void SAutoPrefix::RefreshRuleUI()
{
	if (!RuleContainer.IsValid())
	{
		return;
	}

	RuleContainer->ClearChildren();

	if (!CurrentSet.IsValid())
	{
		return;
	}

	for (const TSharedPtr<FAutoPrefixRuleItem>& Rule : CurrentSet->Rules)
	{
		RuleContainer->AddSlot().AutoHeight()[CreateRuleRow(Rule)];
	}
}

void SAutoPrefix::AddRuleRow()
{
	if (!CurrentSet.IsValid())
	{
		AppendLog(TEXT("请先新建或选择一套前缀"));
		return;
	}

	TSharedPtr<FAutoPrefixRuleItem> NewRule = MakeShared<FAutoPrefixRuleItem>();
	CurrentSet->Rules.Add(NewRule);

	if (RuleContainer.IsValid())
	{
		RuleContainer->AddSlot().AutoHeight()[CreateRuleRow(NewRule)];
	}
}

TSharedRef<SWidget> SAutoPrefix::CreateRuleRow(TSharedPtr<FAutoPrefixRuleItem> Item)
{
	return SNew(SHorizontalBox)

		// 选目标类
		+ SHorizontalBox::Slot().FillWidth(0.65f).Padding(0, 2, 4, 2)
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(UObject::StaticClass())
			.AllowAbstract(true)
			.AllowNone(true)
			.ShowTreeView(true)
			.SelectedClass_Lambda([Item]() -> const UClass*
			{
				return Item.IsValid() ? Item->TargetClass.Get() : nullptr;
			})
			.OnSetClass_Lambda([Item](const UClass* NewClass)
			{
				if (Item.IsValid())
				{
					Item->TargetClass = const_cast<UClass*>(NewClass);
				}
			})
		]

		// 填前缀
		+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(0, 2, 4, 2)
		[
			SNew(SEditableTextBox)
			.HintText(LOCTEXT("PrefixHint", "如 BP_"))
			.Text_Lambda([Item]() { return FText::FromString(Item.IsValid() ? Item->Prefix : FString()); })
			.OnTextChanged_Lambda([Item](const FText& InText)
			{
				if (Item.IsValid()) { Item->Prefix = InText.ToString(); }
			})
		]

		// 删掉这一行
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveRuleBtn", "删除"))
			.ToolTipText(LOCTEXT("RemoveRuleBtnTip", "删除这条规则"))
			.OnClicked_Lambda([this, Item]()
			{
				if (CurrentSet.IsValid())
				{
					CurrentSet->Rules.Remove(Item);
					RefreshRuleUI();
				}
				return FReply::Handled();
			})
		];
}

// ============================ 配置下拉框 ============================

TSharedRef<SWidget> SAutoPrefix::OnGenerateSetComboWidget(TSharedPtr<FAutoPrefixSetItem> InItem)
{
	const FString Label = InItem.IsValid() ? InItem->SetName : TEXT("<空>");

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Label))
		]

		// 下拉项右边带个删除按钮
		+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("DeleteSetBtn", "删除"))
			.ToolTipText(LOCTEXT("DeleteSetBtnTip", "从 JSON 中删除这套前缀"))
			.OnClicked_Lambda([this, InItem]()
			{
				if (SetComboBox.IsValid())
				{
					SetComboBox->SetIsOpen(false);
				}
				DeleteSet(InItem);
				return FReply::Handled();
			})
		];
}

void SAutoPrefix::OnSetSelectionChanged(TSharedPtr<FAutoPrefixSetItem> NewSelection, ESelectInfo::Type SelectInfo)
{
	// 代码里直接切（比如删完自动跳到第一套）会走到这里，这种就不算"用户手选"，跳过日志
	if (SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid() || NewSelection == CurrentSet)
	{
		return;
	}

	SwitchToSet(NewSelection);
	AppendLog(FString::Printf(TEXT("已切换到配置 %s "), *NewSelection->SetName));
}

FText SAutoPrefix::GetCurrentSetNameText() const
{
	return FText::FromString(CurrentSet.IsValid() ? CurrentSet->SetName : TEXT("<无>"));
}

void SAutoPrefix::SwitchToSet(TSharedPtr<FAutoPrefixSetItem> Target)
{
	CurrentSet = Target;
	EditingSetName = Target.IsValid() ? Target->SetName : FString();
	RefreshRuleUI();

	if (SetComboBox.IsValid() && Target.IsValid())
	{
		SetComboBox->SetSelectedItem(Target);
	}
}

// ============================ 默认值 ============================

TArray<TSharedPtr<FAutoPrefixRuleItem>> SAutoPrefix::BuildDefaultRules() const
{
	// 一批常见的 UE 命名约定；某个类这次没加载出来就跳过它，不影响别的
	const TArray<TPair<FString, FString>> Defaults =
	{
		// ---- Gameplay 框架（Actor 系）----
		{ TEXT("/Script/Engine.Actor"),                TEXT("BP_")  },
		{ TEXT("/Script/Engine.Pawn"),                 TEXT("BP_")  },
		{ TEXT("/Script/Engine.Character"),            TEXT("CH_")  },
		{ TEXT("/Script/Engine.LevelScriptActor"),     TEXT("L_")   }, // 关卡蓝图
		{ TEXT("/Script/Engine.GameModeBase"),         TEXT("GM_")  }, // 游戏模式
		{ TEXT("/Script/Engine.GameStateBase"),        TEXT("GS_")  }, // 游戏状态
		{ TEXT("/Script/Engine.PlayerController"),     TEXT("PC_")  }, // 玩家控制器
		{ TEXT("/Script/Engine.PlayerState"),          TEXT("PS_")  }, // 玩家状态
		{ TEXT("/Script/Engine.HUD"),                  TEXT("HUD_") }, // HUD
		{ TEXT("/Script/Engine.ActorComponent"),       TEXT("BPC_") }, // Actor 组件
		{ TEXT("/Script/Engine.SceneComponent"),       TEXT("BP_")  }, // 场景组件

		// ---- Gameplay 框架（非 Actor 系）----
		{ TEXT("/Script/Engine.GameInstance"),         TEXT("GI_")  }, // 游戏实例
		{ TEXT("/Script/Engine.SaveGame"),             TEXT("SG_")  }, // 存档
		{ TEXT("/Script/Engine.DataAsset"),            TEXT("DA_")  }, // 数据资产

		// ---- UI / 动画 ----
		{ TEXT("/Script/UMG.UserWidget"),              TEXT("WBP_") }, // 用户控件（UMG）
		{ TEXT("/Script/Engine.AnimInstance"),         TEXT("ABP_") }, // 动画蓝图实例

		// ---- 资源类型 ----
		{ TEXT("/Script/Engine.Material"),             TEXT("M_")   },
		{ TEXT("/Script/Engine.MaterialInstanceConstant"),TEXT("MI_") },
		{ TEXT("/Script/Engine.MaterialFunction"),     TEXT("MF_")  },
		{ TEXT("/Script/Engine.Texture2D"),            TEXT("T_")   },
		{ TEXT("/Script/Engine.StaticMesh"),           TEXT("SM_")  },
		{ TEXT("/Script/Engine.SkeletalMesh"),         TEXT("SK_")  },
		{ TEXT("/Script/Engine.DataTable"),            TEXT("DT_")  },
		{ TEXT("/Script/Engine.SoundWave"),            TEXT("S_")   },
		{ TEXT("/Script/Engine.SoundCue"),             TEXT("SC_")  },
		{ TEXT("/Script/Engine.CurveFloat"),           TEXT("Curve_") },
	};

	TArray<TSharedPtr<FAutoPrefixRuleItem>> Rules;
	for (const TPair<FString, FString>& Pair : Defaults)
	{
		UClass* Resolved = ResolveClassByPath(Pair.Key);
		if (!Resolved)
		{
			continue;
		}

		TSharedPtr<FAutoPrefixRuleItem> Rule = MakeShared<FAutoPrefixRuleItem>();
		Rule->TargetClass = Resolved;
		Rule->Prefix = Pair.Value;
		Rules.Add(Rule);
	}

	return Rules;
}

void SAutoPrefix::SeedDefaultSet()
{
	TSharedPtr<FAutoPrefixSetItem> DefaultSet = MakeShared<FAutoPrefixSetItem>();
	DefaultSet->SetName = TEXT("默认");
	DefaultSet->bIsSaved = false;
	DefaultSet->Rules = BuildDefaultRules();

	PrefixSets.Add(DefaultSet);
}

void SAutoPrefix::ResetCurrentSetToDefaults()
{
	if (!CurrentSet.IsValid())
	{
		AppendLog(TEXT("重置失败: 当前没有选中的配置"));
		return;
	}

	CurrentSet->Rules = BuildDefaultRules();

	// 名字保留，只在内存里把规则换回内置默认；刷新界面但不自动存盘
	SwitchToSet(CurrentSet);

	AppendLog(TEXT("已将当前配置 ") + CurrentSet->SetName
		+ TEXT(" 的规则重置为内置默认值（尚未保存，点 保存配置 可持久化）"));
}

#undef LOCTEXT_NAMESPACE
