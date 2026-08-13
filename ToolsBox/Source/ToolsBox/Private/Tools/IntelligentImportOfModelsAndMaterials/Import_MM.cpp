// Copyright 2026 SuBase. All Rights Reserved.

#include "Tools/IntelligentImportOfModelsAndMaterials/Import_MM.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "DesktopPlatformModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Interfaces/IMainFrameModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Slate_Assist/FIconStyle.h"
#include "Tools/IntelligentImportOfModelsAndMaterials/Data.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SImport_MM"

void SImport_MM::Construct(const FArguments& InArgs)
{
    TArray<TSharedRef<ITextDecorator>> Decorators;

    TSharedPtr<FRichTextLayoutMarshaller> LocalMarshaller = FRichTextLayoutMarshaller::Create(Decorators, &FAppStyle::Get());
   
    ChildSlot
            [
                SNew(SScrollBox)
               + SScrollBox::Slot().Padding(10)
               [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(10)
                    [
                        SNew(STextBlock).Text(LOCTEXT("Title", "批量导入工具")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
                   [
                       SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                        [
                            SNew(STextBlock)
                            .AutoWrapText(true)
                            .Text(LOCTEXT("AutoPrefixHelp",
                                "使用方法：\n"
                                "  1. 选择要导入模型和贴图的文件夹\n"
                                "  2. 点导入即可\n"
                                "  还不清楚怎么用可前往作者B站查看使用教程\n"
                                ))
			            
                        ]
                   ]
                    + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2) [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("Src", "源文件夹: ")).MinDesiredWidth(100) ]
                            + SHorizontalBox::Slot().FillWidth(1.0f) [ SAssignNew(SourcePathBox, SEditableTextBox).IsReadOnly(true) ]
                            + SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("B1", "浏览")).OnClicked(this, &SImport_MM::OnBrowseSourceClicked) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2) [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("Dst", "保存位置: ")).MinDesiredWidth(100) ]
                            + SHorizontalBox::Slot().FillWidth(1.0f) [ SAssignNew(DestPathBox, SEditableTextBox).HintText(LOCTEXT("Hint", "/Game/BatchImport/")) ]
                            + SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("B2", "选择路径")).OnClicked(this, &SImport_MM::OnBrowseDestClicked) ]
                        ]
                        
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2) [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                
                                SNew(STextBlock)
                                .Text(LOCTEXT("TexFolder", "贴图子文件夹名: ")).MinDesiredWidth(100)
                                .ToolTipText(LOCTEXT("Tip", "如果纹理贴图不与模型同一层级下，而是处于同级的文件夹内，请输入统一的此文件夹名称。留空则默认贴图与模型同级。 "))
                                
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f) [ SAssignNew(TexSubFolderNameBox, SEditableTextBox).HintText(LOCTEXT("TexHint", "留空则在模型同级目录找贴图")) ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
                    [
                        SNew(SExpandableArea)
                       .AreaTitle(LOCTEXT("NamingRules", "资产命名详细规则"))
                       .InitiallyCollapsed(true)
                       .BodyContent()
                       [
            
                           SNew(SBorder)
                           .Padding(FMargin(10, 5))
                           .BorderImage(FIconStyle::Get_Images().GetBrush("ToolsBox.Image_Anon_1K")) 
                           [
                               SNew(SVerticalBox)
                               + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EImportAssetType::Mesh, TEXT("[ 模型 ]"), TEXT("SM_")) ]
                               + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EImportAssetType::Texture, TEXT("[ 贴图 ]"), TEXT("T_")) ]
                               + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EImportAssetType::Material, TEXT("[ 母材质 ]"), TEXT("M_")) ]
                               + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EImportAssetType::Instance, TEXT("[ 材质实例 ]"), TEXT("MI_")) ]
                           ]
                       ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(5)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(bCreateMICheckbox, SCheckBox)
                            // 默认不勾选
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("CreateMI", "创建实例并应用"))
                            .ToolTipText(LOCTEXT("CreateMITip", "在母材质基础上生成材质实例"))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(5)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(bUseCombinedNamePrefixCheckbox, SCheckBox)
                            .IsChecked(ECheckBoxState::Checked) // 默认勾选，保持当前行为
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(LOCTEXT("UseCombinedPrefix", "拆分模型添加合并模型名前缀"))
                            .ToolTipText(LOCTEXT("UseCombinedPrefixTip", "勾选时，合并模型拆分后的每个子模型会加上合并模型名作为前缀（如 SM_Chair_Seat）；取消勾选则只保留子模型原名（如 SM_Seat）"))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                    [
                    SNew(SBorder).BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SAssignNew(bUseParentMICheckbox, SCheckBox)
                                .OnCheckStateChanged(this, &SImport_MM::OnUseParentMIToggled)
                                // 【关键逻辑】如果勾选了“自动创建实例”，则此项变暗禁用
                                .IsEnabled_Lambda([this](){ return !bCreateMICheckbox->IsChecked(); }) 
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("UseMI", "使用父材质实例"))
                                .ToolTipText(LOCTEXT("UseMITip", "从已有父材质实例复制并设置贴图参数"))
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            SAssignNew(ParentMISelector, SObjectPropertyEntryBox)
                            .AllowedClass(UMaterialInstance::StaticClass())
                            .ObjectPath_Lambda([this](){ return SelectedParentMIPath.ToString(); })
                            .OnObjectChanged_Lambda([this](const FAssetData& AssetData){ 
                                SelectedParentMIPath = AssetData.GetSoftObjectPath(); 
                            })
                            .IsEnabled_Lambda([this](){ return bUseParentMICheckbox->IsChecked(); })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
                        [
                            SNew(SButton)
                            .HAlign(HAlign_Center)
                            .Text(LOCTEXT("CreateGenMat", "生成通用父材质及实例"))
                            .ToolTipText(LOCTEXT("CreateGenMatTip", "自动创建一个带有标准参数(BaseColor, Normal, ORM)的父材质及其对应的实例"))
                            .OnClicked(this, &SImport_MM::OnCreateGenericMaterialClicked)
                            .ContentPadding(FMargin(10, 2))
                        ]
                        // 贴图参数名配置区（仅勾选「使用父材质实例」时显示）
                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            SNew(SVerticalBox)
                            .Visibility_Lambda([this](){ return bUseParentMICheckbox->IsChecked() ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("ParamConfig", "贴图参数名配置"))
                                .ColorAndOpacity(FSlateColor(FLinearColor::Gray))
                                .ToolTipText(LOCTEXT("ToolTip", "如果使用自己自定义的材质球，请根据对应通道填写参数名"))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                            [
                                SNew(SGridPanel)
                            .FillColumn(1, 1.0f)
                            // 从 ChannelMeta 数据中自动生成贴图参数名输入行
                            + SGridPanel::Slot(0, 0).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[0].UIDisplayLabel, GetAllChannelMeta()[0].DefaultTextureParam.ToString(), GetAllChannelMeta()[0].Key) ]
                            + SGridPanel::Slot(1, 0).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[1].UIDisplayLabel, GetAllChannelMeta()[1].DefaultTextureParam.ToString(), GetAllChannelMeta()[1].Key) ]
                            + SGridPanel::Slot(0, 1).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[2].UIDisplayLabel, GetAllChannelMeta()[2].DefaultTextureParam.ToString(), GetAllChannelMeta()[2].Key) ]
                            + SGridPanel::Slot(1, 1).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[3].UIDisplayLabel, GetAllChannelMeta()[3].DefaultTextureParam.ToString(), GetAllChannelMeta()[3].Key) ]
                            + SGridPanel::Slot(0, 2).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[4].UIDisplayLabel, GetAllChannelMeta()[4].DefaultTextureParam.ToString(), GetAllChannelMeta()[4].Key) ]
                            + SGridPanel::Slot(1, 2).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[5].UIDisplayLabel, GetAllChannelMeta()[5].DefaultTextureParam.ToString(), GetAllChannelMeta()[5].Key) ]
                            + SGridPanel::Slot(0, 3).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[6].UIDisplayLabel, GetAllChannelMeta()[6].DefaultTextureParam.ToString(), GetAllChannelMeta()[6].Key) ]
                            + SGridPanel::Slot(1, 3).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[7].UIDisplayLabel, GetAllChannelMeta()[7].DefaultTextureParam.ToString(), GetAllChannelMeta()[7].Key) ]
                            + SGridPanel::Slot(0, 4).Padding(2) [ CreateParamInputRow(GetAllChannelMeta()[8].UIDisplayLabel, GetAllChannelMeta()[8].DefaultTextureParam.ToString(), GetAllChannelMeta()[8].Key) ]
                            ]
                        ]
                    ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(10, 10)
                    [
                        SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Run", "导入")).OnClicked(this, &SImport_MM::OnStartImportClicked).ContentPadding(FMargin(40, 5))
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(10, 5)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight() [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(STextBlock).Text(LOCTEXT("LogLabel", "任务日志:")).ColorAndOpacity(FSlateColor(FLinearColor::Gray)) ]
                            + SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("Clear", "清空日志")).OnClicked(this, &SImport_MM::OnClearLog) ]
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 5) [
                            SNew(SBorder)
                            [
                                SAssignNew(LogScrollBox, SScrollBox)
                                + SScrollBox::Slot() [
                                    SAssignNew(LogBox, SMultiLineEditableText)
                                    .Marshaller(LocalMarshaller)
                                    .IsReadOnly(true)
                                    .AutoWrapText(true)
                                ]
                            ]
                        ]
                    ]
                       
                   ]
                
            ];
            
    
    
}

