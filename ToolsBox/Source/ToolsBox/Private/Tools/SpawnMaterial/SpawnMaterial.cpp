// Copyright 2026 SuBase. All Rights Reserved.
#include "Tools/SpawnMaterial/SpawnMaterial.h"

// Content Browser
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

// Asset Tools & Registry
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

// Material
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"

// Engine
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"

// Misc
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"

// Data（关键词匹配、通道元数据等共享定义）
#include "Tools/IntelligentImportOfModelsAndMaterials/Data.h"

// Slate
#include "Slate_Assist/FIconStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SSpawnMaterial"


// ============================================================================
// Construct — 构建 UI
// ============================================================================

void SSpawnMaterial::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot().Padding(10)
        [
            SNew(SVerticalBox)

            // ==================== 顶部：标题 + 功能说明 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(10, 10, 10, 5)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("Title", "批量材质球生成"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [
                    SNew(SBorder)
                    .Padding(FMargin(12, 8))
                    .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .AutoWrapText(true)
                            .Text(LOCTEXT("Desc",
                                "本工具将内容浏览器中选中的纹理贴图，根据文件名关键词自动识别贴图通道类型"
                                "（基础色 / 法线 / ORM / 粗糙度 / 金属度 / 自发光 / 透明度等），"
                                "并据此生成材质球。"))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                        [
                            SNew(STextBlock)
                            .AutoWrapText(true)
                            .Text(LOCTEXT("Usage",
                                "使用方法：\n"
                                "  1. 在内容浏览器中选中一个或多个纹理贴图（除此之外也可以加选一个或多个模型）\n"
                                "  2. 在下方设置保存路径与生成模式\n"
                                "  3. 点击「生成材质」按钮（如果选择中包含模型，生成的材质会赋予给对应的模型，前提是模型命名规范）\n"
                                ))
                        ]
                        
                    ]
                ]
            ]

            // ==================== 保存路径 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(10, 5, 10, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SAssignNew(bSaveNextToTexturesCheckbox, SCheckBox)
                    .ToolTipText(LOCTEXT("SaveNextToTexturesTip", "勾选后忽略上方保存位置，材质球保存到选中贴图所在目录"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SaveNextToTextures", "保存到贴图同级目录"))
                    .ToolTipText(LOCTEXT("SaveNextToTexturesTip2", "勾选后忽略上方保存位置，材质球保存到选中贴图所在目录"))
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(10, 2)
            [
                SNew(SHorizontalBox)
                .IsEnabled_Lambda([this](){ return !bSaveNextToTexturesCheckbox->IsChecked(); })
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(LOCTEXT("Dst", "保存位置: ")).MinDesiredWidth(100)
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(DestPathBox, SEditableTextBox).HintText(LOCTEXT("DstHint", "/Game/Materials"))
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton).Text(LOCTEXT("BrowseDst", "选择路径")).OnClicked(this, &SSpawnMaterial::OnBrowseDestClicked)
                ]
            ]

            // ==================== 资产命名详细规则 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(10, 5)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("NamingRules", "资产命名详细规则"))
                .InitiallyCollapsed(true)
                .BodyContent()
                [
                    SNew(SBorder)
                    .Padding(FMargin(10, 5))
                    .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EAssetType::Mesh, TEXT("[ 模型 ]"), TEXT("SM_")) ]
                        + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EAssetType::Texture, TEXT("[ 贴图 ]"), TEXT("T_")) ]
                        + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EAssetType::Material, TEXT("[ 母材质 ]"), TEXT("M_")) ]
                        + SVerticalBox::Slot().AutoHeight() [ CreateNamingRow(EAssetType::Instance, TEXT("[ 材质实例 ]"), TEXT("MI_")) ]
                    ]
                ]
            ]

            // ==================== 模式选择区 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(5)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SAssignNew(bCreateMICheckbox, SCheckBox)
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
                    SAssignNew(bUseParentMICheckbox, SCheckBox)
                    .OnCheckStateChanged(this, &SSpawnMaterial::OnUseParentMIToggled)
                    .IsEnabled_Lambda([this](){ return !bCreateMICheckbox->IsChecked(); })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("UseMI", "使用父材质实例"))
                    .ToolTipText(LOCTEXT("UseMITip", "从已有父材质实例复制并设置贴图参数"))
                ]
            ]

            // ==================== 父材质选择 + 参数名配置 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
            [
                SNew(SBorder).BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                [
                    SNew(SVerticalBox)
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
                        .ToolTipText(LOCTEXT("CreateGenMatTip",
                            "自动创建一个带有标准参数(BaseColor, Normal, ORM, AO, Roughness, Metallic 等)\n"
                            "和静态开关的父材质及其对应的实例，供「使用父材质实例」模式使用。")) 
                        .OnClicked(this, &SSpawnMaterial::OnCreateGenericMaterialClicked)
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
                            .ToolTipText(LOCTEXT("ParamConfigTip", "如果使用自己自定义的材质球，请根据对应通道填写参数名"))
                            .ColorAndOpacity(FSlateColor(FLinearColor::Gray))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                        [
                            SNew(SGridPanel)
                            .FillColumn(1, 1.0f)
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

            // ==================== 生成按钮 ====================
            + SVerticalBox::Slot().AutoHeight().Padding(10, 10)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .Text(LOCTEXT("Generate", "生成材质"))
                .OnClicked(this, &SSpawnMaterial::OnGenerateClicked)
                .ContentPadding(FMargin(40, 5))
            ]

            // ==================== 日志区 ====================
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(10, 5)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(STextBlock).Text(LOCTEXT("LogLabel", "任务日志:")).ColorAndOpacity(FSlateColor(FLinearColor::Gray))
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton).Text(LOCTEXT("Clear", "清空日志")).OnClicked(this, &SSpawnMaterial::OnClearLog)
                    ]
                ]
                + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 5)
                [
                    SNew(SBorder)
                    [
                        SAssignNew(LogScrollBox, SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SAssignNew(LogBox, SMultiLineEditableText)
                            .IsReadOnly(true)
                            .AutoWrapText(true)
                        ]
                    ]
                ]
            ]
        ]
    ];
}


