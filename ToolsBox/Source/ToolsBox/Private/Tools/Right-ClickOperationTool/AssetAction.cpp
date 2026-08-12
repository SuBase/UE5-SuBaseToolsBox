// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/Right-ClickOperationTool/AssetAction.h"

#include <functional>

#include "AnimationModifiersAssetUserData.h"
#include "AssetToolsModule.h"

#include "Editor.h"
#include "Editor/Blutility/Classes/AssetActionUtility.h"
#include "Engine/EngineTypes.h"
#include "K2Node_EditablePinBase.h"
#include "Editor/BlueprintEditorLibrary/Public/BlueprintEditorLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicsEngine/BodySetup.h"
#include "Slate_Assist/SlateAssistBuildFunctionLibrary.h"
#include "EditorAssetLibrary.h"

#include "K2Node_ConstructObjectFromClass.h"
#include "Editor/Blutility/Public/EditorUtilityLibrary.h"
#include "Editor/StaticMeshEditor/Public/StaticMeshEditorSubsystem.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "EditorScriptingUtilities/Public/EditorDialogLibrary.h"
#include "Modules/ModuleManager.h"

#include "UDynamicMesh.h"                           
#include "Animation/AnimSequence.h"

#include "Kismet/KismetSystemLibrary.h"


UAssetAction::UAssetAction(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// 你的初始化逻辑
	SupportedClasses.Add(UObject::StaticClass());
}

void UAssetAction::AddPrefixAndSuffix(FString Prefix, FString Suffix)
{
	// 1. 获取选中的资产
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
 
	for (UObject* Asset : SelectedAssets)
	{
		if (!Asset) continue;
 
		// 1. 获取原始信息
		FString OldName = Asset->GetName();
		FString NewName = Prefix + OldName + Suffix;
        
		// 如果名字没变，直接跳过
		if (OldName.Equals(NewName)) continue;
 
		// 2. 获取路径
		// GetPathName() 返回类似 /Game/Items/MyAsset.MyAsset
		FString FullPath = Asset->GetPathName();
		// 获取包路径，如 /Game/Items/
		FString PackagePath = FPackageName::GetLongPackagePath(FullPath);
		// 构建完整的新路径 /Game/Items/NewName
		FString NewObjectPath = FPaths::Combine(PackagePath, NewName);
 
		// 3. 使用 EditorAssetLibrary 进行重命名
		// 该方法会自动处理底层的包创建、重定向器逻辑
		if (UEditorAssetLibrary::RenameAsset(FullPath, NewObjectPath))
		{
			SlateAssistBuildFunctionLibrary::SpawnNotifiy(INVTEXT("成功重命名"),0.3f,1.0f,SNotificationItem::ECompletionState::CS_Success);
		}
		else
		{
			SlateAssistBuildFunctionLibrary::SpawnNotifiy(INVTEXT("重命名失败 请检查目标路径是否已存在同名资产或重定向器"),0.3f,1.0f,SNotificationItem::ECompletionState::CS_Fail);
		}
	}
}

void UAssetAction::AutoAddPrefixAndSuffix(bool bReplaceExisting, FString MeshPrefix, FString MeshSuffix,
	FString BlueprintPrefix, FString BlueprintSuffix, FString TexturePrefix, FString TextureSuffix,
	FString MaterialPrefix, FString MaterialSuffix, FString MaterialInstancePrefix, FString MaterialInstanceSuffix,
	FString MaterialFunctionPrefix, FString MaterialFunctionSuffix, FString WorldPrefix, FString WorldSuffix,
	FString SkeletalPrefix, FString SkeletalSuffix, FString TextureRenderPrefix, FString TextureRenderSuffix)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
 
	for (UObject* Asset : SelectedAssets)
	{
		if (!Asset) continue;
 
		FString NewPrefix = "";
		FString NewSuffix = "";
 
		// 1. 基础类型判断
		if (Asset->IsA<UStaticMesh>()) { NewPrefix = MeshPrefix; NewSuffix = MeshSuffix; }
		else if (Asset->IsA<UBlueprint>()) { NewPrefix = BlueprintPrefix; NewSuffix = BlueprintSuffix; }
		else if (Asset->IsA<UTexture2D>()) { NewPrefix = TexturePrefix; NewSuffix = TextureSuffix; }
		else if (Asset->IsA<UMaterialFunction>()) { NewPrefix = MaterialFunctionPrefix; NewSuffix = MaterialFunctionSuffix; }
		else if (Asset->IsA<UWorld>()) { NewPrefix = WorldPrefix; NewSuffix = WorldSuffix; }
		else if (Asset->IsA<USkeletalMesh>()) { NewPrefix = SkeletalPrefix; NewSuffix = SkeletalSuffix; }
		else if (Asset->IsA<UTextureRenderTarget2D>()) { NewPrefix = TextureRenderPrefix; NewSuffix = TextureRenderSuffix; }
		
		// 2. 材质与材质实例的特殊处理
		// 注意：MaterialInstance 也是 MaterialInterface，所以先判断具体的 Instance
		else if (Asset->IsA<UMaterialInstance>()) { NewPrefix = MaterialInstancePrefix; NewSuffix = MaterialInstanceSuffix; }
		else if (Asset->IsA<UMaterial>()) { NewPrefix = MaterialPrefix; NewSuffix = MaterialSuffix; }
 
		if (NewPrefix.IsEmpty() && NewSuffix.IsEmpty()) continue;
 
		SmartRenameAsset(Asset, NewPrefix, NewSuffix, bReplaceExisting);
	}	
}