TSharedRef<SWidget> SImport_MM::CreateNamingRow(EImportAssetType Type, const FString& Label, const FString& DefaultPrefix)
{
    FNamingWidgets Widgets;
 
    // 创建该资产类型的垂直布局组
    TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);
 
    // 第一行：资产类别标题（如：模型、贴图）
    ContentBox->AddSlot().AutoHeight().Padding(0, 5, 0, 2)
    [
        SNew(STextBlock)
        .Text(FText::FromString(Label))
        .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
        .ColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.0f)) // 淡蓝色标识类别
    ];
 
    // 第二行：前缀行
    ContentBox->AddSlot().AutoHeight().Padding(15, 2) // 向右缩进
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SAssignNew(Widgets.bUsePrefix, SCheckBox).IsChecked(ECheckBoxState::Checked)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0)
        [
            SNew(STextBlock).Text(LOCTEXT("PrefixLabel", "前缀:")).MinDesiredWidth(40)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SAssignNew(Widgets.PrefixEntry, SEditableTextBox)
            .Text(FText::FromString(DefaultPrefix))
            .Visibility_Lambda([this, Type]() { 
                return NamingControlMap.Contains(Type) && NamingControlMap[Type].bUsePrefix->IsChecked() ? EVisibility::Visible : EVisibility::Hidden; 
            })
        ]
    ];
 
    // 第三行：后缀行
    ContentBox->AddSlot().AutoHeight().Padding(15, 2, 0, 8)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SAssignNew(Widgets.bUseSuffix, SCheckBox).IsChecked(ECheckBoxState::Unchecked)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0)
        [
            SNew(STextBlock).Text(LOCTEXT("SuffixLabel", "后缀:")).MinDesiredWidth(40)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SAssignNew(Widgets.SuffixEntry, SEditableTextBox)
            .HintText(LOCTEXT("SuffixHint", "输入后缀内容..."))
            .Visibility_Lambda([this, Type]() { 
                return NamingControlMap.Contains(Type) && NamingControlMap[Type].bUseSuffix->IsChecked() ? EVisibility::Visible : EVisibility::Hidden; 
            })
        ]
    ];
 
    NamingControlMap.Add(Type, Widgets);
    return ContentBox;
}
 
FString SImport_MM::GetAppliedName(const FString& RawName, EImportAssetType Type)
{
    if (!NamingControlMap.Contains(Type)) return RawName;
    
    const FNamingWidgets& Widgets = NamingControlMap[Type];
    FString FinalName = RawName;
 
    if (Widgets.bUsePrefix->IsChecked()) FinalName = Widgets.PrefixEntry->GetText().ToString() + FinalName;
    if (Widgets.bUseSuffix->IsChecked()) FinalName = FinalName + Widgets.SuffixEntry->GetText().ToString();
 
    return FinalName;
}
 
 
void SImport_MM::AddLog(const FString& Message, FLinearColor Color)
{
    FString TimeStr = FDateTime::Now().ToString(TEXT("[%H:%M:%S] "));
    FString ColorHex = Color.ToFColor(true).ToHex();
    FString FormattedMessage = FString::Printf(TEXT("%s%s\n"), *TimeStr, *Message);
    FText Current = LogBox->GetText();
    LogBox->SetText(FText::FromString(Current.ToString() + FormattedMessage));
    LogScrollBox->ScrollToEnd();
}
 
FReply SImport_MM::OnClearLog()
{
    LogBox->SetText(FText::GetEmpty());
    return FReply::Handled();
}
 
FReply SImport_MM::OnBrowseSourceClicked()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (DP)
    {
        IMainFrameModule& MF = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        void* Parent = MF.GetParentWindow().IsValid() ? MF.GetParentWindow()->GetNativeWindow()->GetOSWindowHandle() : nullptr;
        FString Out; if (DP->OpenDirectoryDialog(Parent, TEXT("选择源"), TEXT(""), Out)) { SourceFolderPath = Out; SourcePathBox->SetText(FText::FromString(SourceFolderPath)); }
    }
    return FReply::Handled();
}
 
FReply SImport_MM::OnBrowseDestClicked()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (DP)
    {
        IMainFrameModule& MF = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        void* Parent = MF.GetParentWindow().IsValid() ? MF.GetParentWindow()->GetNativeWindow()->GetOSWindowHandle() : nullptr;
        FString Out; if (DP->OpenDirectoryDialog(Parent, TEXT("目标"), FPaths::ProjectContentDir(), Out))
        {
            FString Pkg; if (FPackageName::TryConvertFilenameToLongPackageName(Out, Pkg)) { RelativeDestPath = Pkg; DestPathBox->SetText(FText::FromString(RelativeDestPath)); }
        }
    }
    return FReply::Handled();
}
 
