// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Selection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "UObject/Package.h"
#include "Tools/MaterialTttributeTransfer/MaterialTttributeTransfer.h"
#include "Tools/ToolUserSaveHelper.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"

namespace
{
	/** JSON 文件名：所有配置都追加在同一个文件里（仿自动前缀） */
	const TCHAR* ConfigJsonFileName = TEXT("Configurations.json");
}

#define LOCTEXT_NAMESPACE "MaterialTransferTool"
 
void SMaterialTttributeTransfer::Construct(const FArguments& InArgs)
{
    TargetSavePath = TEXT("/Game/");

    // 读取全部配置（多套）；一个配置都没有就先给一套默认
    LoadAllConfigsFromJson();
    if (Configs.Num() == 0)
    {
        SeedDefaultConfig();
    }
    CurrentConfig = Configs.Num() > 0 ? Configs[0] : nullptr;
    EditingConfigName = CurrentConfig.IsValid() ? CurrentConfig->ConfigName : TEXT("默认");
 
    ChildSlot
    [
        SNew(SVerticalBox)
 
        // 1. 配置管理（仿自动前缀：多套配置下拉 + 追加保存到同一个 JSON）
        + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
        [
            SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            [
                SNew(SVerticalBox)
                // 1.0 使用说明
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(LOCTEXT("MaterialAttributeTransferHelper",
                        "使用方法：\n"
                        "  1. 选择需要继承自的母材质或材质实例类\n"
                        "  2. 内容浏览器中选择一个或多个需要被转移参数值的材质实例或材质类\n"
                        "  3. 配置可在下拉框保存多套、随时切换；点 开始转移参数 后生成新材质并赋值参数\n"
                        ))
                ]
                // 1.1 配置下拉 + 新建空配置
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(LOCTEXT("ConfigLabel", "配置: ")).MinDesiredWidth(100) ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SAssignNew(ConfigComboBox, SComboBox<TSharedPtr<FMaterialTransferConfigItem>>)
                        .OptionsSource(&Configs)
                        .OnGenerateWidget(this, &SMaterialTttributeTransfer::OnGenerateConfigComboWidget)
                        .OnSelectionChanged(this, &SMaterialTttributeTransfer::OnConfigSelectionChanged)
                        [
                            SNew(STextBlock).Text(this, &SMaterialTttributeTransfer::GetCurrentConfigNameText)
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
                    [
                        SNew(SButton).Text(LOCTEXT("NewConfigBtn", "新建空配置"))
                        .ToolTipText(LOCTEXT("NewConfigBtnTip", "清空当前配置，从零开始配置一套新的转移方案"))
                        .OnClicked_Lambda([this]()
                        {
                            TSharedPtr<FMaterialTransferConfigItem> NewCfg = MakeShared<FMaterialTransferConfigItem>();
                            NewCfg->ConfigName = TEXT("新配置");
                            NewCfg->TargetSavePath = TEXT("/Game/");
                            NewCfg->bSaveToRespectiveFolders = true;
                            NewCfg->bIsSaved = false;
                            Configs.Add(NewCfg);
                            if (ConfigComboBox.IsValid()) { ConfigComboBox->RefreshOptions(); }
                            SwitchToConfig(NewCfg);
                            AppendLog(TEXT("已新建空配置，配置完成后记得点 保存配置"));
                            return FReply::Handled();
                        })
                    ]
                ]
                // 1.2 配置名称输入
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(LOCTEXT("ConfigNameLabel", "配置名称: ")).MinDesiredWidth(100) ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(SEditableTextBox)
                        .HintText(LOCTEXT("ConfigNameHint", "这套配置保存时使用的名字"))
                        .Text_Lambda([this]() { return FText::FromString(EditingConfigName); })
                        .OnTextChanged_Lambda([this](const FText& InText) { EditingConfigName = InText.ToString(); })
                    ]
                ]
                // 1.3 操作按钮：保存 / 重新加载 / 打开配置
                + SVerticalBox::Slot().AutoHeight().Padding(5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton).Text(LOCTEXT("SaveConfigBtn", "保存配置"))
                        .ToolTipText(LOCTEXT("SaveConfigBtnTip", "把当前配置追加保存到 JSON，同名则覆盖该配置"))
                        .OnClicked_Lambda([this]() { SaveCurrentConfigToJson(); return FReply::Handled(); })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
                    [
                        SNew(SButton).Text(LOCTEXT("ReloadBtn", "重新加载"))
                        .ToolTipText(LOCTEXT("ReloadBtnTip", "丢弃当前修改，从 JSON 重新读取全部配置"))
                        .OnClicked_Lambda([this]()
                        {
                            LoadAllConfigsFromJson();
                            if (Configs.Num() == 0) SeedDefaultConfig();
                            CurrentConfig = Configs.Num() > 0 ? Configs[0] : nullptr;
                            EditingConfigName = CurrentConfig.IsValid() ? CurrentConfig->ConfigName : TEXT("默认");
                            if (ConfigComboBox.IsValid())
                            {
                                ConfigComboBox->RefreshOptions();
                                if (CurrentConfig.IsValid()) ConfigComboBox->SetSelectedItem(CurrentConfig);
                            }
                            SwitchToConfig(CurrentConfig);
                            AppendLog(FString::Printf(TEXT("已从 JSON 重新加载，共 %d 套配置"), Configs.Num()));
                            return FReply::Handled();
                        })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
                    [
                        SNew(SButton).Text(LOCTEXT("OpenFolderBtn", "打开配置"))
                        .OnClicked_Lambda([this]() {
                            FString Dir = GetSaveDirectory();
                            IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
                            if (!PF.DirectoryExists(*Dir)) PF.CreateDirectoryTree(*Dir);
                            FString NativeDir = FPaths::ConvertRelativePathToFull(Dir).Replace(TEXT("/"), TEXT("\\"));
                            FPlatformProcess::ExploreFolder(*NativeDir);
                            AppendLog(TEXT("已打开配置文件夹: ") + NativeDir);
                            return FReply::Handled();
                        })
                        .ToolTipText(LOCTEXT("OpenFolderBtnTip", "打开配置文件夹"))
                    ]
                ]
            ]
        ]
 
        // 2. 母材质选择器
        + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ SNew(STextBlock).Text(LOCTEXT("TargetLabel", "目标母材质: ")).MinDesiredWidth(100) ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SObjectPropertyEntryBox)
                .AllowedClass(UMaterialInterface::StaticClass())
                .OnObjectChanged(this, &SMaterialTttributeTransfer::OnMasterMaterialChanged)
                .ObjectPath(this, &SMaterialTttributeTransfer::GetMasterMaterialPath)
                .DisplayThumbnail(true)
            ]
        ]
 
        // 3. 转移目标保存路径
        + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
        [
            SNew(SVerticalBox)
            // 3.1 路径输入行
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ SNew(STextBlock).Text(LOCTEXT("PathLabel", "生成保存路径: ")).MinDesiredWidth(100) ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SEditableTextBox)
                    .HintText(LOCTEXT("PathHint", "例如 /Game/Materials/Generated"))
                    .Text_Lambda([this](){ return FText::FromString(TargetSavePath); })
                    .OnTextChanged_Lambda([this](const FText& T){ TargetSavePath = T.ToString(); })
                    .IsEnabled_Lambda([this](){ return !bSaveToRespectiveFolders; })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
                [
                    SNew(SButton).Text(LOCTEXT("GetPathBtn", "获取当前路径"))
                    .OnClicked_Lambda([this](){ UpdateCurrentPathFromContentBrowser(); return FReply::Handled(); })
                    .IsEnabled_Lambda([this](){ return !bSaveToRespectiveFolders; })
                ]
            ]
            // 3.2 保存到各自文件夹开关（默认打钩）
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() { return bSaveToRespectiveFolders ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bSaveToRespectiveFolders = (NewState == ECheckBoxState::Checked); })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
                [ SNew(STextBlock).Text(LOCTEXT("SaveToFolderTip", "保存到各自文件夹中")) ]
            ]
        ]
 
        
        + SVerticalBox::Slot().AutoHeight().Padding(10, 0, 10, 5)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() { return bForceGenerateMaterial ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                    {
                        bForceGenerateMaterial = (NewState == ECheckBoxState::Checked);
                        if (bForceGenerateMaterial) bForceGenerateInstance = false; 
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 20, 0)
                [ SNew(STextBlock).Text(LOCTEXT("GenMatChk", "转换后统一生成材质类")) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() { return bForceGenerateInstance ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                    {
                        bForceGenerateInstance = (NewState == ECheckBoxState::Checked);
                        if (bForceGenerateInstance) bForceGenerateMaterial = false; 
                    })
                    
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
                [ SNew(STextBlock).Text(LOCTEXT("GenInstChk", "转换后统一生成材质实例")) ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CkeckTip", "两者都不勾选：按源材质类型生成（实例→实例，材质类→材质类）。"))
            ]
           
        ]

        // 4. 参数映射列表
        + SVerticalBox::Slot().FillHeight(0.6f).Padding(10, 5)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 2)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(2)[SNew(STextBlock).Text(LOCTEXT("TitleL", "母材质参数名 (新)"))]
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(2)[SNew(STextBlock).Text(LOCTEXT("TitleR", "源材质变量名 (旧)"))]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot() [ SAssignNew(MappingContainer, SVerticalBox) ]
            ]
        ]
 
        // 5. 日志窗口 (修复滚动问题)
        + SVerticalBox::Slot().FillHeight(0.3f).Padding(10, 5)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(LOCTEXT("LogTitle", "执行日志:")) ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SAssignNew(LogScrollBox, SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(LogWindow, SMultiLineEditableText)
                        .IsReadOnly(true)
                        .Text_Lambda([this](){ return LogContent; })
                        .AutoWrapText(true)
                    ]
                ]
            ]
        ]
 
        // 6. 操作按钮
        + SVerticalBox::Slot().AutoHeight().Padding(10)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton).Text(LOCTEXT("AddBtn", "添加参数行"))
                .OnClicked_Lambda([this]() { AddMappingRow(); return FReply::Handled(); })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
            [
                SNew(SButton).Text(LOCTEXT("ClearBtn", "清空参数行"))
                .OnClicked_Lambda([this]() { ClearAllMappings(); return FReply::Handled(); })
                .ToolTipText(LOCTEXT("ClearBtnTip", "移除所有映射行"))
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
            [
                SNew(SButton).Text(LOCTEXT("RunBtn", "开始转移参数"))
                .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                .OnClicked(this, &SMaterialTttributeTransfer::OnExecuteTransfer)
                .IsEnabled_Lambda([this]() { return TargetMasterMaterial.IsValid(); })
            ]
        ]
    ];

    if (ConfigComboBox.IsValid() && CurrentConfig.IsValid())
    {
        ConfigComboBox->SetSelectedItem(CurrentConfig);
    }
    SwitchToConfig(CurrentConfig);
    AppendLog(FString::Printf(TEXT("已载入 %d 套配置，当前: %s"), Configs.Num(),
        CurrentConfig.IsValid() ? *CurrentConfig->ConfigName : TEXT("无")));
}
 