// ============================================================================
// 日志 — 纯文本格式，不含富文本标签
// ============================================================================

void SSpawnMaterial::AddLog(const FString& Message, FLinearColor Color)
{
    FString TimeStr = FDateTime::Now().ToString(TEXT("[%H:%M:%S] "));
    FString FormattedMessage = FString::Printf(TEXT("%s%s\n"), *TimeStr, *Message);
    FText Current = LogBox->GetText();
    LogBox->SetText(FText::FromString(Current.ToString() + FormattedMessage));
    LogScrollBox->ScrollToEnd();
}

FReply SSpawnMaterial::OnClearLog()
{
    LogBox->SetText(FText::GetEmpty());
    return FReply::Handled();
}


// ============================================================================
// 路径选择
// ============================================================================

FReply SSpawnMaterial::OnBrowseDestClicked()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (DP)
    {
        IMainFrameModule& MF = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        void* Parent = MF.GetParentWindow().IsValid() ? MF.GetParentWindow()->GetNativeWindow()->GetOSWindowHandle() : nullptr;
        FString Out;
        if (DP->OpenDirectoryDialog(Parent, TEXT("选择保存位置"), FPaths::ProjectContentDir(), Out))
        {
            FString Pkg;
            if (FPackageName::TryConvertFilenameToLongPackageName(Out, Pkg))
            {
                RelativeDestPath = Pkg;
                DestPathBox->SetText(FText::FromString(RelativeDestPath));
            }
        }
    }
    return FReply::Handled();
}


// ============================================================================
// GetSelectedTextures — 从内容浏览器获取选中的 UTexture2D
// ============================================================================

TArray<UTexture2D*> SSpawnMaterial::GetSelectedTextures()
{
    TArray<UTexture2D*> Result;

    FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FAssetData> SelectedAssets;
    CBModule.Get().GetSelectedAssets(SelectedAssets);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
        {
            Result.Add(Tex);
        }
    }

    return Result;
}


// ============================================================================
// GetSelectedMeshes — 从内容浏览器获取选中的 UStaticMesh
// ============================================================================

TArray<UStaticMesh*> SSpawnMaterial::GetSelectedMeshes()
{
    TArray<UStaticMesh*> Result;

    FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FAssetData> SelectedAssets;
    CBModule.Get().GetSelectedAssets(SelectedAssets);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
        {
            Result.Add(Mesh);
        }
    }

    return Result;
}


// ============================================================================
// AssignMaterialsToMeshes — 按名称关键词匹配将材质赋予模型
// ============================================================================

void SSpawnMaterial::AssignMaterialsToMeshes(const TMap<FString, UMaterialInterface*>& CreatedMaterials, const TArray<UStaticMesh*>& Meshes)
{
    if (CreatedMaterials.Num() == 0 || Meshes.Num() == 0) return;

    // ----------------------------------------------------------------
    // 核心名提取：从带多层前缀/后缀的名字中还原「原始名」
    //
    // 命名结构示例：
    //   贴图:   T_OriginalName_TextureSuffix     (如 T_Door_BaseColor)
    //   材质key: OriginalName                     (如 Door，贴图前缀已剥离)
    //   材质名: M_OriginalName                    (如 M_Door)
    //   模型:   SM_ParentName_OriginalName        (如 SM_BalconyDoor_Door)
    //
    // 提取规则：依次尝试剥离各类型(材质/实例/模型/贴图)的前缀和后缀，
    //           然后取最后一个下划线右侧的部分作为「核心名」
    // ----------------------------------------------------------------
    auto ExtractCoreName = [this](FString Name) -> FString
    {
        // 依次尝试剥离各类型的前缀/后缀（哪个匹配就剥哪个）
        Name = StripNamingAffixes(Name, EAssetType::Material);
        Name = StripNamingAffixes(Name, EAssetType::Instance);
        Name = StripNamingAffixes(Name, EAssetType::Mesh);
        Name = StripNamingAffixes(Name, EAssetType::Texture);

        // 剥离残留的贴图类型后缀（如 _BaseColor / _BC 等）
        for (const FString& S : BaseColorSuffixes::All())
        {
            if (Name.EndsWith(S, ESearchCase::IgnoreCase))
            {
                Name = Name.LeftChop(S.Len());
                break;
            }
        }

        // 取最后一个下划线右侧的部分作为核心名
        int32 LastUnderscore = Name.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (LastUnderscore != INDEX_NONE)
        {
            FString Core = Name.RightChop(LastUnderscore + 1);
            if (!Core.IsEmpty())
                return Core;
        }
        return Name;
    };

    // 预计算每个材质的核心名，建立 核心名(小写) -> 材质 映射
    TMap<FString, UMaterialInterface*> CoreToMaterial;
    for (auto& Pair : CreatedMaterials)
    {
        FString Core = ExtractCoreName(Pair.Key).ToLower();
        if (!Core.IsEmpty() && !CoreToMaterial.Contains(Core))
        {
            CoreToMaterial.Add(Core, Pair.Value);
            AddLog(FString::Printf(TEXT("材质 [%s] → 核心名: \"%s\""), *Pair.Key, *Core), FLinearColor::Gray);
        }
    }

    int32 AssignedCount = 0;

    for (UStaticMesh* SM : Meshes)
    {
        if (!IsValid(SM)) continue;

        FString MeshName = SM->GetName();

        // 对模型名同样提取核心名（剥离 SM_ 等前缀后取最后一段）
        FString MeshCore = ExtractCoreName(MeshName).ToLower();

        bool bAssigned = false;

        // ===== 主匹配：模型核心名 == 材质核心名（精确比对） =====
        if (CoreToMaterial.Contains(MeshCore))
        {
            UMaterialInterface* Mat = CoreToMaterial[MeshCore];
            if (IsValid(Mat))
            {
                for (int32 i = 0; i < SM->GetStaticMaterials().Num(); ++i)
                    SM->SetMaterial(i, Mat);
                SM->PostEditChange();
                AddLog(FString::Printf(TEXT("模型 [%s] → 核心名: \"%s\" ← 材质 [%s]"), *MeshName, *MeshCore, *Mat->GetName()), FLinearColor::Green);
                bAssigned = true;
                AssignedCount++;
            }
            else
            {
                AddLog(FString::Printf(TEXT("模型 [%s] 匹配到核心名 [%s] 但材质无效，跳过。"), *MeshName, *MeshCore), FLinearColor::Red);
                bAssigned = true;
            }
        }

        // ===== 次匹配：模型名的某个完整段 == 材质核心名（整段匹配，非子串） =====
        // 处理核心名不在最后一段的情况（如 SM_Door_BalconyDoor）
        if (!bAssigned)
        {
            TArray<FString> Segments;
            MeshName.ParseIntoArray(Segments, TEXT("_"), true);
            TArray<FString> LowerSegments;
            for (const FString& Seg : Segments)
                LowerSegments.Add(Seg.ToLower());

            for (auto& Pair : CoreToMaterial)
            {
                if (LowerSegments.Contains(Pair.Key))
                {
                    UMaterialInterface* Mat = Pair.Value;
                    if (IsValid(Mat))
                    {
                        for (int32 i = 0; i < SM->GetStaticMaterials().Num(); ++i)
                            SM->SetMaterial(i, Mat);
                        SM->PostEditChange();
                        AddLog(FString::Printf(TEXT("模型 [%s] → 段匹配: \"%s\" ← 材质 [%s]"), *MeshName, *Pair.Key, *Mat->GetName()), FLinearColor::Green);
                        bAssigned = true;
                        AssignedCount++;
                    }
                    break;
                }
            }
        }

        // ===== 兜底：只有一组材质时，未匹配的模型也赋予该材质 =====
        if (!bAssigned && CreatedMaterials.Num() == 1)
        {
            auto It = CreatedMaterials.CreateConstIterator();
            if (It && IsValid(It->Value))
            {
                for (int32 i = 0; i < SM->GetStaticMaterials().Num(); ++i)
                    SM->SetMaterial(i, It->Value);
                SM->PostEditChange();
                AddLog(FString::Printf(TEXT("模型 [%s] ← 材质 [%s]（唯一材质自动适配）"), *MeshName, *It->Value->GetName()), FLinearColor::Yellow);
                AssignedCount++;
            }
        }
        else if (!bAssigned)
        {
            AddLog(FString::Printf(TEXT("模型 [%s] → 核心名: \"%s\" 未匹配到任何材质，跳过。"), *MeshName, *MeshCore), FLinearColor::Red);
        }
    }

    AddLog(FString::Printf(TEXT("材质赋予完成：成功 %d / 共 %d 个模型"), AssignedCount, Meshes.Num()), FLinearColor::White);
}