FReply SImport_MM::OnStartImportClicked()
{
    if (SourceFolderPath.IsEmpty()) return FReply::Handled();
    FString FinalDest = DestPathBox->GetText().ToString().IsEmpty() ? TEXT("/Game/BatchImport") : DestPathBox->GetText().ToString();
    FString TexSubFolderName = TexSubFolderNameBox->GetText().ToString();
 
    AddLog(TEXT("--- 开始执行层级递归导入 ---"), FLinearColor::White);
    IFileManager& FM = IFileManager::Get();
    
    TFunction<void(const FString&)> RecursiveScan;
    RecursiveScan = [&](const FString& CurrentDirPath) {
        TArray<FString> LocalFiles;
        FM.FindFiles(LocalFiles, *(CurrentDirPath / TEXT("*")), true, false);
 
        FString RelativePath = CurrentDirPath == SourceFolderPath ? TEXT("") : CurrentDirPath.RightChop(SourceFolderPath.Len() + 1);
 
        for (const FString& FileName : LocalFiles) {
            FString Ext = FPaths::GetExtension(FileName).ToLower();
            if (Ext == TEXT("fbx") || Ext == TEXT("obj")) {
                FImportFolderTask Task;
                Task.MeshPath = CurrentDirPath / FileName;
                Task.FolderName = RelativePath.IsEmpty() ? FPaths::GetBaseFilename(SourceFolderPath) : RelativePath;
                
                FString TexSearchPath = TexSubFolderName.IsEmpty() ? CurrentDirPath : (CurrentDirPath / TexSubFolderName);
 
                TArray<FString> TexFiles;
                FM.FindFiles(TexFiles, *(TexSearchPath / TEXT("*")), true, false);
                for (const FString& TF : TexFiles) {
                    FString TExt = FPaths::GetExtension(TF).ToLower();
                    if (TExt == TEXT("png") || TExt == TEXT("tga") || TExt == TEXT("jpg") || TExt == TEXT("jpeg")|| TExt == TEXT("JPEG")) {
                        Task.TextureMap.Add(TF, TexSearchPath / TF);
                    }
                }
                ExecuteImportTask(Task, FinalDest);
            }
        }
 
        TArray<FString> SubDirs;
        FM.FindFiles(SubDirs, *(CurrentDirPath / TEXT("*")), false, true);
        for (const FString& SubDirName : SubDirs) {
            // 排除指定的贴图子目录，避免扫描其内部的模型
            if (!TexSubFolderName.IsEmpty() && SubDirName.Equals(TexSubFolderName, ESearchCase::IgnoreCase)) continue;
            RecursiveScan(CurrentDirPath / SubDirName);
        }
    };
 
    RecursiveScan(SourceFolderPath);
    AddLog(TEXT("--- 所有导入任务已结束 ---"), FLinearColor::White);
    return FReply::Handled();
}


void SImport_MM::ExecuteImportTask(const FImportFolderTask& Task, const FString& BaseDestPath)
{
    const FString FinalPath = BaseDestPath / Task.FolderName;
    const FString MeshBaseName = FPaths::GetBaseFilename(Task.MeshPath);
 
    // 阶段 1: 导入模型
    PerformMeshImport(Task, FinalPath);
 
    // 阶段 2: 导入贴图
    PerformTextureImport(Task, FinalPath);
 
    // 阶段 3: 获取导入的模型对象
    TArray<UStaticMesh*> Meshes = CollectImportedMeshes(FinalPath, MeshBaseName);
    if (Meshes.Num() == 0) return;

    // 如果取消勾选"拆分模型添加合并模型名前缀"，则重命名子模型去掉合并模型名
    if (bUseCombinedNamePrefixCheckbox.IsValid() && !bUseCombinedNamePrefixCheckbox->IsChecked())
    {
        RenameSplitMeshes(Meshes, MeshBaseName, FinalPath);
        // 重命名后子模型名称已改变（不再包含 MeshBaseName），
        // 且指针可能因重命名操作失效，因此重新收集路径下所有 StaticMesh
        Meshes = CollectAllStaticMeshesInPath(FinalPath);
        if (Meshes.Num() == 0) return;
    }
 
    // --- 关键修正点：将局部变量类型改为 UMaterialInterface* ---
    TMap<FString, UMaterialInterface*> CreatedMaterials; 
    UMaterialInterface* SingleFallbackMat = nullptr;
    int32 BaseColorCount = 0;
 
    // 阶段 4: 分析贴图并创建材质 (现在类型匹配了，引用绑定成功)
    GenerateMaterials(Task, FinalPath, CreatedMaterials, SingleFallbackMat, BaseColorCount);
 
    // 阶段 5: 分配材质 (同时更新此函数内部处理逻辑以支持 Interface)
    ApplyMaterialsToMeshes(Meshes, CreatedMaterials, BaseColorCount, SingleFallbackMat, MeshBaseName, FinalPath);
}


void SImport_MM::PerformMeshImport(const FImportFolderTask& Task, const FString& FinalPath)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    
    // 获取原始文件名并应用命名规则
    FString RawMeshName = FPaths::GetBaseFilename(Task.MeshPath);
    FString AppliedMeshName = GetAppliedName(RawMeshName, EImportAssetType::Mesh);
 
    UFbxFactory* Fact = NewObject<UFbxFactory>();
    Fact->ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
    Fact->ImportUI->bImportMaterials = Fact->ImportUI->bImportTextures = false;
    Fact->ImportUI->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Fact->ImportUI->StaticMeshImportData->bConvertSceneUnit = true;
    Fact->ImportUI->StaticMeshImportData->ImportUniformScale = 1.0f;
 
    UAssetImportTask* MTask = NewObject<UAssetImportTask>();
    MTask->Filename = Task.MeshPath; 
    MTask->DestinationPath = FinalPath;
    // 【关键修改】：明确指定导入后的资产名称，应用前缀和后缀
    MTask->DestinationName = AppliedMeshName; 
    MTask->Factory = Fact; 
    MTask->bAutomated = true;
    
    AT.ImportAssetTasks({ MTask });
 
    AddLog(FString::Printf(TEXT("模型导入任务已提交: %s"), *AppliedMeshName), FLinearColor::White);
}
 
 
void SImport_MM::PerformTextureImport(const FImportFolderTask& Task, const FString& FinalPath)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
 
    TArray<UAssetImportTask*> TextureTasks;
 
    // 遍历任务中的每一张贴图
    for (auto& TP : Task.TextureMap)
    {
        FString RawTexName = FPaths::GetBaseFilename(TP.Key);
        // 【关键修改】：对贴图应用命名规则
        FString AppliedTexName = GetAppliedName(RawTexName, EImportAssetType::Texture);
 
        UAssetImportTask* TTask = NewObject<UAssetImportTask>();
        TTask->Filename = TP.Value; // 原始文件全路径
        TTask->DestinationPath = FinalPath;
        TTask->DestinationName = AppliedTexName; // 应用命名规则后的名称
        TTask->bAutomated = true;
        TTask->bSave = true;
 
        TextureTasks.Add(TTask);
    }
    
    if (TextureTasks.Num() > 0)
    {
        AT.ImportAssetTasks(TextureTasks);
    }
 
    ARM.Get().ScanPathsSynchronous({ FinalPath });
}


TArray<UStaticMesh*> SImport_MM::CollectImportedMeshes(const FString& FinalPath, const FString& MeshBaseName)
{
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> MeshDatas; 
    ARM.Get().GetAssetsByPath(FName(*FinalPath), MeshDatas);
    
    TArray<UStaticMesh*> Meshes;
    for (const FAssetData& Ad : MeshDatas) 
    {
        UStaticMesh* M = Cast<UStaticMesh>(Ad.GetAsset());
        if (M && M->GetName().Contains(MeshBaseName)) 
        {
            Meshes.Add(M);
        }
    }
    return Meshes;
}


TArray<UStaticMesh*> SImport_MM::CollectAllStaticMeshesInPath(const FString& FinalPath)
{
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    ARM.Get().ScanPathsSynchronous({ FinalPath });

    TArray<FAssetData> MeshDatas;
    ARM.Get().GetAssetsByPath(FName(*FinalPath), MeshDatas);

    TArray<UStaticMesh*> Meshes;
    for (const FAssetData& Ad : MeshDatas)
    {
        if (UStaticMesh* M = Cast<UStaticMesh>(Ad.GetAsset()))
        {
            Meshes.Add(M);
        }
    }
    return Meshes;
}