void SMaterialTttributeTransfer::AppendLog(const FString& InLog)
{
    FString NewLine = FDateTime::Now().ToString(TEXT("[%H:%M:%S] ")) + InLog + TEXT("\n");
    LogContent = FText::FromString(LogContent.ToString() + NewLine);
    
    // 强制 UI 更新并在下一帧滚动到底部
    if (LogScrollBox.IsValid())
    {
        LogScrollBox->ScrollToEnd();
    }
}
 
void SMaterialTttributeTransfer::UpdateCurrentPathFromContentBrowser()
{
    FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FString> SelectedPaths;
    CBModule.Get().GetSelectedPathViewFolders(SelectedPaths);
    
    if (SelectedPaths.Num() > 0)
    {
        FString Path = SelectedPaths[0];
        

        if (Path.StartsWith(TEXT("/All")))
        {
            Path.RemoveFromStart(TEXT("/All"));
        }
        
        // 如果裁剪后变空了（说明选中的是根目录），设为 /Game
        if (Path.IsEmpty() || Path == TEXT("/")) 
        {
            Path = TEXT("/Game");
        }
 
        TargetSavePath = Path;
        AppendLog(FString::Printf(TEXT("路径已获取并修正: %s"), *TargetSavePath));
    }
}
 
TSharedRef<SWidget> SMaterialTttributeTransfer::CreateMappingRowWidget(TSharedPtr<FParamMappingPair> InPair)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
        [
            SNew(SEditableTextBox)
            .Text_Lambda([InPair]() { return FText::FromString(InPair->TargetParamName); })
            .OnTextCommitted_Lambda([InPair](const FText& T, ETextCommit::Type) { InPair->TargetParamName = T.ToString(); })
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
        [
            SNew(SEditableTextBox)
            .Text_Lambda([InPair]() { return FText::FromString(InPair->SourceParamName); })
            .OnTextCommitted_Lambda([InPair](const FText& T, ETextCommit::Type) { InPair->SourceParamName = T.ToString(); })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2, 2, 0, 2)
        [
            SNew(SButton)
            .Text(LOCTEXT("RowDelBtn", "删除"))
            .ButtonStyle(FAppStyle::Get(), "Button")
            .ContentPadding(FMargin(4, 2))
            .OnClicked_Lambda([this, InPair]() { RemoveMappingRow(InPair); return FReply::Handled(); })
            .ToolTipText(LOCTEXT("RowDelTip", "删除这一行映射"))
        ];
}
 