FReply SSpawnMaterial::OnGenerateClicked()
{
    // 1. 获取选中的贴图
    TArray<UTexture2D*> SelectedTextures = GetSelectedTextures();
    if (SelectedTextures.Num() == 0)
    {
        AddLog(TEXT("错误：未在内容浏览器中选中任何纹理贴图，请先选择贴图再执行。"), FLinearColor::Red);
        return FReply::Handled();
    }

    AddLog(FString::Printf(TEXT("--- 开始生成材质（共选中 %d 张贴图）---"), SelectedTextures.Num()), FLinearColor::White);

    // 2. 确定保存路径
    bool bSaveNextToTextures = bSaveNextToTexturesCheckbox.IsValid() ? bSaveNextToTexturesCheckbox->IsChecked() : false;
    FString DefaultSavePath = DestPathBox->GetText().ToString();
    if (DefaultSavePath.IsEmpty()) DefaultSavePath = TEXT("/Game/Materials");

    // 3. 读取模式选项
    bool bUseExistingParent = bUseParentMICheckbox.IsValid() ? bUseParentMICheckbox->IsChecked() : false;
    bool bAutoCreateInstance = bCreateMICheckbox.IsValid() ? bCreateMICheckbox->IsChecked() : false;
    UMaterialInterface* ParentMI = Cast<UMaterialInterface>(SelectedParentMIPath.TryLoad());

    if (bUseExistingParent && !ParentMI)
    {
        AddLog(TEXT("错误：已勾选「使用父材质实例」但未选择有效的父材质实例，请先选择或生成。"), FLinearColor::Red);
        return FReply::Handled();
    }

    // 4. 分类并分组
    TArray<FString> BaseColorRawNames;
    TMap<FString, TMap<FString, UTexture2D*>> Groups;
    TMap<FString, bool> HasOpacityMap;
    int32 BaseColorCount = 0;

    CategorizeAndGroupTextures(SelectedTextures, BaseColorRawNames, Groups, HasOpacityMap, BaseColorCount);

    if (BaseColorCount == 0)
    {
        AddLog(TEXT("警告：未找到包含基础色关键词(base/albedo/color/diffuse)的贴图，无法推导材质名，已跳过。"), FLinearColor::Red);
        AddLog(TEXT("--- 生成结束 ---"), FLinearColor::White);
        return FReply::Handled();
    }

    // 5. 为每个分组生成材质
    int32 SuccessCount = 0;
    TMap<FString, UMaterialInterface*> CreatedMaterials; // 材质基础名 -> 生成的材质
    for (const FString& BCRawName : BaseColorRawNames)
    {
        if (!Groups.Contains(BCRawName)) continue;

        const TMap<FString, UTexture2D*>& LocalMatch = Groups[BCRawName];
        bool bHasOpacity = HasOpacityMap.Contains(BCRawName) ? HasOpacityMap[BCRawName] : false;

        // 推导材质基础名：剥离 BC 后缀
        FString MatNameBase = BCRawName;
        for (const FString& S : BaseColorSuffixes::All())
        {
            if (MatNameBase.EndsWith(S, ESearchCase::IgnoreCase))
            {
                MatNameBase = MatNameBase.LeftChop(S.Len());
                break;
            }
        }
        if (MatNameBase.IsEmpty()) MatNameBase = BCRawName;

        // 剥离贴图命名前缀/后缀（如 T_ 前缀），避免材质名叠两层前缀（M_T_Door → M_Door）
        FString CleanMatName = StripNamingAffixes(MatNameBase, EAssetType::Texture);
        if (CleanMatName.IsEmpty()) CleanMatName = MatNameBase;

        AddLog(FString::Printf(TEXT("正在生成材质: %s"), *CleanMatName), FLinearColor::White);

        // 勾选"保存到贴图同级目录"时，从 BC 贴图所在包路径推导保存位置
        FString CurrentSavePath = DefaultSavePath;
        if (bSaveNextToTextures)
        {
            // 优先用 BC 贴图的路径，其次用组内任意贴图的路径
            const UTexture2D* PathTex = LocalMatch.Contains(ChannelKey::BC) ? LocalMatch[ChannelKey::BC] : nullptr;
            if (!PathTex)
            {
                for (auto& Pair : LocalMatch)
                {
                    if (Pair.Value) { PathTex = Pair.Value; break; }
                }
            }
            if (PathTex)
            {
                CurrentSavePath = FPackageName::GetLongPackagePath(PathTex->GetOutermost()->GetName());
            }
        }

        UMaterialInterface* GeneratedMat = GenerateMaterialForGroup(
            CleanMatName, LocalMatch, bHasOpacity, CurrentSavePath,
            bUseExistingParent, bAutoCreateInstance, ParentMI);

        if (GeneratedMat)
        {
            SuccessCount++;
            CreatedMaterials.Add(CleanMatName, GeneratedMat);
            AddLog(FString::Printf(TEXT("成功：已生成材质 [%s]"), *GeneratedMat->GetName()), FLinearColor::Green);
        }
        else
        {
            AddLog(FString::Printf(TEXT("失败：材质 [%s] 生成失败，请检查日志。"), *CleanMatName), FLinearColor::Red);
        }
    }

    AddLog(FString::Printf(TEXT("--- 生成结束，成功 %d / 共 %d 组 ---"), SuccessCount, BaseColorCount), FLinearColor::White);

    // 6. 如果同时选中了模型，按名称关键词匹配赋予材质
    TArray<UStaticMesh*> SelectedMeshes = GetSelectedMeshes();
    if (SelectedMeshes.Num() > 0 && CreatedMaterials.Num() > 0)
    {
        AddLog(FString::Printf(TEXT("--- 开始赋予材质到模型（共 %d 个模型）---"), SelectedMeshes.Num()), FLinearColor::White);
        AssignMaterialsToMeshes(CreatedMaterials, SelectedMeshes);
    }

    return FReply::Handled();
}