bool SImport_MM::DoesAssetExistInPath(const FString& AssetName, const FString& PackagePath) const
{
    // 构造对象路径，如 /Game/Folder/Sub/Name.Name
    FString ObjectPath = (PackagePath / AssetName) + TEXT(".") + AssetName;

    // 优先检查内存中已加载的对象（刚导入但可能尚未保存到磁盘的资产）
    if (FindObject<UObject>(nullptr, *ObjectPath))
    {
        return true;
    }

    // 再查资产注册表（已保存但可能未加载的资产）
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    return ARM.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid();
}


FString SImport_MM::MakeUniqueAssetName(const FString& DesiredName, const FString& PackagePath) const
{
    if (!DoesAssetExistInPath(DesiredName, PackagePath))
    {
        return DesiredName;
    }

    // 名称已被占用，追加 _1, _2 ... 直到不冲突
    int32 Suffix = 1;
    FString UniqueName;
    do
    {
        UniqueName = FString::Printf(TEXT("%s_%d"), *DesiredName, Suffix++);
    } while (DoesAssetExistInPath(UniqueName, PackagePath));

    return UniqueName;
}


void SImport_MM::GenerateMaterials(const FImportFolderTask& Task, const FString& FinalPath, 
    TMap<FString, UMaterialInterface*>& OutCreatedMaterials, UMaterialInterface*& OutSingleFallbackMat, int32& OutBaseColorCount)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    bool bUseExistingParent = bUseParentMICheckbox.IsValid() ? bUseParentMICheckbox->IsChecked() : false;
    bool bAutoCreateInstance = bCreateMICheckbox.IsValid() ? bCreateMICheckbox->IsChecked() : false;
    
    UMaterialInterface* ParentMI = Cast<UMaterialInterface>(SelectedParentMIPath.TryLoad());
 
    // 1. 预处理：识别所有基础颜色贴图分组
    OutBaseColorCount = 0;
    TArray<FString> BaseColorRawNames;
    for (auto& TP : Task.TextureMap) 
    {
        FString FileName = FPaths::GetBaseFilename(TP.Key).ToLower();
        if (TextureMatch::ContainsAny(FileName, TextureMatch::BC())) 
        {
            BaseColorRawNames.Add(FPaths::GetBaseFilename(TP.Key));
            OutBaseColorCount++;
        }
    }
 
    // 2. 核心逻辑：遍历每个 BaseColor 组
    for (const FString& BCRawName : BaseColorRawNames) 
    {
        TMap<FString, UTexture2D*> LocalMatch;
        UMaterialInterface* WorkingMat = nullptr;
        bool bHasOpacity = false; 

        // 剥离 BC 后缀，得到干净的材质命名基础名（如 "Chair_BaseColor" → "Chair"）
        FString MatNameBase = BCRawName;
        for (const FString& S : BaseColorSuffixes::All())
        {
            if (MatNameBase.EndsWith(S, ESearchCase::IgnoreCase))
            {
                MatNameBase = MatNameBase.LeftChop(S.Len());
                break;
            }
        }

        // 提取材质组前缀，用于过滤贴图
        FString CleanPrefix = BCRawName;
        if (OutBaseColorCount > 1) {
            for(const FString& S : BaseColorSuffixes::All()) {
                if(CleanPrefix.EndsWith(S, ESearchCase::IgnoreCase)) {
                    CleanPrefix = CleanPrefix.LeftChop(S.Len());
                    break;
                }
            }
        }
 
        for (auto& TP : Task.TextureMap) 
        {
            FString TexFileName = FPaths::GetBaseFilename(TP.Key);
            if (OutBaseColorCount > 1 && !TexFileName.Contains(CleanPrefix)) continue;
 
            FString AppliedAssetName = GetAppliedName(TexFileName, EImportAssetType::Texture);
            UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *(FinalPath / AppliedAssetName + TEXT(".") + AppliedAssetName));
            if (!Tex) continue;
 
            FString L = TexFileName.ToLower();
            // 使用数据文件中定义的匹配规则进行纹理分类
            if (TextureMatch::ContainsAny(L, TextureMatch::BC()))
                LocalMatch.Add(ChannelKey::BC, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::N()))
                LocalMatch.Add(ChannelKey::N, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::EM()))
                LocalMatch.Add(ChannelKey::EM, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::OP())) {
                LocalMatch.Add(ChannelKey::OP, Tex);
                bHasOpacity = true; 
            }
            else if (TextureMatch::IsORMTexture(L))
                LocalMatch.Add(ChannelKey::ORM, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::AO()))
                LocalMatch.Add(ChannelKey::AO, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::Roughness()))
                LocalMatch.Add(ChannelKey::R, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::Metallic()))
                LocalMatch.Add(ChannelKey::M, Tex);
        }
 
        // 根据是否有ORM贴图，选择对应的材质生成函数
        bool bFoundORM = LocalMatch.Contains(ChannelKey::ORM);
        if (bFoundORM)
        {
            GenerateMaterialWithORM(MatNameBase, LocalMatch, bHasOpacity, FinalPath, 
                                    bUseExistingParent, bAutoCreateInstance, ParentMI, WorkingMat);
        }
        else
        {
            GenerateMaterialWithoutORM(MatNameBase, LocalMatch, bHasOpacity, FinalPath, 
                                       bUseExistingParent, bAutoCreateInstance, ParentMI, WorkingMat);
        }

        if (!OutSingleFallbackMat) OutSingleFallbackMat = WorkingMat;
        OutCreatedMaterials.Add(MatNameBase, WorkingMat);
    }
}


