// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "Editor/Blutility/Classes/AssetActionUtility.h"
#include "Engine/EngineTypes.h"
#include "AssetAction.generated.h"

/**
 * 
 */


UCLASS()
class TOOLSBOX_API UAssetAction : public UAssetActionUtility
{
	GENERATED_BODY()


public:


	//通过初始化参数列表让构造函数执行父类构造函数
	UAssetAction(const FObjectInitializer& ObjectInitializer);


	
	UFUNCTION(BlueprintCallable,DisplayName="添加前后缀",CallInEditor,Category="通用")
	void  AddPrefixAndSuffix(FString Prefix, FString Suffix);


	UFUNCTION(BlueprintCallable,DisplayName="根据资产类型添加或替换前后缀",CallInEditor,Category="通用")
	void AutoAddPrefixAndSuffix( 
		bool bReplaceExisting = true,
		FString MeshPrefix = TEXT("SM_"), FString MeshSuffix = TEXT(""),
		FString BlueprintPrefix = TEXT("BP_"), FString BlueprintSuffix = TEXT(""),
		FString TexturePrefix = TEXT("T_"), FString TextureSuffix = TEXT(""),
		FString MaterialPrefix = TEXT("M_"), FString MaterialSuffix = TEXT(""),
		FString MaterialInstancePrefix = TEXT("MI_"), FString MaterialInstanceSuffix = TEXT(""),
		FString MaterialFunctionPrefix = TEXT("MF_"), FString MaterialFunctionSuffix = TEXT(""),
		FString WorldPrefix = TEXT("L_"), FString WorldSuffix = TEXT(""),
		FString SkeletalPrefix = TEXT("SK_"), FString SkeletalSuffix = TEXT(""),
		FString TextureRenderPrefix = TEXT("RT_"), FString TextureRenderSuffix = TEXT("")
	);
	
	//文本替换
	UFUNCTION(BlueprintCallable,DisplayName="文本替换",CallInEditor,Category="通用")
	void  ReplaceText_(FString OldText, FString NewText);


	//批量添加标签
	UFUNCTION(BlueprintCallable,DisplayName="批量添加标签",CallInEditor,Category="Actor")
	void  AddTags(TArray<FName> TagName);


	//重设纹理大小
	UFUNCTION(BlueprintCallable,DisplayName="重设纹理大小",CallInEditor,Category="纹理")
	void  ResizeTexture(FIntPoint NewSize);

	//设置凸包分解碰撞
	UFUNCTION(BlueprintCallable,DisplayName="设置凸包分解碰撞",CallInEditor,Category="静态网格体")
	void  SetConvexDecompositionCollision(int32 HullCount, int32 MaxHullVerts, int32 HullPrecision);

	//清除未使用节点
	UFUNCTION(BlueprintCallable,DisplayName="清除未使用节点",CallInEditor,Category="蓝图")
	void  ClearUnusedNodes();

	//移除未使用变量
	UFUNCTION(BlueprintCallable,DisplayName="移除未使用变量",CallInEditor,Category="蓝图")
	void  RemoveUnusedVariables();

	//设置网格体物理材质
	UFUNCTION(BlueprintCallable,DisplayName="设置网格体物理材质",CallInEditor,Category="静态网格体")
	void  SetMeshPhysicsMaterial(UPhysicalMaterial* PhysicsMaterial);

	//Nanite设置
	UFUNCTION(BlueprintCallable,DisplayName="Nanite设置",CallInEditor,Category="静态网格体")
	void  SetNaniteSetting(
		const bool& bEnableNanite,
		const bool& ExplicitTangents,
		const bool& LerpUVs,
		const float& KeepPercentTriangles,
		const ENaniteFallbackTarget &FallbackTarget,
		const bool& bApplyChanges);

	//移除前(后)n个字符
	UFUNCTION(BlueprintCallable,DisplayName="移除前(后)n个字符（默认移除前1个字符）",CallInEditor,Category="通用")
	void  RemoveChar(int32 CharCount=1, bool Interval=false);


	//简化模型
	UFUNCTION(BlueprintCallable,DisplayName="简化模型",CallInEditor,Category="静态网格体")
	void  SimplifyMesh(float Percent);



	
private:
	/** 智能重命名核心逻辑 */
	void SmartRenameAsset(UObject* Asset, const FString& NewPrefix, const FString& NewSuffix, bool bReplaceExisting);
	
};