// ============================================================================
// CategorizeAndGroupTextures — 分类并分组
// ============================================================================

void SSpawnMaterial::CategorizeAndGroupTextures(
    const TArray<UTexture2D*>& Textures,
    TArray<FString>& OutBaseColorRawNames,
    TMap<FString, TMap<FString, UTexture2D*>>& OutGroups,
    TMap<FString, bool>& OutHasOpacityMap,
    int32& OutBaseColorCount)
{
    OutBaseColorCount = 0;

    // 1. 识别所有基础色贴图，作为分组锚点
    for (UTexture2D* Tex : Textures)
    {
        if (!IsValid(Tex)) continue;
        FString AssetName = Tex->GetName();
        FString LowerName = AssetName.ToLower();

        if (TextureMatch::ContainsAny(LowerName, TextureMatch::BC()))
        {
            OutBaseColorRawNames.Add(AssetName);
            OutBaseColorCount++;
        }
    }

    // 2. 为每个 BC 贴图推导材质基础名和分组前缀
    for (const FString& BCRawName : OutBaseColorRawNames)
    {
        // 剥离 BC 后缀得到材质命名基础名
        FString MatNameBase = BCRawName;
        for (const FString& S : BaseColorSuffixes::All())
        {
            if (MatNameBase.EndsWith(S, ESearchCase::IgnoreCase))
            {
                MatNameBase = MatNameBase.LeftChop(S.Len());
                break;
            }
        }

        // 分组前缀（多组时用于过滤贴图）
        FString CleanPrefix = BCRawName;
        if (OutBaseColorCount > 1)
        {
            for (const FString& S : BaseColorSuffixes::All())
            {
                if (CleanPrefix.EndsWith(S, ESearchCase::IgnoreCase))
                {
                    CleanPrefix = CleanPrefix.LeftChop(S.Len());
                    break;
                }
            }
        }

        TMap<FString, UTexture2D*> LocalMatch;
        bool bHasOpacity = false;

        // 3. 遍历所有贴图，匹配到当前分组
        for (UTexture2D* Tex : Textures)
        {
            if (!IsValid(Tex)) continue;
            FString TexName = Tex->GetName();

            // 多组时：贴图名必须包含当前组的 CleanPrefix
            if (OutBaseColorCount > 1 && !TexName.Contains(CleanPrefix)) continue;

            FString L = TexName.ToLower();

            // 使用 Data.h 中定义的匹配规则进行纹理分类
            if (TextureMatch::ContainsAny(L, TextureMatch::BC()))
                LocalMatch.Add(ChannelKey::BC, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::N()))
                LocalMatch.Add(ChannelKey::N, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::EM()))
                LocalMatch.Add(ChannelKey::EM, Tex);
            else if (TextureMatch::ContainsAny(L, TextureMatch::OP()))
            {
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

        OutGroups.Add(BCRawName, LocalMatch);
        OutHasOpacityMap.Add(BCRawName, bHasOpacity);
    }
}


// ============================================================================
// GenerateMaterialForGroup — 分派到 ORM / 非 ORM 路径
// ============================================================================