void SImport_MM::GenerateMaterialWithoutORM(
    const FString& BCRawName,
    const TMap<FString, UTexture2D*>& LocalMatch,
    bool bHasOpacity,
    const FString& FinalPath,
    bool bUseExistingParent,
    bool bAutoCreateInstance,
    UMaterialInterface* ParentMI,
    UMaterialInterface*& OutWorkingMat)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // --- 分支 A: 材质实例模式 (MIC) ---
    if (bUseExistingParent && ParentMI)
    {
        FString MIName = GetAppliedName(BCRawName, EImportAssetType::Instance);
        MIName = MakeUniqueAssetName(MIName, FinalPath);
        UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AT.DuplicateAsset(MIName, FinalPath, ParentMI));

        if (MIC)
        {
            OutWorkingMat = MIC;
            FStaticParameterSet NewStaticParameters;
            MIC->GetStaticParameterValues(NewStaticParameters);

            // 强制设置开关状态的辅助 Lambda
            auto ForceSetSwitch = [&](FName SwitchParamName, bool bValue)
            {
                for (FStaticSwitchParameter& Switch : NewStaticParameters.StaticSwitchParameters)
                {
                    if (Switch.ParameterInfo.Name == SwitchParamName)
                    {
                        Switch.Value = bValue;
                        Switch.bOverride = true;
                        return;
                    }
                }
                // 如果没找到，手动添加一个
                FStaticSwitchParameter NewSwitch;
                NewSwitch.ParameterInfo = FMaterialParameterInfo(SwitchParamName);
                NewSwitch.Value = bValue;
                NewSwitch.bOverride = true;
                NewStaticParameters.StaticSwitchParameters.Add(NewSwitch);
            };

            // 应用贴图并设置压缩的辅助 Lambda
            auto ApplyTexture = [&](const FString& LocalKey, FName DefaultParamName, UTexture2D* T)
            {
                if (!T) return;
                // 设置正确的压缩和颜色空间
                const FChannelMeta* Meta = FindChannelMeta(LocalKey);
                bool bIsSRGB = Meta ? Meta->bSRGB : false;
                T->SRGB = bIsSRGB;
                T->CompressionSettings = Meta ? (TextureCompressionSettings)Meta->CompressionSettings : TC_Default;
                T->PostEditChange();

                // 从 UI 映射表获取用户定义的参数名
                FString PName = ParamNameInputs.Contains(LocalKey) ? ParamNameInputs[LocalKey]->GetText().ToString() : DefaultParamName.ToString();
                if (PName.IsEmpty()) PName = DefaultParamName.ToString();

                MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(*PName)), T);
            };

            // --- 设置基础通道开关 ---
            bool bFoundBC  = LocalMatch.Contains(ChannelKey::BC);
            bool bFoundN   = LocalMatch.Contains(ChannelKey::N);
            bool bFoundEM  = LocalMatch.Contains(ChannelKey::EM);
            bool bFoundOP  = LocalMatch.Contains(ChannelKey::OP);

            ForceSetSwitch(SwitchParam::Use_BaseColor, bFoundBC);
            ForceSetSwitch(SwitchParam::Use_Normal, bFoundN);
            ForceSetSwitch(SwitchParam::Use_Emissive, bFoundEM);
            ForceSetSwitch(SwitchParam::Use_Opacity, bFoundOP);

            // --- 无ORM贴图：关闭ORM总开关，使用独立通道 ---
            ForceSetSwitch(SwitchParam::Use_ORM, false);

            bool bFoundAO = LocalMatch.Contains(ChannelKey::AO);
            bool bFoundR  = LocalMatch.Contains(ChannelKey::R);
            bool bFoundM  = LocalMatch.Contains(ChannelKey::M);

            ForceSetSwitch(SwitchParam::Use_AO, bFoundAO);
            ForceSetSwitch(SwitchParam::Use_Roughness, bFoundR);
            ForceSetSwitch(SwitchParam::Use_Metallic, bFoundM);

            // --- 应用贴图 ---
            if (bFoundBC) ApplyTexture(ChannelKey::BC, TextureParam::BaseColor, LocalMatch[ChannelKey::BC]);
            if (bFoundN)  ApplyTexture(ChannelKey::N,  TextureParam::Normal,    LocalMatch[ChannelKey::N]);
            if (bFoundEM) ApplyTexture(ChannelKey::EM, TextureParam::Emissive,  LocalMatch[ChannelKey::EM]);
            if (bFoundOP) ApplyTexture(ChannelKey::OP, TextureParam::Opacity,   LocalMatch[ChannelKey::OP]);
            if (bFoundAO) ApplyTexture(ChannelKey::AO, TextureParam::AmbientOcclusion, LocalMatch[ChannelKey::AO]);
            if (bFoundR)  ApplyTexture(ChannelKey::R,  TextureParam::Roughness,        LocalMatch[ChannelKey::R]);
            if (bFoundM)  ApplyTexture(ChannelKey::M,  TextureParam::Metallic,         LocalMatch[ChannelKey::M]);

            if (bHasOpacity) {
                MIC->BasePropertyOverrides.bOverride_BlendMode = true;
                MIC->BasePropertyOverrides.BlendMode = BLEND_Masked;
            }

            // 保存静态参数变更
            MIC->UpdateStaticPermutation(NewStaticParameters);
            MIC->PostEditChange();
        }
    }
    // --- 分支 B: 母材质连线模式 ---
    else
    {
        FString FinalMatName = GetAppliedName(BCRawName, EImportAssetType::Material);
        FinalMatName = MakeUniqueAssetName(FinalMatName, FinalPath);
        UMaterial* NewMat = Cast<UMaterial>(AT.CreateAsset(FinalMatName, FinalPath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>()));

        if (NewMat)
        {
            OutWorkingMat = NewMat;
            if (bHasOpacity) NewMat->BlendMode = BLEND_Masked;

            int32 YPos = 0;
            for (auto& Pair : LocalMatch)
            {
                UTexture2D* T = Pair.Value;
                auto* Node = Cast<UMaterialExpressionTextureSample>(UMaterialEditingLibrary::CreateMaterialExpression(NewMat, UMaterialExpressionTextureSample::StaticClass()));
                Node->Texture = T;
                Node->MaterialExpressionEditorY = YPos;
                YPos += 350;

                FString K = Pair.Key;
                const FChannelMeta* Meta = FindChannelMeta(K);
                if (Meta)
                {
                    T->SRGB = Meta->bSRGB;
                    T->CompressionSettings = Meta->CompressionSettings;
                    Node->SamplerType = Meta->SamplerType;
                    T->PostEditChange();
                    UMaterialEditingLibrary::ConnectMaterialProperty(Node, TEXT(""), Meta->MaterialProp);
                }
            }

            UMaterialEditingLibrary::RecompileMaterial(NewMat);

            if (bAutoCreateInstance) {
                FString FinalInstName = GetAppliedName(BCRawName, EImportAssetType::Instance);
                FinalInstName = MakeUniqueAssetName(FinalInstName, FinalPath);
                UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(AT.CreateAsset(FinalInstName, FinalPath, UMaterialInstanceConstant::StaticClass(), NewObject<UMaterialInstanceConstantFactoryNew>()));
                if (NewMIC) {
                    NewMIC->SetParentEditorOnly(NewMat);
                    NewMIC->PostEditChange();
                    OutWorkingMat = NewMIC;
                }
            }
        }
    }
}