void SMaterialTttributeTransfer::LoadConfigDataFromCurrent()
{
    if (!CurrentConfig.IsValid())
    {
        MappingList.Empty();
        TargetMasterMaterial = nullptr;
        TargetSavePath = TEXT("/Game/");
        bSaveToRespectiveFolders = true;
        bForceGenerateMaterial = false;
        bForceGenerateInstance = false;
        RefreshMappingUI();
        return;
    }

    TargetMasterMaterial = CurrentConfig->MasterMaterialPath.IsEmpty()
        ? nullptr
        : Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *CurrentConfig->MasterMaterialPath));
    TargetSavePath = CurrentConfig->TargetSavePath.IsEmpty() ? TEXT("/Game/") : CurrentConfig->TargetSavePath;
    bSaveToRespectiveFolders = CurrentConfig->bSaveToRespectiveFolders;
    bForceGenerateMaterial = CurrentConfig->bForceGenerateMaterial;
    bForceGenerateInstance = CurrentConfig->bForceGenerateInstance;

    MappingList = DeepCopyMappings(CurrentConfig->Mappings);
    if (MappingList.Num() == 0) AddMappingRow();
    else RefreshMappingUI();
}

void SMaterialTttributeTransfer::WriteConfigDataToCurrent()
{
    if (!CurrentConfig.IsValid()) return;
    CurrentConfig->MasterMaterialPath = TargetMasterMaterial.IsValid() ? TargetMasterMaterial->GetPathName() : TEXT("");
    CurrentConfig->TargetSavePath = TargetSavePath;
    CurrentConfig->bSaveToRespectiveFolders = bSaveToRespectiveFolders;
    CurrentConfig->bForceGenerateMaterial = bForceGenerateMaterial;
    CurrentConfig->bForceGenerateInstance = bForceGenerateInstance;
    CurrentConfig->Mappings = DeepCopyMappings(MappingList);
}

TArray<TSharedPtr<FParamMappingPair>> SMaterialTttributeTransfer::DeepCopyMappings(const TArray<TSharedPtr<FParamMappingPair>>& InMappings) const
{
    TArray<TSharedPtr<FParamMappingPair>> Out;
    for (const TSharedPtr<FParamMappingPair>& Pair : InMappings)
    {
        if (!Pair.IsValid()) continue;
        TSharedPtr<FParamMappingPair> Cloned = MakeShared<FParamMappingPair>();
        Cloned->TargetParamName = Pair->TargetParamName;
        Cloned->SourceParamName = Pair->SourceParamName;
        Out.Add(Cloned);
    }
    return Out;
}