UMaterialInterface* SSpawnMaterial::GenerateMaterialForGroup(
    const FString& MatNameBase,
    const TMap<FString, UTexture2D*>& LocalMatch,
    bool bHasOpacity,
    const FString& SavePath,
    bool bUseExistingParent,
    bool bAutoCreateInstance,
    UMaterialInterface* ParentMI)
{
    bool bFoundORM = LocalMatch.Contains(ChannelKey::ORM);

    if (bFoundORM)
    {
        return GenerateMaterialWithORM(MatNameBase, LocalMatch, bHasOpacity, SavePath,
                                       bUseExistingParent, bAutoCreateInstance, ParentMI);
    }
    else
    {
        return GenerateMaterialWithoutORM(MatNameBase, LocalMatch, bHasOpacity, SavePath,
                                          bUseExistingParent, bAutoCreateInstance, ParentMI);
    }
}


// ============================================================================
// GenerateMaterialWithoutORM — 无 ORM 贴图时生成材质
// ============================================================================

UMaterialInterface* SSpawnMaterial::GenerateMaterialWithoutORM(
    const FString& BCRawName,
    const TMap<FString, UTexture2D*>& LocalMatch,
    bool bHasOpacity,
    const FString& SavePath,
    bool bUseExistingParent,
    bool bAutoCreateInstance,
    UMaterialInterface* ParentMI)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // --- 分支 A：材质实例模式（从父 MI 复制并设置参数） ---
    if (bUseExistingParent && ParentMI)
    {
        FString MIName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Instance), SavePath);
        UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AT.DuplicateAsset(MIName, SavePath, ParentMI));

        if (MIC)
        {
            FStaticParameterSet NewStaticParameters;
            MIC->GetStaticParameterValues(NewStaticParameters);

            // 强制设置静态开关
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
                FStaticSwitchParameter NewSwitch;
                NewSwitch.ParameterInfo = FMaterialParameterInfo(SwitchParamName);
                NewSwitch.Value = bValue;
                NewSwitch.bOverride = true;
                NewStaticParameters.StaticSwitchParameters.Add(NewSwitch);
            };

            // 应用贴图并设置压缩
            auto ApplyTexture = [&](const FString& LocalKey, FName DefaultParamName, UTexture2D* T)
            {
                if (!T) return;
                const FChannelMeta* Meta = FindChannelMeta(LocalKey);
                bool bIsSRGB = Meta ? Meta->bSRGB : false;
                T->SRGB = bIsSRGB;
                T->CompressionSettings = Meta ? (TextureCompressionSettings)Meta->CompressionSettings : TC_Default;
                T->PostEditChange();

                FString PName = ParamNameInputs.Contains(LocalKey) ? ParamNameInputs[LocalKey]->GetText().ToString() : DefaultParamName.ToString();
                if (PName.IsEmpty()) PName = DefaultParamName.ToString();

                MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(*PName)), T);
            };

            // 设置基础通道开关
            bool bFoundBC = LocalMatch.Contains(ChannelKey::BC);
            bool bFoundN  = LocalMatch.Contains(ChannelKey::N);
            bool bFoundEM = LocalMatch.Contains(ChannelKey::EM);
            bool bFoundOP = LocalMatch.Contains(ChannelKey::OP);

            ForceSetSwitch(SwitchParam::Use_BaseColor, bFoundBC);
            ForceSetSwitch(SwitchParam::Use_Normal,    bFoundN);
            ForceSetSwitch(SwitchParam::Use_Emissive,  bFoundEM);
            ForceSetSwitch(SwitchParam::Use_Opacity,   bFoundOP);

            // 无 ORM：关闭 ORM 总开关，使用独立通道
            ForceSetSwitch(SwitchParam::Use_ORM,       false);
            ForceSetSwitch(SwitchParam::Use_AO,        LocalMatch.Contains(ChannelKey::AO));
            ForceSetSwitch(SwitchParam::Use_Roughness, LocalMatch.Contains(ChannelKey::R));
            ForceSetSwitch(SwitchParam::Use_Metallic,  LocalMatch.Contains(ChannelKey::M));

            // 应用贴图
            if (bFoundBC) ApplyTexture(ChannelKey::BC, TextureParam::BaseColor, LocalMatch[ChannelKey::BC]);
            if (bFoundN)  ApplyTexture(ChannelKey::N,  TextureParam::Normal,    LocalMatch[ChannelKey::N]);
            if (bFoundEM) ApplyTexture(ChannelKey::EM, TextureParam::Emissive,  LocalMatch[ChannelKey::EM]);
            if (bFoundOP) ApplyTexture(ChannelKey::OP, TextureParam::Opacity,   LocalMatch[ChannelKey::OP]);
            if (LocalMatch.Contains(ChannelKey::AO)) ApplyTexture(ChannelKey::AO, TextureParam::AmbientOcclusion, LocalMatch[ChannelKey::AO]);
            if (LocalMatch.Contains(ChannelKey::R))  ApplyTexture(ChannelKey::R,  TextureParam::Roughness,        LocalMatch[ChannelKey::R]);
            if (LocalMatch.Contains(ChannelKey::M))  ApplyTexture(ChannelKey::M,  TextureParam::Metallic,         LocalMatch[ChannelKey::M]);

            if (bHasOpacity)
            {
                MIC->BasePropertyOverrides.bOverride_BlendMode = true;
                MIC->BasePropertyOverrides.BlendMode = BLEND_Masked;
            }

            MIC->UpdateStaticPermutation(NewStaticParameters);
            MIC->PostEditChange();
            return MIC;
        }
    }
    // --- 分支 B：母材质连线模式 ---
    else
    {
        FString FinalMatName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Material), SavePath);
        UMaterial* NewMat = Cast<UMaterial>(AT.CreateAsset(FinalMatName, SavePath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>()));

        if (NewMat)
        {
            if (bHasOpacity) NewMat->BlendMode = BLEND_Masked;

            int32 YPos = 0;
            for (auto& Pair : LocalMatch)
            {
                UTexture2D* T = Pair.Value;
                auto* Node = Cast<UMaterialExpressionTextureSample>(
                    UMaterialEditingLibrary::CreateMaterialExpression(NewMat, UMaterialExpressionTextureSample::StaticClass()));
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

            // 可选：在母材质基础上创建实例
            if (bAutoCreateInstance)
            {
                FString FinalInstName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Instance), SavePath);
                UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(
                    AT.CreateAsset(FinalInstName, SavePath, UMaterialInstanceConstant::StaticClass(),
                                   NewObject<UMaterialInstanceConstantFactoryNew>()));
                if (NewMIC)
                {
                    NewMIC->SetParentEditorOnly(NewMat);
                    NewMIC->PostEditChange();
                    return NewMIC;
                }
            }
            return NewMat;
        }
    }

    return nullptr;
}