void SImport_MM::GenerateMaterialWithORM(
    const FString& BCRawName,
    const TMap<FString, UTexture2D*>& LocalMatch,
    bool bHasOpacity,
    const FString& FinalPath,
    bool bUseExistingParent,
    bool bAutoCreateInstance,
    UMaterialInterface* ParentMI,
    UMaterialInterface*& OutWorkingMat)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // --- 分支 A: 材质实例模式 (MIC) ---
    if (bUseExistingParent && ParentMI)
    {
        FString MIName = GetAppliedName(BCRawName, EImportAssetType::Instance);
        MIName = MakeUniqueAssetName(MIName, FinalPath);
        UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AT.DuplicateAsset(MIName, FinalPath, ParentMI));

        if (MIC)
        {
            OutWorkingMat = MIC;
            FStaticParameterSet NewStaticParameters;
            MIC->GetStaticParameterValues(NewStaticParameters);

            // 强制设置开关状态的辅助 Lambda
            auto ForceSetSwitch = [&](FName SwitchParamName, bool bValue)
            {
                for (FStaticSwitchParameter& Switch : NewStaticParameters.StaticSwitchParameters)
                {
                    if (Switch.ParameterInfo.Name == SwitchParamName)
                    {
                        Switch.Value = bValue;
                        Switch.bOverride = true;
                        return;
                    }
                }
                // 如果没找到，手动添加一个
                FStaticSwitchParameter NewSwitch;
                NewSwitch.ParameterInfo = FMaterialParameterInfo(SwitchParamName);
                NewSwitch.Value = bValue;
                NewSwitch.bOverride = true;
                NewStaticParameters.StaticSwitchParameters.Add(NewSwitch);
            };

            // 应用贴图并设置压缩的辅助 Lambda
            auto ApplyTexture = [&](const FString& LocalKey, FName DefaultParamName, UTexture2D* T)
            {
                if (!T) return;
                // 设置正确的压缩和颜色空间
                const FChannelMeta* Meta = FindChannelMeta(LocalKey);
                bool bIsSRGB = Meta ? Meta->bSRGB : false;
                T->SRGB = bIsSRGB;
                T->CompressionSettings = Meta ? (TextureCompressionSettings)Meta->CompressionSettings : TC_Default;
                T->PostEditChange();

                // 从 UI 映射表获取用户定义的参数名
                FString PName = ParamNameInputs.Contains(LocalKey) ? ParamNameInputs[LocalKey]->GetText().ToString() : DefaultParamName.ToString();
                if (PName.IsEmpty()) PName = DefaultParamName.ToString();

                MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(*PName)), T);
            };

            // --- 设置基础通道开关 ---
            bool bFoundBC  = LocalMatch.Contains(ChannelKey::BC);
            bool bFoundN   = LocalMatch.Contains(ChannelKey::N);
            bool bFoundEM  = LocalMatch.Contains(ChannelKey::EM);
            bool bFoundOP  = LocalMatch.Contains(ChannelKey::OP);

            ForceSetSwitch(SwitchParam::Use_BaseColor, bFoundBC);
            ForceSetSwitch(SwitchParam::Use_Normal, bFoundN);
            ForceSetSwitch(SwitchParam::Use_Emissive, bFoundEM);
            ForceSetSwitch(SwitchParam::Use_Opacity, bFoundOP);

            // --- 有ORM贴图：开启ORM总开关，关闭独立通道开关 ---
            ForceSetSwitch(SwitchParam::Use_ORM, true);
            ForceSetSwitch(SwitchParam::Use_AO, false);
            ForceSetSwitch(SwitchParam::Use_Roughness, false);
            ForceSetSwitch(SwitchParam::Use_Metallic, false);

            // --- 应用基础通道贴图 ---
            if (bFoundBC) ApplyTexture(ChannelKey::BC, TextureParam::BaseColor, LocalMatch[ChannelKey::BC]);
            if (bFoundN)  ApplyTexture(ChannelKey::N,  TextureParam::Normal,    LocalMatch[ChannelKey::N]);
            if (bFoundEM) ApplyTexture(ChannelKey::EM, TextureParam::Emissive,  LocalMatch[ChannelKey::EM]);
            if (bFoundOP) ApplyTexture(ChannelKey::OP, TextureParam::Opacity,   LocalMatch[ChannelKey::OP]);

            // --- 将ORM贴图放入ORM参数 ---
            ApplyTexture(ChannelKey::ORM, TextureParam::ORM, LocalMatch[ChannelKey::ORM]);

            if (bHasOpacity) {
                MIC->BasePropertyOverrides.bOverride_BlendMode = true;
                MIC->BasePropertyOverrides.BlendMode = BLEND_Masked;
            }

            // 保存静态参数变更
            MIC->UpdateStaticPermutation(NewStaticParameters);
            MIC->PostEditChange();
        }
    }
    // --- 分支 B: 母材质连线模式 ---
    else
    {
        FString FinalMatName = GetAppliedName(BCRawName, EImportAssetType::Material);
        FinalMatName = MakeUniqueAssetName(FinalMatName, FinalPath);
        UMaterial* NewMat = Cast<UMaterial>(AT.CreateAsset(FinalMatName, FinalPath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>()));

        if (NewMat)
        {
            OutWorkingMat = NewMat;
            if (bHasOpacity) NewMat->BlendMode = BLEND_Masked;

            int32 YPos = 0;
            for (auto& Pair : LocalMatch)
            {
                UTexture2D* T = Pair.Value;
                auto* Node = Cast<UMaterialExpressionTextureSample>(UMaterialEditingLibrary::CreateMaterialExpression(NewMat, UMaterialExpressionTextureSample::StaticClass()));
                Node->Texture = T;
                Node->MaterialExpressionEditorY = YPos;
                YPos += 350;

                FString K = Pair.Key;
                const FChannelMeta* Meta = FindChannelMeta(K);
                if (Meta)
                {
                    T->SRGB = Meta->bSRGB;
                    T->CompressionSettings = Meta->CompressionSettings;
                    Node->SamplerType = Meta->SamplerType;
                    T->PostEditChange();

                    if (K == ChannelKey::ORM)
                    {
                        // ORM贴图：利用 ChannelMeta 中的通道信息进行连接
                        // R=AO, G=Roughness, B=Metallic
                        for (const FChannelMeta& M : GetAllChannelMeta())
                        {
                            if (M.ORMChannelIndex > 0 && !M.ORMChannelLetter.IsEmpty())
                            {
                                UMaterialEditingLibrary::ConnectMaterialProperty(Node, *M.ORMChannelLetter, M.MaterialProp);
                            }
                        }
                    }
                    else
                    {
                        UMaterialEditingLibrary::ConnectMaterialProperty(Node, TEXT(""), Meta->MaterialProp);
                    }
                }
            }

            UMaterialEditingLibrary::RecompileMaterial(NewMat);

            if (bAutoCreateInstance) {
                FString FinalInstName = GetAppliedName(BCRawName, EImportAssetType::Instance);
                FinalInstName = MakeUniqueAssetName(FinalInstName, FinalPath);
                UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(AT.CreateAsset(FinalInstName, FinalPath, UMaterialInstanceConstant::StaticClass(), NewObject<UMaterialInstanceConstantFactoryNew>()));
                if (NewMIC) {
                    NewMIC->SetParentEditorOnly(NewMat);
                    NewMIC->PostEditChange();
                    OutWorkingMat = NewMIC;
                }
            }
        }
    }
}



void SImport_MM::RenameSplitMeshes(TArray<UStaticMesh*>& Meshes, const FString& MeshBaseName, const FString& FinalPath)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    const FString AppliedMeshBaseName = GetAppliedName(MeshBaseName, EImportAssetType::Mesh);
    const FString MeshSearchPrefix = AppliedMeshBaseName + TEXT("_");

    FString Prefix;
    if (NamingControlMap.Contains(EImportAssetType::Mesh) && NamingControlMap[EImportAssetType::Mesh].bUsePrefix->IsChecked())
    {
        Prefix = NamingControlMap[EImportAssetType::Mesh].PrefixEntry->GetText().ToString();
    }

    TArray<FAssetRenameData> RenameAssets;
    TSet<FString> ReservedNames;
    ReservedNames.Add(AppliedMeshBaseName); // 主模型名字必须永远保留，谁都不能占用

    for (UStaticMesh* SM : Meshes)
    {
        if (!IsValid(SM)) continue; // 防御性判空
        FString CurrentName = SM->GetName();
        if (CurrentName == AppliedMeshBaseName) continue;

        FString CleanLogicName = CurrentName;
        if (CurrentName.StartsWith(MeshSearchPrefix))
            CleanLogicName = CurrentName.RightChop(MeshSearchPrefix.Len());
        else if (CurrentName.StartsWith(AppliedMeshBaseName))
        {
            CleanLogicName = CurrentName.RightChop(AppliedMeshBaseName.Len());
            if (CleanLogicName.StartsWith(TEXT("_"))) CleanLogicName = CleanLogicName.RightChop(1);
        }
        if (CleanLogicName.IsEmpty()) continue;

        FString NewName = Prefix + CleanLogicName;
        if (NewName.Equals(CurrentName)) continue;

        // 【关键修复】如果目标名字已被占用（撞主模型、撞其它子模型、或撞已存在的贴图/材质），
        // 自动加编号消歧，而不是交给引擎弹窗去"覆写"一个还在被引用的对象
        FString UniqueName = NewName;
        int32 Suffix = 1;
        while (ReservedNames.Contains(UniqueName) || DoesAssetExistInPath(UniqueName, FinalPath))
        {
            UniqueName = FString::Printf(TEXT("%s_%d"), *NewName, Suffix++);
        }
        ReservedNames.Add(UniqueName);

        RenameAssets.Add(FAssetRenameData(SM, FinalPath, UniqueName));
        AddLog(FString::Printf(TEXT("重命名: %s → %s"), *CurrentName, *UniqueName), FLinearColor::Yellow);
    }

    if (RenameAssets.Num() > 0)
    {
        // 使用非对话框版本，名字已保证唯一，不会再触发覆写确认框
        AT.RenameAssets(RenameAssets);
    }
}


