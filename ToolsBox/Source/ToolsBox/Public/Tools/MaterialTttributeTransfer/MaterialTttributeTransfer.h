// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/VerticalBox.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SComboBox.h"

struct FParamMappingPair
{
	FString TargetParamName;
	FString SourceParamName;
};

/** 一套材质属性转移配置（多套追加保存在同一个 JSON 里，用下拉框切换，仿自动前缀） */
struct FMaterialTransferConfigItem
{
	FString ConfigName;                                       // 这套配置的名字，也是 JSON 里的唯一标识
	FString MasterMaterialPath;                               // 目标母材质路径
	FString TargetSavePath;                                   // 生成保存路径
	bool bSaveToRespectiveFolders = true;                     // 保存到各自文件夹
	bool bForceGenerateMaterial = false;                     // 统一生成材质类
	bool bForceGenerateInstance = false;                     // 统一生成材质实例
	TArray<TSharedPtr<FParamMappingPair>> Mappings;          // 参数映射表
	bool bIsSaved = false;                                    // 是否已写入过 JSON
};
 
class SMaterialTttributeTransfer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialTttributeTransfer) {}
	SLATE_END_ARGS()
 
	void Construct(const FArguments& InArgs);
 
private:
	TSharedPtr<SVerticalBox> MappingContainer;
	TArray<TSharedPtr<FParamMappingPair>> MappingList;
	TWeakObjectPtr<UMaterialInterface> TargetMasterMaterial;

	// 配置多套（仿自动前缀：追加保存到同一个 JSON，下拉框切换）
	TArray<TSharedPtr<FMaterialTransferConfigItem>> Configs;
	TSharedPtr<FMaterialTransferConfigItem> CurrentConfig;
	FString EditingConfigName;
	TSharedPtr<SComboBox<TSharedPtr<FMaterialTransferConfigItem>>> ConfigComboBox;

	FString TargetSavePath;

	// 勾选后：生成的新材质实例保存在源材质各自所在的文件夹中；默认勾选
	bool bSaveToRespectiveFolders = true;

	// 输出类型控制：两个不能同时打钩，默认都不勾选（按源材质类型生成）
	bool bForceGenerateMaterial = false;  // 统一生成材质类
	bool bForceGenerateInstance = false; // 统一生成材质实例
 
	// 日志组件修复：添加 ScrollBox 引用
	TSharedPtr<SScrollBox> LogScrollBox;
	TSharedPtr<SMultiLineEditableText> LogWindow;
	FText LogContent;
	void AppendLog(const FString& InLog);
 
	void AddMappingRow();
	void RemoveMappingRow(TSharedPtr<FParamMappingPair> InPair);  // 删除单行映射
	void ClearAllMappings();                                      // 清空所有映射行
	TSharedRef<SWidget> CreateMappingRowWidget(TSharedPtr<FParamMappingPair> InPair);
	void RefreshMappingUI();
    
	FString GetSaveDirectory() const;
	FString GetConfigJsonPath() const;

	// 配置存档（仿自动前缀：多套追加保存到同一个 JSON）
	void LoadAllConfigsFromJson();
	bool WriteAllConfigsToJson();
	void SaveCurrentConfigToJson();
	void DeleteConfig(TSharedPtr<FMaterialTransferConfigItem> Target);
	TArray<TSharedPtr<FParamMappingPair>> DeepCopyMappings(const TArray<TSharedPtr<FParamMappingPair>>& InMappings) const;
	void LoadConfigDataFromCurrent();                 // 把当前配置的数据填进面板成员
	void WriteConfigDataToCurrent();                  // 把面板成员写回当前配置
	void SeedDefaultConfig();                          // 第一次用，先给一套默认

	// 配置下拉框
	TSharedRef<SWidget> OnGenerateConfigComboWidget(TSharedPtr<FMaterialTransferConfigItem> InItem);
	void OnConfigSelectionChanged(TSharedPtr<FMaterialTransferConfigItem> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetCurrentConfigNameText() const;
	void SwitchToConfig(TSharedPtr<FMaterialTransferConfigItem> Target);

	// 追溯母材质链，找到最底层的基础材质类（UMaterial），用于"生成材质类"时复制其材质图
	UMaterial* FindBaseMaterialTemplate(UMaterialInterface* InMat) const;
	// 在同一包路径下生成不重名的资产名，避免命名冲突导致创建失败
	void MakeUniqueAssetName(const FString& PackagePath, const FString& BaseName, FString& OutName) const;
	// 将源材质（按映射表）的参数值写入目标材质（实例或材质类均可）
	void ApplyParameterValues(UMaterialInterface* Target, UMaterialInterface* Source) const;
 
	FReply OnExecuteTransfer();
	void UpdateCurrentPathFromContentBrowser(); 
 
	void OnMasterMaterialChanged(const FAssetData& AssetData);
	FString GetMasterMaterialPath() const;
};