void UAssetAction::ReplaceText_(FString OldText, FString NewText)
{
	if (OldText.IsEmpty())
	{
		SlateAssistBuildFunctionLibrary::SpawnNotifiy(INVTEXT("请输入要查找的文本"),0.3f,1.0f,SNotificationItem::ECompletionState::CS_Fail);
		return;
	}

	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	
}

void UAssetAction::AddTags(TArray<FName> TagName)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	
	for (UObject* Asset : SelectedAssets)
	{
		UBlueprint* BP = Cast<UBlueprint>(Asset);
		if (BP && BP->GeneratedClass)
		{
			// 获取蓝图生成的默认对象
			AActor* DefaultActor = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
			if (DefaultActor)
			{
				for (FName Tag : TagName)
				{
					DefaultActor->Tags.Add(Tag);
                    				
				}
				
			}
		}
	}
}

void UAssetAction::ResizeTexture(FIntPoint NewSize)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
		{
			Texture->Modify();
			Texture->MaxTextureSize = FMath::Max(NewSize.X, NewSize.Y);
			Texture->PostEditChange();
		}
	}
}

void UAssetAction::SetConvexDecompositionCollision(int32 HullCount, int32 MaxHullVerts, int32 HullPrecision)
{
	
	
	if (UStaticMeshEditorSubsystem *SMESubsystem=GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
	{
		TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
		for (UObject* Asset : SelectedAssets)
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
			{
				
				SMESubsystem->SetConvexDecompositionCollisions(Mesh, HullCount, MaxHullVerts, HullPrecision);
				Mesh->PostEditChange();
				
			}
		}
	}
}

void UAssetAction::ClearUnusedNodes()
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			TArray<UEdGraph*> AllGraphs;
			BP->GetAllGraphs(AllGraphs);
			for (UEdGraph* Graph : AllGraphs)
			{
				TArray<UEdGraphNode*> NodesToRemove;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					bool bIsConnected = false;
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->LinkedTo.Num() > 0)
						{
							bIsConnected = true;
							break;
						}
					}
					// 排除事件节点和入口节点，仅移除无连线的功能节点
					if (!bIsConnected && !Node->IsA<UK2Node_EditablePinBase>())
					{
						NodesToRemove.Add(Node);
					}
				}
				for (UEdGraphNode* Node : NodesToRemove)
				{
					FBlueprintEditorUtils::RemoveNode(BP,Node);
				}
			}
		}
	}
}

void UAssetAction::RemoveUnusedVariables()
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			UBlueprintEditorLibrary::RemoveUnusedVariables(BP);
		}
	}
}

void UAssetAction::SetMeshPhysicsMaterial(UPhysicalMaterial* PhysicsMaterial)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			if (UBodySetup* BodySetup = Mesh->GetBodySetup())
			{
				BodySetup->Modify();
				BodySetup->PhysMaterial = PhysicsMaterial;
				Mesh->PostEditChange();
			}
		}
	}
}

void UAssetAction::SetNaniteSetting(const bool& bEnableNanite,const bool& ExplicitTangents,
	const bool& LerpUVs, const float& KeepPercentTriangles, const ENaniteFallbackTarget& FallbackTarget,
	const bool& bApplyChanges)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			Mesh->Modify();
			FMeshNaniteSettings& Settings = Mesh->NaniteSettings;
			Settings.bEnabled = bEnableNanite;
			Settings.bExplicitTangents = ExplicitTangents;
			Settings.bLerpUVs = LerpUVs;
			Settings.FallbackTarget = FallbackTarget;
			Settings.KeepPercentTriangles = KeepPercentTriangles;
			// 注意：FallbackPercentTriangles 在新版本中替代了部分旧参数
            
			if (bApplyChanges)
			{
				Mesh->PostEditChange();
			}
		}
	}
}

void UAssetAction::RemoveChar(int32 CharCount, bool Interval)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
 
	TArray<FAssetRenameData> RenameDataList;
 
	for (UObject* Asset : SelectedAssets)
	{
		FString OldName = Asset->GetName();
		if (OldName.Len() <= CharCount) continue;
 
		FString NewName = Interval ? OldName.Left(OldName.Len() - CharCount) : OldName.Right(OldName.Len() - CharCount);
		FString ObjectPath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
 
		RenameDataList.Add(FAssetRenameData(Asset, ObjectPath, NewName));
	}
 
	if (RenameDataList.Num() > 0)
	{
		AssetToolsModule.Get().RenameAssets(RenameDataList);
	}
}