void SMaterialTttributeTransfer::LoadAllConfigsFromJson()
{
    // 升级兼容：把老版本直接放在根目录的 Configurations.json 搬到新的工具子目录
    FToolUserSave::MigrateLegacyFile(TEXT("MaterialAttributeTransfer"), ConfigJsonFileName);
    Configs.Empty();

    FString JsonString;
    bool bLoaded = FFileHelper::LoadFileToString(JsonString, *GetConfigJsonPath());
    if (bLoaded)
    {
        TSharedPtr<FJsonObject> RootObject;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* ConfigsArray = nullptr;
            if (RootObject->TryGetArrayField(TEXT("Configs"), ConfigsArray) && ConfigsArray)
            {
                for (const TSharedPtr<FJsonValue>& Val : *ConfigsArray)
                {
                    const TSharedPtr<FJsonObject>* Obj = nullptr;
                    if (!Val.IsValid() || !Val->TryGetObject(Obj) || !Obj) continue;

                    TSharedPtr<FMaterialTransferConfigItem> NewCfg = MakeShared<FMaterialTransferConfigItem>();
                    NewCfg->ConfigName = (*Obj)->GetStringField(TEXT("Name"));
                    NewCfg->MasterMaterialPath = (*Obj)->GetStringField(TEXT("MasterMaterial"));
                    NewCfg->TargetSavePath = (*Obj)->GetStringField(TEXT("TargetSavePath"));
                    if (NewCfg->TargetSavePath.IsEmpty()) NewCfg->TargetSavePath = TEXT("/Game/");
                    if ((*Obj)->HasField(TEXT("SaveToRespectiveFolders")))
                        NewCfg->bSaveToRespectiveFolders = (*Obj)->GetBoolField(TEXT("SaveToRespectiveFolders"));
                    if ((*Obj)->HasField(TEXT("ForceGenerateMaterial")))
                        NewCfg->bForceGenerateMaterial = (*Obj)->GetBoolField(TEXT("ForceGenerateMaterial"));
                    if ((*Obj)->HasField(TEXT("ForceGenerateInstance")))
                        NewCfg->bForceGenerateInstance = (*Obj)->GetBoolField(TEXT("ForceGenerateInstance"));

                    const TArray<TSharedPtr<FJsonValue>>* MappingsArray = nullptr;
                    if ((*Obj)->TryGetArrayField(TEXT("Mappings"), MappingsArray) && MappingsArray)
                    {
                        for (const TSharedPtr<FJsonValue>& M : *MappingsArray)
                        {
                            const TSharedPtr<FJsonObject>* MObj = nullptr;
                            if (!M.IsValid() || !M->TryGetObject(MObj) || !MObj) continue;
                            TSharedPtr<FParamMappingPair> Pair = MakeShared<FParamMappingPair>();
                            Pair->TargetParamName = (*MObj)->GetStringField(TEXT("T"));
                            Pair->SourceParamName = (*MObj)->GetStringField(TEXT("S"));
                            NewCfg->Mappings.Add(Pair);
                        }
                    }
                    NewCfg->bIsSaved = true;   // 能从 JSON 读出来的，说明之前已经存过了
                    Configs.Add(NewCfg);
                }
            }
        }
    }

    // 新文件为空/不存在：尝试把老版本"每个配置一个独立文件"的 DefaultSettings.json 导入为新格式第一套
    if (Configs.Num() == 0)
    {
        const FString LegacyPath = GetSaveDirectory() + TEXT("DefaultSettings.json");
        FString LegacyJson;
        if (FFileHelper::LoadFileToString(LegacyJson, *LegacyPath))
        {
            TSharedPtr<FJsonObject> RootObject;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LegacyJson);
            if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
            {
                TSharedPtr<FMaterialTransferConfigItem> NewCfg = MakeShared<FMaterialTransferConfigItem>();
                NewCfg->ConfigName = TEXT("默认");
                NewCfg->MasterMaterialPath = RootObject->GetStringField(TEXT("MasterMaterial"));
                NewCfg->TargetSavePath = RootObject->GetStringField(TEXT("TargetSavePath"));
                if (NewCfg->TargetSavePath.IsEmpty()) NewCfg->TargetSavePath = TEXT("/Game/");
                if (RootObject->HasField(TEXT("SaveToRespectiveFolders")))
                    NewCfg->bSaveToRespectiveFolders = RootObject->GetBoolField(TEXT("SaveToRespectiveFolders"));
                if (RootObject->HasField(TEXT("ForceGenerateMaterial")))
                    NewCfg->bForceGenerateMaterial = RootObject->GetBoolField(TEXT("ForceGenerateMaterial"));
                if (RootObject->HasField(TEXT("ForceGenerateInstance")))
                    NewCfg->bForceGenerateInstance = RootObject->GetBoolField(TEXT("ForceGenerateInstance"));

                const TArray<TSharedPtr<FJsonValue>>* MappingsArray = nullptr;
                if (RootObject->TryGetArrayField(TEXT("Mappings"), MappingsArray) && MappingsArray)
                {
                    for (const auto& M : *MappingsArray)
                    {
                        const TSharedPtr<FJsonObject>* MObj = nullptr;
                        if (M.IsValid() && M->TryGetObject(MObj) && MObj)
                        {
                            TSharedPtr<FParamMappingPair> Pair = MakeShared<FParamMappingPair>();
                            Pair->TargetParamName = (*MObj)->GetStringField(TEXT("T"));
                            Pair->SourceParamName = (*MObj)->GetStringField(TEXT("S"));
                            NewCfg->Mappings.Add(Pair);
                        }
                    }
                }
                NewCfg->bIsSaved = true;
                Configs.Add(NewCfg);

                // 写回新格式后删除旧文件，避免遗留
                if (WriteAllConfigsToJson())
                {
                    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
                    PF.DeleteFile(*LegacyPath);
                }
            }
        }
    }
}