// ============================================================================
// GenerateMaterialWithORM — 有 ORM 贴图时生成材质
// ============================================================================

UMaterialInterface* SSpawnMaterial::GenerateMaterialWithORM(
    const FString& BCRawName,
    const TMap<FString, UTexture2D*>& LocalMatch,
    bool bHasOpacity,
    const FString& SavePath,
    bool bUseExistingParent,
    bool bAutoCreateInstance,
    UMaterialInterface* ParentMI)
{
    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // --- 分支 A：材质实例模式 ---
    if (bUseExistingParent && ParentMI)
    {
        FString MIName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Instance), SavePath);
        UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AT.DuplicateAsset(MIName, SavePath, ParentMI));

        if (MIC)
        {
            FStaticParameterSet NewStaticParameters;
            MIC->GetStaticParameterValues(NewStaticParameters);

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
                FStaticSwitchParameter NewSwitch;
                NewSwitch.ParameterInfo = FMaterialParameterInfo(SwitchParamName);
                NewSwitch.Value = bValue;
                NewSwitch.bOverride = true;
                NewStaticParameters.StaticSwitchParameters.Add(NewSwitch);
            };

            auto ApplyTexture = [&](const FString& LocalKey, FName DefaultParamName, UTexture2D* T)
            {
                if (!T) return;
                const FChannelMeta* Meta = FindChannelMeta(LocalKey);
                bool bIsSRGB = Meta ? Meta->bSRGB : false;
                T->SRGB = bIsSRGB;
                T->CompressionSettings = Meta ? (TextureCompressionSettings)Meta->CompressionSettings : TC_Default;
                T->PostEditChange();

                FString PName = ParamNameInputs.Contains(LocalKey) ? ParamNameInputs[LocalKey]->GetText().ToString() : DefaultParamName.ToString();
                if (PName.IsEmpty()) PName = DefaultParamName.ToString();

                MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(*PName)), T);
            };

            // 基础通道开关
            bool bFoundBC = LocalMatch.Contains(ChannelKey::BC);
            bool bFoundN  = LocalMatch.Contains(ChannelKey::N);
            bool bFoundEM = LocalMatch.Contains(ChannelKey::EM);
            bool bFoundOP = LocalMatch.Contains(ChannelKey::OP);

            ForceSetSwitch(SwitchParam::Use_BaseColor, bFoundBC);
            ForceSetSwitch(SwitchParam::Use_Normal,    bFoundN);
            ForceSetSwitch(SwitchParam::Use_Emissive,  bFoundEM);
            ForceSetSwitch(SwitchParam::Use_Opacity,   bFoundOP);

            // 有 ORM：开启 ORM 总开关，关闭独立通道
            ForceSetSwitch(SwitchParam::Use_ORM,       true);
            ForceSetSwitch(SwitchParam::Use_AO,        false);
            ForceSetSwitch(SwitchParam::Use_Roughness, false);
            ForceSetSwitch(SwitchParam::Use_Metallic,  false);

            // 应用贴图
            if (bFoundBC) ApplyTexture(ChannelKey::BC, TextureParam::BaseColor, LocalMatch[ChannelKey::BC]);
            if (bFoundN)  ApplyTexture(ChannelKey::N,  TextureParam::Normal,    LocalMatch[ChannelKey::N]);
            if (bFoundEM) ApplyTexture(ChannelKey::EM, TextureParam::Emissive,  LocalMatch[ChannelKey::EM]);
            if (bFoundOP) ApplyTexture(ChannelKey::OP, TextureParam::Opacity,   LocalMatch[ChannelKey::OP]);

            // ORM 贴图
            ApplyTexture(ChannelKey::ORM, TextureParam::ORM, LocalMatch[ChannelKey::ORM]);

            if (bHasOpacity)
            {
                MIC->BasePropertyOverrides.bOverride_BlendMode = true;
                MIC->BasePropertyOverrides.BlendMode = BLEND_Masked;
            }

            MIC->UpdateStaticPermutation(NewStaticParameters);
            MIC->PostEditChange();
            return MIC;
        }
    }
    // --- 分支 B：母材质连线模式 ---
    else
    {
        FString FinalMatName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Material), SavePath);
        UMaterial* NewMat = Cast<UMaterial>(AT.CreateAsset(FinalMatName, SavePath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>()));

        if (NewMat)
        {
            if (bHasOpacity) NewMat->BlendMode = BLEND_Masked;

            int32 YPos = 0;
            for (auto& Pair : LocalMatch)
            {
                UTexture2D* T = Pair.Value;
                auto* Node = Cast<UMaterialExpressionTextureSample>(
                    UMaterialEditingLibrary::CreateMaterialExpression(NewMat, UMaterialExpressionTextureSample::StaticClass()));
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
                        // ORM 贴图：R=AO, G=Roughness, B=Metallic
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

            if (bAutoCreateInstance)
            {
                FString FinalInstName = MakeUniqueAssetName(GetAppliedName(BCRawName, EAssetType::Instance), SavePath);
                UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(
                    AT.CreateAsset(FinalInstName, SavePath, UMaterialInstanceConstant::StaticClass(),
                                   NewObject<UMaterialInstanceConstantFactoryNew>()));
                if (NewMIC)
                {
                    NewMIC->SetParentEditorOnly(NewMat);
                    NewMIC->PostEditChange();
                    return NewMIC;
                }
            }
            return NewMat;
        }
    }

    return nullptr;
}


// ============================================================================
// OnCreateGenericMaterialClicked — 生成通用父材质及实例
// ============================================================================