void UAssetAction::SimplifyMesh(float Percent)
{
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	for (UObject* Asset : SelectedAssets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			// 创建临时 DynamicMesh
			TObjectPtr<UDynamicMesh> DMesh = NewObject<UDynamicMesh>();
			EGeometryScriptOutcomePins Result;
			
			// 从 StaticMesh 拷贝数据
			TObjectPtr<UDynamicMesh> NewDMesh = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				Mesh, 
				DMesh, 
				FGeometryScriptCopyMeshFromAssetOptions(true, true, true, true), 
				FGeometryScriptMeshReadLOD(), 
				Result);
 
			if (NewDMesh && Result == EGeometryScriptOutcomePins::Success)
			{
				int32 OriginalTriangleCount = NewDMesh->GetTriangleCount();
				int32 NewDMeshTriangleCount = FMath::TruncToInt(OriginalTriangleCount * Percent);
 
				FString Message = FString::Format(
					TEXT("当前网格体：{0}\n模型简化前三角面数量：{1}\n模型简化后三角面数量：{2}\n注意：简化后请再三确认后再保存！"),
					{
						Mesh->GetName(),
						FString::FromInt(OriginalTriangleCount),
						FString::FromInt(NewDMeshTriangleCount)
					}
				);
 
				EAppReturnType::Type TypeReturn = UEditorDialogLibrary::ShowMessage(
					FText::FromString(TEXT("提示")),
					FText::FromString(Message), 
					EAppMsgType::YesNo
				);
 
				switch (TypeReturn)
				{
				case EAppReturnType::Yes:
					{ // <--- 修复 C2360 必须添加大括号以限制作用域
						// 执行简化操作
						UDynamicMesh* SimplifiedMesh = UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
							NewDMesh,
							NewDMeshTriangleCount,
							FGeometryScriptSimplifyMeshOptions(EGeometryScriptRemoveMeshSimplificationType::StandardQEM));
 
						
						UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
							SimplifiedMesh,
							Mesh,
							FGeometryScriptCopyMeshToAssetOptions(),
							FGeometryScriptMeshWriteLOD(), 
							Result);
 

						if(Mesh->IsNaniteEnabled())
						{
							FMeshNaniteSettings CurrentSettings = Mesh->NaniteSettings;

							GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>()->SetNaniteSettings(Mesh,CurrentSettings, true);
							
							Mesh->PostEditChange();
						}
						
					}
					break;
 
				case EAppReturnType::No:
					
					continue; 
				}
			}
		}
	}
}



void UAssetAction::SmartRenameAsset(UObject* Asset, const FString& NewPrefix, const FString& NewSuffix,
                                    bool bReplaceExisting)
{
	if (!Asset) return;
 
	FString OldName = Asset->GetName();
	FString BaseName = OldName;
    
	// ... 原有的 BaseName 提取逻辑保持不变 ...
 
	FString FinalName = bReplaceExisting ? (NewPrefix + BaseName + NewSuffix) : OldName;
	if (!bReplaceExisting)
	{
		if (!NewPrefix.IsEmpty() && !OldName.StartsWith(NewPrefix)) FinalName = NewPrefix + FinalName;
		if (!NewSuffix.IsEmpty() && !OldName.EndsWith(NewSuffix)) FinalName = FinalName + NewSuffix;
	}
 
	if (FinalName == OldName) return;
 
	// 路径处理
	FString CurrentPath = Asset->GetPathName();
	FString PackagePath = FPackageName::GetLongPackagePath(CurrentPath);
	FString NewPath = PackagePath + TEXT("/") + FinalName;
 
	// 针对 UWorld 的特殊安全性检查
	if (Asset->IsA<UWorld>())
	{
		// 如果该关卡在当前编辑器窗口中是打开的，重命名可能会失败
		// 我们尝试通过检测世界类型来预判风险
		UWorld* World = Cast<UWorld>(Asset);
		if (World && World->IsGameWorld()) // 正在运行或模拟
		{
			SlateAssistBuildFunctionLibrary::SpawnNotifiy(INVTEXT("有关卡正在运行，无法重命名"),5.0f,3.0f,SNotificationItem::ECompletionState::CS_Fail);

			return;
		}
	}
 
	// 执行重命名操作
	if (!UEditorAssetLibrary::RenameAsset(CurrentPath, NewPath))
	{
		// 失败日志，通常是因为资源被锁定或正在被编辑器使用
			SlateAssistBuildFunctionLibrary::SpawnNotifiy(INVTEXT("有文件被锁定（可能被打开），无法重命名"),0.3f,1.0f,SNotificationItem::ECompletionState::CS_Fail);
	}
}