bool SMaterialTttributeTransfer::WriteAllConfigsToJson()
{
    const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ConfigsArray;

    for (const TSharedPtr<FMaterialTransferConfigItem>& Cfg : Configs)
    {
        if (!Cfg.IsValid()) continue;

        const TSharedRef<FJsonObject> CfgObj = MakeShared<FJsonObject>();
        CfgObj->SetStringField(TEXT("Name"), Cfg->ConfigName);
        CfgObj->SetStringField(TEXT("MasterMaterial"), Cfg->MasterMaterialPath);
        CfgObj->SetStringField(TEXT("TargetSavePath"), Cfg->TargetSavePath);
        CfgObj->SetBoolField(TEXT("SaveToRespectiveFolders"), Cfg->bSaveToRespectiveFolders);
        CfgObj->SetBoolField(TEXT("ForceGenerateMaterial"), Cfg->bForceGenerateMaterial);
        CfgObj->SetBoolField(TEXT("ForceGenerateInstance"), Cfg->bForceGenerateInstance);

        TArray<TSharedPtr<FJsonValue>> MappingsArray;
        for (const TSharedPtr<FParamMappingPair>& Pair : Cfg->Mappings)
        {
            if (!Pair.IsValid()) continue;
            const TSharedRef<FJsonObject> PairObj = MakeShared<FJsonObject>();
            PairObj->SetStringField(TEXT("T"), Pair->TargetParamName);
            PairObj->SetStringField(TEXT("S"), Pair->SourceParamName);
            MappingsArray.Add(MakeShared<FJsonValueObject>(PairObj));
        }
        CfgObj->SetArrayField(TEXT("Mappings"), MappingsArray);

        ConfigsArray.Add(MakeShared<FJsonValueObject>(CfgObj));
    }

    RootObject->SetArrayField(TEXT("Configs"), ConfigsArray);

    FString OutputString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (!FJsonSerializer::Serialize(RootObject, Writer)) return false;

    return FFileHelper::SaveStringToFile(OutputString, *GetConfigJsonPath());
}

void SMaterialTttributeTransfer::SaveCurrentConfigToJson()
{
    if (!CurrentConfig.IsValid())
    {
        AppendLog(TEXT("保存失败: 当前没有可保存的配置"));
        return;
    }

    const FString TrimmedName = EditingConfigName.TrimStartAndEnd();
    if (TrimmedName.IsEmpty())
    {
        AppendLog(TEXT("保存失败: 配置名称不能为空"));
        return;
    }

    // 先看是不是已经有一套同名（不区分大小写）
    TSharedPtr<FMaterialTransferConfigItem> Existing;
    for (const TSharedPtr<FMaterialTransferConfigItem>& Cfg : Configs)
    {
        if (Cfg.IsValid() && Cfg->ConfigName.Equals(TrimmedName, ESearchCase::IgnoreCase))
        {
            Existing = Cfg;
            break;
        }
    }

    // 先把当前面板数据写回当前配置对象
    WriteConfigDataToCurrent();

    if (Existing.IsValid())
    {
        // 同名：直接覆盖这套的内容（保留它这个对象，不产生重复项）
        Existing->MasterMaterialPath = CurrentConfig->MasterMaterialPath;
        Existing->TargetSavePath = CurrentConfig->TargetSavePath;
        Existing->bSaveToRespectiveFolders = CurrentConfig->bSaveToRespectiveFolders;
        Existing->bForceGenerateMaterial = CurrentConfig->bForceGenerateMaterial;
        Existing->bForceGenerateInstance = CurrentConfig->bForceGenerateInstance;
        Existing->Mappings = DeepCopyMappings(CurrentConfig->Mappings);
        Existing->bIsSaved = true;

        // 当前正在编的是另一套"还没存过"的（比如默认/新建空配置），已经被合并，从列表移除免得留空壳
        if (CurrentConfig != Existing && !CurrentConfig->bIsSaved)
        {
            Configs.Remove(CurrentConfig);
        }
        CurrentConfig = Existing;
    }
    else if (!CurrentConfig->bIsSaved)
    {
        // 当前是"还没存过"的新建/默认配置：直接给它起这个名字存，不重复追加
        CurrentConfig->ConfigName = TrimmedName;
        CurrentConfig->bIsSaved = true;
        if (!Configs.Contains(CurrentConfig)) Configs.Add(CurrentConfig);
    }
    else
    {
        // 当前这套已经存过、但这次换了个新名字：就在文件里另存一套
        TSharedPtr<FMaterialTransferConfigItem> NewCfg = MakeShared<FMaterialTransferConfigItem>();
        NewCfg->ConfigName = TrimmedName;
        NewCfg->MasterMaterialPath = CurrentConfig->MasterMaterialPath;
        NewCfg->TargetSavePath = CurrentConfig->TargetSavePath;
        NewCfg->bSaveToRespectiveFolders = CurrentConfig->bSaveToRespectiveFolders;
        NewCfg->bForceGenerateMaterial = CurrentConfig->bForceGenerateMaterial;
        NewCfg->bForceGenerateInstance = CurrentConfig->bForceGenerateInstance;
        NewCfg->Mappings = DeepCopyMappings(CurrentConfig->Mappings);
        NewCfg->bIsSaved = true;
        Configs.Add(NewCfg);
        CurrentConfig = NewCfg;
    }

    if (ConfigComboBox.IsValid()) { ConfigComboBox->RefreshOptions(); }
    SwitchToConfig(CurrentConfig);

    if (WriteAllConfigsToJson())
    {
        AppendLog(FString::Printf(TEXT("已保存配置 %s ，文件内共 %d 套 -> %s"),
            *CurrentConfig->ConfigName, Configs.Num(), *GetConfigJsonPath()));
    }
    else
    {
        AppendLog(TEXT("保存失败: 无法写入 ") + GetConfigJsonPath());
    }
}