FReply SSpawnMaterial::OnCreateGenericMaterialClicked()
{
    FString TargetPath = DestPathBox->GetText().ToString();
    if (TargetPath.IsEmpty()) TargetPath = TEXT("/Game/Materials");

    // 勾选"保存到贴图同级目录"时，从选中贴图所在包路径推导保存位置
    if (bSaveNextToTexturesCheckbox.IsValid() && bSaveNextToTexturesCheckbox->IsChecked())
    {
        TArray<UTexture2D*> SelectedTextures = GetSelectedTextures();
        if (SelectedTextures.Num() > 0)
        {
            TargetPath = FPackageName::GetLongPackagePath(SelectedTextures[0]->GetOutermost()->GetName());
        }
    }

    // 预加载引擎默认贴图
    UTexture2D* DefNormal      = LoadObject<UTexture2D>(nullptr, EngineDefaults::NormalMapPath);
    UTexture2D* DefBlackColor  = LoadObject<UTexture2D>(nullptr, EngineDefaults::BlackColorPath);
    UTexture2D* DefBlackLinear = LoadObject<UTexture2D>(nullptr, EngineDefaults::BlackLinearPath);
    UTexture2D* DefWhiteMask   = LoadObject<UTexture2D>(nullptr, EngineDefaults::WhiteMaskPath);

    IAssetTools& AT = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UMaterialFactoryNew* MatFact = NewObject<UMaterialFactoryNew>();
    UMaterial* MasterMat = Cast<UMaterial>(AT.CreateAsset(EngineDefaults::MasterMatName, TargetPath, UMaterial::StaticClass(), MatFact));
    if (!MasterMat)
    {
        AddLog(TEXT("错误：通用父材质创建失败。"), FLinearColor::Red);
        return FReply::Handled();
    }

    UMaterialEditorOnlyData* MatEditorData = MasterMat->GetEditorOnlyData();

    // 常量节点
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

    // 通道创建 Lambda：贴图参数 + 静态开关 + 连接到材质属性
    auto CreateChannel = [&](FName TexName, FName SwName, int32 Y, EMaterialSamplerType Sampler, UTexture2D* DefTex, UMaterialExpression* Fallback, FExpressionInput& TargetPin)
    {
        auto* TexNode = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
        TexNode->ParameterName = TexName;
        TexNode->SamplerType = Sampler;
        TexNode->Texture = DefTex;
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

    // 构建基础通道
    CreateChannel(TextureParam::BaseColor,  SwitchParam::Use_BaseColor,  0,    SAMPLERTYPE_Color,       DefBlackColor,  ConstZero,   MatEditorData->BaseColor);
    CreateChannel(TextureParam::Normal,     SwitchParam::Use_Normal,     350,  SAMPLERTYPE_Normal,      DefNormal,      ConstNormal, MatEditorData->Normal);
    CreateChannel(TextureParam::Emissive,   SwitchParam::Use_Emissive,   700,  SAMPLERTYPE_Color,       DefBlackColor,  ConstZero,   MatEditorData->EmissiveColor);
    CreateChannel(TextureParam::Specular,   SwitchParam::Use_Specular,   1050, SAMPLERTYPE_LinearColor, DefBlackLinear, ConstHalf,   MatEditorData->Specular);
    CreateChannel(TextureParam::Anisotropy, SwitchParam::Use_Anisotropy, 1400, SAMPLERTYPE_LinearColor, DefBlackLinear, ConstZero,   MatEditorData->Anisotropy);
    CreateChannel(TextureParam::Opacity,    SwitchParam::Use_Opacity,    1750, SAMPLERTYPE_Masks,       DefWhiteMask,   ConstOne,    MatEditorData->OpacityMask);

    // ORM 合并贴图节点（三个通道共享）
    auto* ORMTex = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
    ORMTex->ParameterName = TextureParam::ORM;
    ORMTex->SamplerType = SAMPLERTYPE_Masks;
    ORMTex->Texture = DefWhiteMask;
    ORMTex->MaterialExpressionEditorX = -1600;
    ORMTex->MaterialExpressionEditorY = 2100;
    MasterMat->GetExpressionCollection().AddExpression(ORMTex);

    // PBR 通道：三级回退（ORM合并贴图 > 独立贴图 > 常量兜底）
    auto CreatePBRChannel = [&](FName IndividualParamName, FName UseIndividualSwName, int32 ORMOutputIndex,
                                 UTexture2D* DefIndividualTex, EMaterialSamplerType IndividualSamplerType, UMaterialExpression* ConstFallback,
                                 FExpressionInput& TargetPin, int32 Y)
    {
        auto* IndividualTex = NewObject<UMaterialExpressionTextureSampleParameter2D>(MasterMat);
        IndividualTex->ParameterName = IndividualParamName;
        IndividualTex->SamplerType = IndividualSamplerType;
        IndividualTex->Texture = DefIndividualTex;
        IndividualTex->MaterialExpressionEditorX = -1200;
        IndividualTex->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(IndividualTex);

        auto* InnerSw = NewObject<UMaterialExpressionStaticSwitchParameter>(MasterMat);
        InnerSw->ParameterName = UseIndividualSwName;
        InnerSw->MaterialExpressionEditorX = -800;
        InnerSw->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(InnerSw);
        InnerSw->A.Expression = IndividualTex; InnerSw->A.OutputIndex = 1;
        InnerSw->B.Expression = ConstFallback;

        auto* OuterSw = NewObject<UMaterialExpressionStaticSwitchParameter>(MasterMat);
        OuterSw->ParameterName = SwitchParam::Use_ORM;
        OuterSw->MaterialExpressionEditorX = -400;
        OuterSw->MaterialExpressionEditorY = Y;
        MasterMat->GetExpressionCollection().AddExpression(OuterSw);
        OuterSw->A.Expression = ORMTex; OuterSw->A.OutputIndex = ORMOutputIndex;
        OuterSw->B.Expression = InnerSw;

        TargetPin.Expression = OuterSw;
    };

    CreatePBRChannel(TextureParam::AmbientOcclusion, SwitchParam::Use_AO,        1, DefWhiteMask, SAMPLERTYPE_Masks,       ConstOne,  MatEditorData->AmbientOcclusion, 2100);
    CreatePBRChannel(TextureParam::Roughness,         SwitchParam::Use_Roughness, 2, DefWhiteMask, SAMPLERTYPE_Masks,       ConstOne,  MatEditorData->Roughness,         2400);
    CreatePBRChannel(TextureParam::Metallic,          SwitchParam::Use_Metallic,  3, DefWhiteMask, SAMPLERTYPE_Masks,       ConstZero, MatEditorData->Metallic,          2700);

    MasterMat->PostEditChange();
    UMaterialEditingLibrary::RecompileMaterial(MasterMat);

    // 创建对应实例
    FString InstanceName = EngineDefaults::MasterInstName;
    UMaterialInstanceConstantFactoryNew* MIFact = NewObject<UMaterialInstanceConstantFactoryNew>();
    UMaterialInstanceConstant* NewMI = Cast<UMaterialInstanceConstant>(AT.CreateAsset(InstanceName, TargetPath, UMaterialInstanceConstant::StaticClass(), MIFact));
    if (NewMI)
    {
        NewMI->SetParentEditorOnly(MasterMat);
        NewMI->PostEditChange();
        SelectedParentMIPath = FSoftObjectPath(NewMI);
        AddLog(FString::Printf(TEXT("成功：已生成通用父材质 [%s] 及实例 [%s]"), EngineDefaults::MasterMatName, EngineDefaults::MasterInstName), FLinearColor::Green);
    }

    return FReply::Handled();
}


// ============================================================================
// 资产名辅助函数
// ============================================================================

bool SSpawnMaterial::DoesAssetExistInPath(const FString& AssetName, const FString& PackagePath) const
{
    FString ObjectPath = (PackagePath / AssetName) + TEXT(".") + AssetName;

    if (FindObject<UObject>(nullptr, *ObjectPath))
    {
        return true;
    }

    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    return ARM.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid();
}

FString SSpawnMaterial::MakeUniqueAssetName(const FString& DesiredName, const FString& PackagePath) const
{
    if (!DoesAssetExistInPath(DesiredName, PackagePath))
    {
        return DesiredName;
    }

    int32 Suffix = 1;
    FString UniqueName;
    do
    {
        UniqueName = FString::Printf(TEXT("%s_%d"), *DesiredName, Suffix++);
    } while (DoesAssetExistInPath(UniqueName, PackagePath));

    return UniqueName;
}


// ============================================================================
// UI 辅助函数
// ============================================================================

TSharedRef<SWidget> SSpawnMaterial::CreateParamInputRow(const FString& ChannelLabel, const FString& DefaultParamName, const FString& Key)
{
    TSharedPtr<SEditableTextBox> InputBox;
    TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(FText::FromString(ChannelLabel)).MinDesiredWidth(60)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SAssignNew(InputBox, SEditableTextBox).Text(FText::FromString(DefaultParamName))
        ];

    ParamNameInputs.Add(Key, InputBox);
    return Widget;
}

