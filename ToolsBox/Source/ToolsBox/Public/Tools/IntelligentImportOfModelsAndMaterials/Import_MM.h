#pragma once

#include "CoreMinimal.h"
#include "PropertyCustomizationHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"

// 前向声明，优化编译速度
class SEditableTextBox;
class SMultiLineEditableText;
class SScrollBox;
class FRichTextLayoutMarshaller;


/** 定义资产命名类型枚举 */
enum class EImportAssetType : uint8 { Mesh, Texture, Material, Instance };
 
/** 命名规则 UI 组结构体 */
struct FNamingWidgets {
	TSharedPtr<SCheckBox> bUsePrefix;
	TSharedPtr<SEditableTextBox> PrefixEntry;
	TSharedPtr<SCheckBox> bUseSuffix;
	TSharedPtr<SEditableTextBox> SuffixEntry;
};

struct FImportFolderTask
{
	FString FolderName;
	FString MeshPath;
	TMap<FString, FString> TextureMap;
};

class SImport_MM : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImport_MM) {}
	SLATE_END_ARGS()

	/** 构建 Slate 界面 */
	void Construct(const FArguments& InArgs);

private:
	/** UI 事件回调 */
	FReply OnBrowseSourceClicked();
	FReply OnBrowseDestClicked();
	FReply OnStartImportClicked();
	FReply OnClearLog();
    
	/** 
	 * 向日志框添加信息
	 * @param Message 日志内容
	 * @param Color   颜色（支持富文本渲染）
	 */
	void AddLog(const FString& Message, FLinearColor Color);
    
	/** 执行具体的 FBX/OBJ 导入逻辑 */
	void ExecuteImportTask(const FImportFolderTask& Task, const FString& BaseDestPath);

	// 1. 执行模型导入
	void PerformMeshImport(const FImportFolderTask& Task, const FString& FinalPath);
 
	// 2. 执行贴图导入
	void PerformTextureImport(const FImportFolderTask& Task, const FString& FinalPath);
 
	// 3. 收集并过滤导入后的 StaticMesh
	TArray<UStaticMesh*> CollectImportedMeshes(const FString& FinalPath, const FString& MeshBaseName);
 
	// 4. 创建并连接材质节点
	void GenerateMaterials(const FImportFolderTask& Task, const FString& FinalPath,
	                       TMap<FString, UMaterialInterface*>& OutCreatedMaterials, UMaterialInterface*& OutSingleFallbackMat, int32&
	                       OutBaseColorCount);

	// 4a. 无ORM贴图时生成材质（AO、粗糙度、金属度为独立贴图）
	void GenerateMaterialWithoutORM(
		const FString& BCRawName,
		const TMap<FString, UTexture2D*>& LocalMatch,
		bool bHasOpacity,
		const FString& FinalPath,
		bool bUseExistingParent,
		bool bAutoCreateInstance,
		UMaterialInterface* ParentMI,
		UMaterialInterface*& OutWorkingMat
	);

	// 4b. 有ORM贴图时生成材质（AO、粗糙度、金属度合并在一张贴图内）
	void GenerateMaterialWithORM(
		const FString& BCRawName,
		const TMap<FString, UTexture2D*>& LocalMatch,
		bool bHasOpacity,
		const FString& FinalPath,
		bool bUseExistingParent,
		bool bAutoCreateInstance,
		UMaterialInterface* ParentMI,
		UMaterialInterface*& OutWorkingMat
	);

	// 5. 将材质分配给模型
	void ApplyMaterialsToMeshes(
		const TArray<UStaticMesh*>& Meshes, 
		const TMap<FString, UMaterialInterface*>& CreatedMaterials, // 改为 Interface
		int32 BaseColorCount, 
		UMaterialInterface* SingleFallbackMat,                       // 改为 Interface
		const FString& MeshBaseName, 
		const FString& FinalPath
	);

	// 5a. 拆分模型重命名（去掉合并模型名前缀）
	void RenameSplitMeshes(TArray<UStaticMesh*>& Meshes, const FString& MeshBaseName, const FString& FinalPath);

	// 5b. 收集指定路径下的所有 StaticMesh（重命名后使用）
	TArray<UStaticMesh*> CollectAllStaticMeshesInPath(const FString& FinalPath);

	// 5c. 检查指定路径下是否已存在同名资产（含内存中未保存的对象）
	bool DoesAssetExistInPath(const FString& AssetName, const FString& PackagePath) const;
	// 5d. 若目标名称已被占用则追加 _1,_2... 生成不冲突的唯一资产名
	FString MakeUniqueAssetName(const FString& DesiredName, const FString& PackagePath) const;

	/** 路径数据 */
	FString SourceFolderPath;
	FString RelativeDestPath;

	/** UI 组件指针 */
	TSharedPtr<SEditableTextBox> SourcePathBox;
	TSharedPtr<SEditableTextBox> DestPathBox;
	TSharedPtr<SEditableTextBox> TexSubFolderNameBox; // 已修正为单行文本框指针
	TSharedPtr<SCheckBox> bCreateMICheckbox;
	TSharedPtr<SCheckBox> bUseCombinedNamePrefixCheckbox; // 拆分模型是否添加合并模型名前缀
	TSharedPtr<SMultiLineEditableText> LogBox;
	TSharedPtr<SScrollBox> LogScrollBox;

	/** 
	 * 富文本处理器：用于在 SMultiLineEditableText 中显示颜色标签 
	 * 命名已修改为 MarshallerPtr 以避免隐藏局部变量 (C4458)
	 */
	TSharedPtr<FRichTextLayoutMarshaller> RichTextMarshallerPtr;

	FReply OnCreateGenericMaterialClicked();
    
	// 辅助函数：向材质添加参数节点
	UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(UMaterial* InMaterial, FName InParamName, int32 InYPos, EMaterialSamplerType InSamplerType);


	// 新增 UI 指针
	TSharedPtr<SCheckBox> bUseParentMICheckbox;
	TSharedPtr<SObjectPropertyEntryBox> ParentMISelector;
    
	// 用于存储不同通道对应的参数名输入框
	// Key 为通道缩写（BC, N, ORM, OP, EM 等），Value 为对应的输入框
	TMap<FString, TSharedPtr<SEditableTextBox>> ParamNameInputs;
 
	// 存储用户选择的父类 MI 路径
	FSoftObjectPath SelectedParentMIPath;


	TMap<EImportAssetType, FNamingWidgets> NamingControlMap;
	
	// 辅助函数：创建参数名输入行
	TSharedRef<SWidget> CreateParamInputRow(const FString& ChannelLabel, const FString& DefaultParamName, const FString& Key);
    
	// 获取当前勾选状态
	ECheckBoxState IsUseParentMIChecked() const { return bUseParentMICheckbox->GetCheckedState(); }
	void OnUseParentMIToggled(ECheckBoxState NewState);


	/** 辅助函数 */
	TSharedRef<SWidget> CreateNamingRow(EImportAssetType Type, const FString& Label, const FString& DefaultPrefix);
	FString GetAppliedName(const FString& RawName, EImportAssetType Type);

};