void SImport_MM::ApplyMaterialsToMeshes(const TArray<UStaticMesh*>& Meshes, const TMap<FString, UMaterialInterface*>& CreatedMaterials, int32 BaseColorCount, UMaterialInterface* SingleFallbackMat, const FString& MeshBaseName, const FString& FinalPath)
{
    // 关键修正：获取模型在应用命名规则后的预期名称前缀
    const FString AppliedMeshBaseName = GetAppliedName(MeshBaseName, EImportAssetType::Mesh);
    const FString MeshSearchPrefix = AppliedMeshBaseName + TEXT("_");
    
    bool bHasNamingError = false;
 
    for (UStaticMesh* SM : Meshes) 
    {
        if (!IsValid(SM)) continue;
        // 1. 获取模型当前在引擎中的真实名称
        FString CurrentSMName = SM->GetName();
        
        // 2. 清理名称：去掉前缀部分，还原出原始逻辑名（用于和 CreatedMaterials 的 Key 匹配）
        // 例如：从 "SM_Chair_Seat" 还原出 "Seat"
        FString CleanLogicName = CurrentSMName.StartsWith(MeshSearchPrefix) ? CurrentSMName.RightChop(MeshSearchPrefix.Len()) : CurrentSMName;
        
        // 如果连 AppliedMeshBaseName 都包含了，也清理掉
        if (CleanLogicName.StartsWith(AppliedMeshBaseName))
        {
             CleanLogicName = CleanLogicName.RightChop(AppliedMeshBaseName.Len()).TrimStartAndEnd().Replace(TEXT("_"), TEXT(""), ESearchCase::IgnoreCase);
        }

        // 兼容重命名后的子模型：如果以上都没匹配到，尝试只剥离命名前缀（如 "SM_" → "Seat"）
        if (CleanLogicName == CurrentSMName && NamingControlMap.Contains(EImportAssetType::Mesh))
        {
            const FNamingWidgets& MeshWidgets = NamingControlMap[EImportAssetType::Mesh];
            if (MeshWidgets.bUsePrefix->IsChecked())
            {
                FString Prefix = MeshWidgets.PrefixEntry->GetText().ToString();
                if (CurrentSMName.StartsWith(Prefix))
                {
                    CleanLogicName = CurrentSMName.RightChop(Prefix.Len());
                }
            }
        }
 
        bool bAssigned = false;
 
        // 3. 遍历已创建的材质映射表（注意：CreatedMaterials 的 Key 通常是原始贴图名，如 "Chair_BC"）
        for (auto& MatPair : CreatedMaterials) 
        {
            FString MatKey = MatPair.Key.ToLower();
            FString TargetMatchName = CleanLogicName.ToLower();
 
            // 模糊匹配逻辑：如果模型名包含材质名，或材质名包含模型名
            if (MatKey.Contains(TargetMatchName) || TargetMatchName.Contains(MatKey) || TargetMatchName.IsEmpty()) 
            {
                if (!IsValid(MatPair.Value))
                {
                    AddLog(FString::Printf(TEXT("模型 [%s] 匹配到材质但材质无效，跳过。"), *CurrentSMName), FLinearColor::Red);
                    break;
                }
                for (int32 i = 0; i < SM->GetStaticMaterials().Num(); ++i)
                {
                    SM->SetMaterial(i, MatPair.Value);
                }
                SM->PostEditChange(); 
                bAssigned = true; 
                break;
            }
        }
 
        // 4. 兜底逻辑：如果没匹配上但只有一个材质，直接赋予
        if (!bAssigned) 
        {
            if (BaseColorCount == 1 && SingleFallbackMat) 
            {
                for (int32 i = 0; i < SM->GetStaticMaterials().Num(); ++i)
                {
                    SM->SetMaterial(i, SingleFallbackMat);
                }
                SM->PostEditChange();
                bAssigned = true;
                AddLog(FString::Printf(TEXT("模型 [%s] 自动适配唯一材质。"), *CurrentSMName), FLinearColor::Yellow);
            } 
            else 
            {
                bHasNamingError = true;
            }
        }
    }
 
    if (bHasNamingError) 
    {
        AddLog(FString::Printf(TEXT("提醒：模型 [%s] 部分组件未能自动匹配到对应材质，请手动检查。"), *AppliedMeshBaseName), FLinearColor::Red);
    }
    else
    {
        AddLog(FString::Printf(TEXT("成功：已为 [%s] 及其组件完成材质分配。"), *AppliedMeshBaseName), FLinearColor::Green);
    }
}


TSharedRef<SWidget> SImport_MM::CreateParamInputRow(const FString& ChannelLabel, const FString& DefaultParamName, const FString& Key)
{
    TSharedPtr<SEditableTextBox> InputBox;
    TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(STextBlock).Text(FText::FromString(ChannelLabel)).MinDesiredWidth(60) ]
        + SHorizontalBox::Slot().FillWidth(1.0f) [ SAssignNew(InputBox, SEditableTextBox).Text(FText::FromString(DefaultParamName)) ];
    
    ParamNameInputs.Add(Key, InputBox);
    return Widget;
}


void SImport_MM::OnUseParentMIToggled(ECheckBoxState NewState)
{
    bool bUseParent = (NewState == ECheckBoxState::Checked);
    
    if (bUseParent)
    {
        AddLog(TEXT("已切换至 [材质实例实例化] 模式。请确保下方选择了有效的父材质实例，并填写了正确的参数名称。"), FLinearColor::Green);
    }
    else
    {
        AddLog(TEXT("已切换至 [新材质生成] 模式。程序将自动创建材质节点并连接。"), FLinearColor::Green);
    }
}