void SMaterialTttributeTransfer::DeleteConfig(TSharedPtr<FMaterialTransferConfigItem> Target)
{
    if (!Target.IsValid()) return;

    const FString RemovedName = Target->ConfigName;
    Configs.Remove(Target);

    // 删的要是正在编的这套，就自动切到第一套（空了则给一套默认）
    if (CurrentConfig == Target)
    {
        if (Configs.Num() > 0)
        {
            SwitchToConfig(Configs[0]);
        }
        else
        {
            CurrentConfig = nullptr;
            SeedDefaultConfig();
        }
    }

    if (WriteAllConfigsToJson())
    {
        AppendLog(FString::Printf(TEXT("已删除配置 %s ，剩余 %d 套"), *RemovedName, Configs.Num()));
    }
    else
    {
        AppendLog(TEXT("删除后写回 JSON 失败"));
    }

    if (ConfigComboBox.IsValid())
    {
        ConfigComboBox->RefreshOptions();
        if (CurrentConfig.IsValid()) ConfigComboBox->SetSelectedItem(CurrentConfig);
    }
}

void SMaterialTttributeTransfer::SeedDefaultConfig()
{
    TSharedPtr<FMaterialTransferConfigItem> Def = MakeShared<FMaterialTransferConfigItem>();
    Def->ConfigName = TEXT("默认");
    Def->TargetSavePath = TEXT("/Game/");
    Def->bSaveToRespectiveFolders = true;
    Def->bIsSaved = false;
    Configs.Add(Def);
    SwitchToConfig(Def);
}

// ============================ 配置下拉框 ============================

TSharedRef<SWidget> SMaterialTttributeTransfer::OnGenerateConfigComboWidget(TSharedPtr<FMaterialTransferConfigItem> InItem)
{
    const FString Label = InItem.IsValid() ? InItem->ConfigName : TEXT("<空>");

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [ SNew(STextBlock).Text(FText::FromString(Label)) ]
        // 下拉项右边带个删除按钮
        + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
        [
            SNew(SButton)
            .Text(LOCTEXT("DeleteConfigBtn", "删除"))
            .ToolTipText(LOCTEXT("DeleteConfigBtnTip", "从 JSON 中删除这套配置"))
            .OnClicked_Lambda([this, InItem]()
            {
                if (ConfigComboBox.IsValid()) { ConfigComboBox->SetIsOpen(false); }
                DeleteConfig(InItem);
                return FReply::Handled();
            })
        ];
}

void SMaterialTttributeTransfer::OnConfigSelectionChanged(TSharedPtr<FMaterialTransferConfigItem> NewSelection, ESelectInfo::Type SelectInfo)
{
    // 代码里直接切（比如删完自动跳到第一套）会走到这里，这种就不算"用户手选"，跳过日志
    if (SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid() || NewSelection == CurrentConfig) return;

    SwitchToConfig(NewSelection);
    AppendLog(FString::Printf(TEXT("已切换到配置 %s"), *NewSelection->ConfigName));
}

FText SMaterialTttributeTransfer::GetCurrentConfigNameText() const
{
    return FText::FromString(CurrentConfig.IsValid() ? CurrentConfig->ConfigName : TEXT("<无>"));
}