void SSpawnMaterial::OnUseParentMIToggled(ECheckBoxState NewState)
{
    bool bUseParent = (NewState == ECheckBoxState::Checked);

    if (bUseParent)
    {
        AddLog(TEXT("已切换至 [材质实例实例化] 模式。请确保选择了有效的父材质实例，并填写了正确的参数名称。"), FLinearColor::Green);
    }
    else
    {
        AddLog(TEXT("已切换至 [新材质生成] 模式。程序将自动创建材质节点并连接。"), FLinearColor::Green);
    }
}


// ============================================================================
// 命名规则 — CreateNamingRow / GetAppliedName / StripNamingAffixes
// ============================================================================

TSharedRef<SWidget> SSpawnMaterial::CreateNamingRow(EAssetType Type, const FString& Label, const FString& DefaultPrefix)
{
    FSpawnNamingWidgets Widgets;

    TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);

    // 第一行：资产类别标题
    ContentBox->AddSlot().AutoHeight().Padding(0, 5, 0, 2)
    [
        SNew(STextBlock)
        .Text(FText::FromString(Label))
        .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
        .ColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.0f))
    ];

    // 第二行：前缀勾选 + 输入框
    ContentBox->AddSlot().AutoHeight().Padding(15, 2)
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

    // 第三行：后缀勾选 + 输入框
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

FString SSpawnMaterial::GetAppliedName(const FString& RawName, EAssetType Type)
{
    if (!NamingControlMap.Contains(Type)) return RawName;

    const FSpawnNamingWidgets& Widgets = NamingControlMap[Type];
    FString FinalName = RawName;

    if (Widgets.bUsePrefix->IsChecked())
        FinalName = Widgets.PrefixEntry->GetText().ToString() + FinalName;
    if (Widgets.bUseSuffix->IsChecked())
        FinalName = FinalName + Widgets.SuffixEntry->GetText().ToString();

    return FinalName;
}

FString SSpawnMaterial::StripNamingAffixes(const FString& Name, EAssetType Type) const
{
    if (!NamingControlMap.Contains(Type)) return Name;

    const FSpawnNamingWidgets& Widgets = NamingControlMap[Type];
    FString Result = Name;

    if (Widgets.bUseSuffix->IsChecked())
    {
        FString Suffix = Widgets.SuffixEntry->GetText().ToString();
        if (!Suffix.IsEmpty() && Result.EndsWith(Suffix, ESearchCase::IgnoreCase))
            Result = Result.LeftChop(Suffix.Len());
    }
    if (Widgets.bUsePrefix->IsChecked())
    {
        FString Prefix = Widgets.PrefixEntry->GetText().ToString();
        if (!Prefix.IsEmpty() && Result.StartsWith(Prefix, ESearchCase::IgnoreCase))
            Result = Result.RightChop(Prefix.Len());
    }

    return Result;
}


#undef LOCTEXT_NAMESPACE