FReply SImport_MM::OnCreateGenericMaterialClicked()
{
    FString TargetPath = DestPathBox->GetText().ToString();
    if (TargetPath.IsEmpty()) TargetPath = TEXT("/Game/BatchImport");

    // 预加载引擎资源（保留您原始定义的路径）
    UTexture2D* DefNormal = LoadObject<UTexture2D>(nullptr, EngineDefaults::NormalMapPath);
    UTexture2D* DefBlackColor = LoadObject<UTexture2D>(nullptr, EngineDefaults::BlackColorPath);
    UTexture2D* DefBlackLinear = LoadObject<UTexture2D>(nullptr, EngineDefaults::BlackLinearPath);
    UTexture2D* DefWhiteMask = LoadObject<UTexture2D>(nullptr, EngineDefaults::WhiteMaskPath);

    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UMaterialFactoryNew* MatFact = NewObject<UMaterialFactoryNew>();
    UMaterial* MasterMat = Cast<UMaterial>(AT.CreateAsset(EngineDefaults::MasterMatName, TargetPath, UMaterial::StaticClass(), MatFact));
    if (!MasterMat) return FReply::Handled();

    UMaterialEditorOnlyData* MatEditorData = MasterMat->GetEditorOnlyData();

    // 创建常量节点
    auto* ConstZero = NewObject<UMaterialExpressionConstant>(MasterMat);
    MasterMat->GetExpressionCollection().AddExpression(ConstZero);
    auto* ConstOne = NewObject<UMaterialExpressionConstant>(MasterMat);
    ConstOne->R = 1.0f;
    MasterMat->GetExpressionCollection().AddExpression(ConstOne);
    auto* ConstHalf = NewObject<UMaterialExpressionConstant>(MasterMat);
    ConstHalf->R = 0.5f;
    MasterMat->GetExpressionCollection().AddExpression(ConstHalf);
    auto* ConstNormal = NewObject<UMaterialExpressionConstant3Vector>(MasterMat);
    ConstNormal->Constant = FLinearColor(0, 0, 1);
    MasterMat->GetExpressionCollection().AddExpression(ConstNormal);

    auto CreateChannel = [&](FName TexName, FName SwName, int32 Y, EMaterialSamplerType Sampler, UTexture2D* DefTex, UMaterialExpression* Fallback, FExpressionInput& TargetPin)
    {
        auto* TexNode = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
        TexNode->ParameterName = TexName;
        TexNode->SamplerType = Sampler;
        TexNode->Texture = DefTex; // 必须设置一个非空默认值
        TexNode->MaterialExpressionEditorX = -1000;
        TexNode->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(TexNode);

        auto* SwNode = NewObject<UMaterialExpressionStaticSwitchParameter>(MasterMat);
        SwNode->ParameterName = SwName;
        SwNode->MaterialExpressionEditorX = -500;
        SwNode->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(SwNode);

        SwNode->A.Expression = TexNode;
        SwNode->B.Expression = Fallback;
        TargetPin.Expression = SwNode;
    };

    // 构建图表
    CreateChannel(TextureParam::BaseColor, SwitchParam::Use_BaseColor, 0, SAMPLERTYPE_Color, DefBlackColor, ConstZero, MatEditorData->BaseColor);
    CreateChannel(TextureParam::Normal, SwitchParam::Use_Normal, 350, SAMPLERTYPE_Normal, DefNormal, ConstNormal, MatEditorData->Normal);
    CreateChannel(TextureParam::Emissive, SwitchParam::Use_Emissive, 700, SAMPLERTYPE_Color, DefBlackColor, ConstZero, MatEditorData->EmissiveColor);
    CreateChannel(TextureParam::Specular, SwitchParam::Use_Specular, 1050, SAMPLERTYPE_LinearColor, DefBlackLinear, ConstHalf, MatEditorData->Specular);
    CreateChannel(TextureParam::Anisotropy, SwitchParam::Use_Anisotropy, 1400, SAMPLERTYPE_LinearColor, DefBlackLinear, ConstZero, MatEditorData->Anisotropy);
    //CreateChannel(TextureParam::Metallic, SwitchParam::Use_Metallic, 1400, SAMPLERTYPE_Masks, DefBlackLinear, ConstZero, MatEditorData->Anisotropy);
    // 透明度通道连接
    CreateChannel(TextureParam::Opacity, SwitchParam::Use_Opacity, 1750, SAMPLERTYPE_Masks, DefWhiteMask, ConstOne, MatEditorData->OpacityMask);

    // ================== PBR 通道：AO / Roughness / Metallic ==================
    // 三级回退，和 GenerateMaterials 里的赋值逻辑一一对应：
    //   1) Use_ORM=true  -> 从合并的 ORM 贴图对应通道取值 (R=AO, G=Roughness, B=Metallic)
    //   2) Use_ORM=false 且 Use_AO/Use_Roughness/Use_Metallic=true -> 使用对应的独立贴图
    //   3) 以上都不满足 -> 使用常量兜底

    // 合并 ORM 贴图节点（仅方案1使用，三个通道共享同一张贴图、同一个 Use_ORM 参数）
    auto* ORMTex = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
    ORMTex->ParameterName = TextureParam::ORM; ORMTex->SamplerType = SAMPLERTYPE_Masks; ORMTex->Texture = DefWhiteMask;
    ORMTex->MaterialExpressionEditorX = -1600; ORMTex->MaterialExpressionEditorY = 2100;
    MasterMat->GetExpressionCollection().AddExpression(ORMTex);

    auto CreatePBRChannel = [&](FName IndividualParamName, FName UseIndividualSwName, int32 ORMOutputIndex,
                                 UTexture2D* DefIndividualTex, EMaterialSamplerType IndividualSamplerType, UMaterialExpression* ConstFallback,
                                 FExpressionInput& TargetPin, int32 Y)
    {
        // 1. 独立贴图参数节点（不使用ORM合并贴图时，可单独指定的贴图）
        auto* IndividualTex = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
        IndividualTex->ParameterName = IndividualParamName;
        IndividualTex->SamplerType = IndividualSamplerType; // 【修正】按通道传入，不再统一写死 Masks
        IndividualTex->Texture = DefIndividualTex;
        IndividualTex->MaterialExpressionEditorX = -1200;
        IndividualTex->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(IndividualTex);

        // 2. 内层开关：独立贴图 vs 常量兜底
        auto* InnerSw = NewObject<UMaterialExpressionStaticSwitchParameter>(MasterMat);
        InnerSw->ParameterName = UseIndividualSwName;
        InnerSw->MaterialExpressionEditorX = -800;
        InnerSw->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(InnerSw);
        InnerSw->A.Expression = IndividualTex; InnerSw->A.OutputIndex = 1; // 单通道贴图R=G=B
        InnerSw->B.Expression = ConstFallback;

        // 3. 外层开关（三个通道共用同一个 Use_ORM 参数）：ORM合并贴图 vs 内层结果
        auto* OuterSw = NewObject<UMaterialExpressionStaticSwitchParameter>(MasterMat);
        OuterSw->ParameterName = SwitchParam::Use_ORM;
        OuterSw->MaterialExpressionEditorX = -400;
        OuterSw->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(OuterSw);
        OuterSw->A.Expression = ORMTex; OuterSw->A.OutputIndex = ORMOutputIndex;
        OuterSw->B.Expression = InnerSw;

        TargetPin.Expression = OuterSw;
    };

    CreatePBRChannel(TextureParam::AmbientOcclusion, SwitchParam::Use_AO, 1, DefWhiteMask, SAMPLERTYPE_Masks, ConstOne, MatEditorData->AmbientOcclusion, 2100);
    CreatePBRChannel(TextureParam::Roughness, SwitchParam::Use_Roughness, 2, DefWhiteMask, SAMPLERTYPE_Masks, ConstOne, MatEditorData->Roughness, 2400);
    // 【修正】金属度独立贴图节点改为 LinearColor：占位贴图 DefBlackLinear 本身是线性格式，
    // 之前写死 Masks 和占位贴图格式对不上，母材质编译就会报错。
    CreatePBRChannel(TextureParam::Metallic, SwitchParam::Use_Metallic, 3, DefWhiteMask, SAMPLERTYPE_Masks, ConstZero, MatEditorData->Metallic, 2700);

    MasterMat->PostEditChange();
    UMaterialEditingLibrary::RecompileMaterial(MasterMat);

    FString InstanceName = EngineDefaults::MasterInstName;
    UMaterialInstanceConstantFactoryNew* MIFact = NewObject<UMaterialInstanceConstantFactoryNew>();
    UMaterialInstanceConstant* NewMI = Cast<UMaterialInstanceConstant>(AT.CreateAsset(InstanceName, TargetPath, UMaterialInstanceConstant::StaticClass(), MIFact));
    if (NewMI) {
        NewMI->SetParentEditorOnly(MasterMat);
        NewMI->PostEditChange();
        SelectedParentMIPath = FSoftObjectPath(NewMI);
    }
    return FReply::Handled();
}
UMaterialExpressionTextureSampleParameter2D* SImport_MM::AddTextureParameter(UMaterial* InMaterial, FName InParamName, int32 InYPos, EMaterialSamplerType InSamplerType)
{
    auto* Node = Cast<UMaterialExpressionTextureSampleParameter2D>(
        UMaterialEditingLibrary::CreateMaterialExpression(InMaterial, UMaterialExpressionTextureSampleParameter2D::StaticClass())
    );
    Node->ParameterName = InParamName;
    Node->SamplerType = InSamplerType;
    Node->MaterialExpressionEditorX = -400;
    Node->MaterialExpressionEditorY = InYPos;
    return Node;
}