void SMaterialTttributeTransfer::SwitchToConfig(TSharedPtr<FMaterialTransferConfigItem> Target)
{
    CurrentConfig = Target;
    EditingConfigName = Target.IsValid() ? Target->ConfigName : FString();
    LoadConfigDataFromCurrent();

    if (ConfigComboBox.IsValid() && Target.IsValid())
    {
        ConfigComboBox->SetSelectedItem(Target);
    }
}
 
	FReply SMaterialTttributeTransfer::OnExecuteTransfer()
{
    // 1. 【核心修复】直接从内容浏览器获取选中的资产数据
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FAssetData> SelectedAssetsData;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssetsData);
 
    // 过滤出材质类资产
    TArray<UMaterialInterface*> SelectedMaterials;
    for (const FAssetData& AssetData : SelectedAssetsData)
    {
        if (UMaterialInterface* Mat = Cast<UMaterialInterface>(AssetData.GetAsset()))
        {
            SelectedMaterials.Add(Mat);
        }
    }
 
    if (SelectedMaterials.Num() == 0)
    {
        AppendLog(TEXT("错误: 未在内容浏览器选中任何有效的材质或材质实例！"));
        return FReply::Handled();
    }
 
    if (!TargetMasterMaterial.IsValid())
    {
        AppendLog(TEXT("错误: 请先在工具上方选择目标母材质！"));
        return FReply::Handled();
    }
 
    // 2. 保存路径模式 & 输出类型
    const bool bSaveToRespective = bSaveToRespectiveFolders;

    FString OutputModeStr;
    if (bForceGenerateInstance) OutputModeStr = TEXT("统一材质实例");
    else if (bForceGenerateMaterial) OutputModeStr = TEXT("统一材质类");
    else OutputModeStr = TEXT("按源材质类型");

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    AppendLog(FString::Printf(
        TEXT("开始处理... 选中数量: %d, 保存模式: %s, 输出类型: %s"),
        SelectedMaterials.Num(),
        bSaveToRespective ? TEXT("各自源文件夹") : *TargetSavePath,
        *OutputModeStr));
 
    int32 SuccessCount = 0;
    int32 SkipCount = 0;
 
    // 3. 循环处理
    for (UMaterialInterface* SourceMat : SelectedMaterials)
    {
        // 排除母材质本身
        if (SourceMat == TargetMasterMaterial.Get())
        {
            AppendLog(FString::Printf(TEXT("跳过: %s (它是母材质本身)"), *SourceMat->GetName()));
            SkipCount++;
            continue;
        }

        // 计算本次生成的目标路径
        FString FinalPath = TargetSavePath.TrimStartAndEnd();
        if (bSaveToRespective)
        {
            // 使用源材质所在的包路径，生成的 MIC 保存到各自的文件夹中
            FinalPath = FPaths::GetPath(SourceMat->GetPathName());
        }
        else
        {
            if (FinalPath.StartsWith(TEXT("/All")))
            {
                FinalPath.RemoveFromStart(TEXT("/All"));
            }

            // 统一处理斜杠
            while (FinalPath.StartsWith(TEXT("/"))) { FinalPath.RemoveFromStart(TEXT("/")); }
            while (FinalPath.EndsWith(TEXT("/"))) { FinalPath.RemoveFromEnd(TEXT("/")); }

            if (FinalPath.StartsWith(TEXT("Game")))
            {
                FinalPath = TEXT("/") + FinalPath;
            }
            else
            {
                FinalPath = FinalPath.IsEmpty() ? TEXT("/Game") : TEXT("/Game/") + FinalPath;
            }
        }

        // 确保路径中没有 //
        FinalPath.ReplaceInline(TEXT("//"), TEXT("/"));

        // 决定本次输出类型
        enum class EOutputType { MatchSource, Instance, Material };
        EOutputType OutType = bForceGenerateInstance ? EOutputType::Instance
            : (bForceGenerateMaterial ? EOutputType::Material : EOutputType::MatchSource);
        if (OutType == EOutputType::MatchSource)
        {
            // 按源材质类型：材质实例 → 实例；材质类 → 材质类
            OutType = Cast<UMaterialInstanceConstant>(SourceMat) ? EOutputType::Instance : EOutputType::Material;
        }

        if (OutType == EOutputType::Instance)
        {
            // 统一/匹配生成材质实例
            FString NewAssetName = SourceMat->GetName() + TEXT("_INST");
            MakeUniqueAssetName(FinalPath, NewAssetName, NewAssetName);

            UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
            UObject* NewAsset = AssetTools.CreateAsset(NewAssetName, FinalPath, UMaterialInstanceConstant::StaticClass(), Factory);
            UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(NewAsset);

            if (NewMIC)
            {
                NewMIC->SetParentEditorOnly(TargetMasterMaterial.Get());
                ApplyParameterValues(NewMIC, SourceMat);
                NewMIC->PostEditChange();
                FAssetRegistryModule::AssetCreated(NewMIC);

                SuccessCount++;
                AppendLog(FString::Printf(TEXT("成功生成(实例): %s -> %s"), *NewAssetName, *FinalPath));
            }
            else
            {
                AppendLog(FString::Printf(TEXT("失败: 无法在路径 %s 下创建资产 %s"), *FinalPath, *NewAssetName));
            }
        }
        else
        {
            // 统一/匹配生成材质类：复制母材质（基础材质）并烘焙参数值
            UMaterial* Template = FindBaseMaterialTemplate(TargetMasterMaterial.Get());
            if (!Template)
            {
                AppendLog(FString::Printf(
                    TEXT("跳过: %s (统一生成材质类需要母材质为材质类，或能向上追溯到材质类)"), *SourceMat->GetName()));
                SkipCount++;
                continue;
            }

            FString NewAssetName = SourceMat->GetName() + TEXT("_MAT");
            MakeUniqueAssetName(FinalPath, NewAssetName, NewAssetName);

            FString PackagePath = FinalPath / NewAssetName;
            UPackage* Pkg = CreatePackage(*PackagePath);
            UMaterial* NewMat = DuplicateObject<UMaterial>(Template, Pkg, *NewAssetName);
            if (NewMat)
            {
                NewMat->SetFlags(RF_Public | RF_Standalone);
                ApplyParameterValues(NewMat, SourceMat);
                NewMat->PostEditChange();
                NewMat->MarkPackageDirty();
                FAssetRegistryModule::AssetCreated(NewMat);

                SuccessCount++;
                AppendLog(FString::Printf(TEXT("成功生成(材质类): %s -> %s"), *NewAssetName, *FinalPath));
            }
            else
            {
                AppendLog(FString::Printf(TEXT("失败: 无法在路径 %s 下复制材质 %s"), *FinalPath, *NewAssetName));
            }
        }
    }
 
    AppendLog(FString::Printf(TEXT("任务完成！成功: %d, 跳过: %d"), SuccessCount, SkipCount));
    UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);
 
    return FReply::Handled();
}
 
