// Copyright 2026 SuBase. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PropertyCustomizationHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"

// 前向声明
class SEditableTextBox;
class SMultiLineEditableText;
class SScrollBox;
class SObjectPropertyEntryBox;

/** 资产命名类型枚举 */
enum class EAssetType : uint8 { Mesh, Texture, Material, Instance };

/** 命名规则 UI 组结构体 */
struct FSpawnNamingWidgets {
    TSharedPtr<SCheckBox> bUsePrefix;
    TSharedPtr<SEditableTextBox> PrefixEntry;
    TSharedPtr<SCheckBox> bUseSuffix;
    TSharedPtr<SEditableTextBox> SuffixEntry;
};

/**
 * 批量材质球生成工具
 * 从内容浏览器获取选中的纹理贴图，按文件名关键词自动分类贴图通道，
 * 然后生成材质球（母材质连线模式 或 父材质实例化模式）。
 */
class SSpawnMaterial : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSpawnMaterial) {}
    SLATE_END_ARGS()

    /** 构建 Slate 界面 */
    void Construct(const FArguments& InArgs);

private:
    // ==================== UI 事件回调 ====================

    /** 选择保存路径 */
    FReply OnBrowseDestClicked();

    /** 生成材质按钮回调 — 核心入口 */
    FReply OnGenerateClicked();

    /** 清空日志 */
    FReply OnClearLog();

    /** 生成通用父材质及实例（供 MI 模式使用） */
    FReply OnCreateGenericMaterialClicked();

    // ==================== 日志 ====================

    /**
     * 向日志框添加信息（纯文本格式，不含富文本标签）
     * @param Message 日志内容
     * @param Color   颜色（仅记录分类，实际渲染为纯文本）
     */
    void AddLog(const FString& Message, FLinearColor Color);

    // ==================== 核心逻辑 ====================

    /** 从内容浏览器获取选中的 UTexture2D 资产 */
    TArray<UTexture2D*> GetSelectedTextures();

    /** 从内容浏览器获取选中的 UStaticMesh 资产 */
    TArray<UStaticMesh*> GetSelectedMeshes();

    /**
     * 将生成的材质按名称关键词匹配赋予给选中的模型
     * @param CreatedMaterials 材质基础名 -> 生成的材质接口
     * @param Meshes 选中的模型数组
     */
    void AssignMaterialsToMeshes(const TMap<FString, UMaterialInterface*>& CreatedMaterials, const TArray<UStaticMesh*>& Meshes);

    /**
     * 将贴图按关键词分类并按材质基础名分组
     * @param Textures 选中的贴图数组
     * @param OutBaseColorRawNames 各组的 BC 原始名（用于推导材质名）
     * @param OutGroups 分组结果：材质基础名 -> (通道键 -> 贴图)
     * @param OutHasOpacityMap 各组是否包含透明度贴图
     * @param OutBaseColorCount BC 贴图总数
     */
    void CategorizeAndGroupTextures(
        const TArray<UTexture2D*>& Textures,
        TArray<FString>& OutBaseColorRawNames,
        TMap<FString, TMap<FString, UTexture2D*>>& OutGroups,
        TMap<FString, bool>& OutHasOpacityMap,
        int32& OutBaseColorCount);

    /**
     * 为单个贴图组生成材质（根据是否有 ORM 贴图自动选择路径）
     * @return 生成的材质接口（可能为母材质或材质实例）
     */
    UMaterialInterface* GenerateMaterialForGroup(
        const FString& MatNameBase,
        const TMap<FString, UTexture2D*>& LocalMatch,
        bool bHasOpacity,
        const FString& SavePath,
        bool bUseExistingParent,
        bool bAutoCreateInstance,
        UMaterialInterface* ParentMI);

    /** 无 ORM 贴图时生成材质（AO、粗糙度、金属度为独立贴图） */
    UMaterialInterface* GenerateMaterialWithoutORM(
        const FString& BCRawName,
        const TMap<FString, UTexture2D*>& LocalMatch,
        bool bHasOpacity,
        const FString& SavePath,
        bool bUseExistingParent,
        bool bAutoCreateInstance,
        UMaterialInterface* ParentMI);

    /** 有 ORM 贴图时生成材质（AO、粗糙度、金属度合并在一张贴图内） */
    UMaterialInterface* GenerateMaterialWithORM(
        const FString& BCRawName,
        const TMap<FString, UTexture2D*>& LocalMatch,
        bool bHasOpacity,
        const FString& SavePath,
        bool bUseExistingParent,
        bool bAutoCreateInstance,
        UMaterialInterface* ParentMI);

    // ==================== 资产名辅助 ====================

    /** 检查指定路径下是否已存在同名资产 */
    bool DoesAssetExistInPath(const FString& AssetName, const FString& PackagePath) const;

    /** 若目标名称已被占用则追加 _1,_2... 生成不冲突的唯一资产名 */
    FString MakeUniqueAssetName(const FString& DesiredName, const FString& PackagePath) const;

    // ==================== UI 辅助 ====================

    /** 创建贴图参数名输入行 */
    TSharedRef<SWidget> CreateParamInputRow(const FString& ChannelLabel, const FString& DefaultParamName, const FString& Key);

    /** 父材质实例勾选状态变更回调 */
    void OnUseParentMIToggled(ECheckBoxState NewState);

    /** 创建命名规则配置行（前缀/后缀勾选+输入框） */
    TSharedRef<SWidget> CreateNamingRow(EAssetType Type, const FString& Label, const FString& DefaultPrefix);

    /** 根据命名规则为原始名应用前缀和后缀 */
    FString GetAppliedName(const FString& RawName, EAssetType Type);

    /** 从名称中剥离指定类型的前缀和后缀（反向操作，用于匹配时还原原名） */
    FString StripNamingAffixes(const FString& Name, EAssetType Type) const;

    // ==================== 数据成员 ====================

    /** 保存路径（包路径，如 /Game/Materials） */
    FString RelativeDestPath;

    /** UI 组件指针 */
    TSharedPtr<SEditableTextBox> DestPathBox;
    TSharedPtr<SMultiLineEditableText> LogBox;
    TSharedPtr<SScrollBox> LogScrollBox;

    /** 模式选择 */
    TSharedPtr<SCheckBox> bCreateMICheckbox;        // 创建实例并应用
    TSharedPtr<SCheckBox> bUseParentMICheckbox;     // 使用父材质实例
    TSharedPtr<SCheckBox> bSaveNextToTexturesCheckbox; // 保存到贴图同级目录
    TSharedPtr<SObjectPropertyEntryBox> ParentMISelector;

    /** 贴图参数名输入框映射表（通道键 -> 输入框） */
    TMap<FString, TSharedPtr<SEditableTextBox>> ParamNameInputs;

    /** 用户选择的父材质实例路径 */
    FSoftObjectPath SelectedParentMIPath;

    /** 命名规则控件映射表 */
    TMap<EAssetType, FSpawnNamingWidgets> NamingControlMap;
};