UMaterial* SMaterialTttributeTransfer::FindBaseMaterialTemplate(UMaterialInterface* InMat) const
{
    UMaterialInterface* Cur = InMat;
    while (Cur)
    {
        if (UMaterial* Mat = Cast<UMaterial>(Cur))
        {
            return Mat;
        }
        // 材质实例可向上追溯到其父材质
        UMaterialInstance* MI = Cast<UMaterialInstance>(Cur);
        Cur = MI ? MI->Parent.Get() : nullptr;
    }
    return nullptr;
}

void SMaterialTttributeTransfer::MakeUniqueAssetName(const FString& PackagePath, const FString& BaseName, FString& OutName) const
{
    OutName = BaseName;
    int32 Suffix = 1;
    bool bExists = false;
    do
    {
        FString Full = PackagePath / OutName;
        bExists = (FindObject<UPackage>(nullptr, *Full) != nullptr)
               || FPackageName::DoesPackageExist(Full);
        if (bExists)
        {
            OutName = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++);
        }
    } while (bExists);
}

void SMaterialTttributeTransfer::ApplyParameterValues(UMaterialInterface* Target, UMaterialInterface* Source) const
{
    for (const TSharedPtr<FParamMappingPair>& Mapping : MappingList)
    {
        if (Mapping->TargetParamName.IsEmpty() || Mapping->SourceParamName.IsEmpty()) continue;

        FName DestName(*Mapping->TargetParamName);
        FName SrcName(*Mapping->SourceParamName);

        // 转移 Texture
        UTexture* SourceTex = nullptr;
        if (Source->GetTextureParameterValue(SrcName, SourceTex))
        {
            if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Target)) MIC->SetTextureParameterValueEditorOnly(DestName, SourceTex);
            else if (UMaterial* Mat = Cast<UMaterial>(Target)) Mat->SetTextureParameterValueEditorOnly(DestName, SourceTex);
        }

        // 转移 Scalar
        float SourceScalar = 0.f;
        if (Source->GetScalarParameterValue(SrcName, SourceScalar))
        {
            if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Target)) MIC->SetScalarParameterValueEditorOnly(DestName, SourceScalar);
            else if (UMaterial* Mat = Cast<UMaterial>(Target)) Mat->SetScalarParameterValueEditorOnly(DestName, SourceScalar);
        }

        // 转移 Vector
        FLinearColor SourceVector;
        if (Source->GetVectorParameterValue(SrcName, SourceVector))
        {
            if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Target)) MIC->SetVectorParameterValueEditorOnly(DestName, SourceVector);
            else if (UMaterial* Mat = Cast<UMaterial>(Target)) Mat->SetVectorParameterValueEditorOnly(DestName, SourceVector);
        }
    }
}

FString SMaterialTttributeTransfer::GetSaveDirectory() const {
    // 每个工具在自己的子目录里存文件，互不干扰；目录不存在会被自动建好
    return FToolUserSave::GetToolSaveDir(TEXT("MaterialAttributeTransfer"));
}
 
FString SMaterialTttributeTransfer::GetConfigJsonPath() const {
    // 所有配置都追加在同一个 JSON 文件里（仿自动前缀）
    return GetSaveDirectory() + ConfigJsonFileName;
}
 
void SMaterialTttributeTransfer::RefreshMappingUI() {
    if (MappingContainer.IsValid()) {
        MappingContainer->ClearChildren();
        for (auto& P : MappingList) MappingContainer->AddSlot().AutoHeight()[CreateMappingRowWidget(P)];
    }
}
 
void SMaterialTttributeTransfer::AddMappingRow() {
    TSharedPtr<FParamMappingPair> NP = MakeShared<FParamMappingPair>();
    MappingList.Add(NP);
    if (MappingContainer.IsValid()) MappingContainer->AddSlot().AutoHeight()[CreateMappingRowWidget(NP)];
}

void SMaterialTttributeTransfer::RemoveMappingRow(TSharedPtr<FParamMappingPair> InPair) {
    if (!InPair.IsValid()) return;
    int32 Idx = MappingList.IndexOfByKey(InPair);
    if (Idx != INDEX_NONE) {
        MappingList.RemoveAt(Idx);
        AppendLog(FString::Printf(TEXT("已删除一条映射行，剩余 %d 行。"), MappingList.Num()));
    }
    RefreshMappingUI();
}

void SMaterialTttributeTransfer::ClearAllMappings() {
    if (MappingList.Num() == 0) {
        AppendLog(TEXT("映射行已为空，无需清空。"));
        return;
    }
    MappingList.Empty();
    AppendLog(TEXT("已清空所有映射行。"));
    RefreshMappingUI();
}
 
void SMaterialTttributeTransfer::OnMasterMaterialChanged(const FAssetData& AssetData) { TargetMasterMaterial = Cast<UMaterialInterface>(AssetData.GetAsset()); }
FString SMaterialTttributeTransfer::GetMasterMaterialPath() const { return TargetMasterMaterial.IsValid() ? TargetMasterMaterial->GetPathName() : TEXT(""); }
 
#undef LOCTEXT_NAMESPACE