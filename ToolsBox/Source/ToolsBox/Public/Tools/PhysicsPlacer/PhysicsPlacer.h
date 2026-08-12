// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "Containers/Ticker.h"
#include "AssetThumbnail.h"                       // FAssetThumbnail / FAssetThumbnailPool（生成网格缩略图，已在 UnrealEd 依赖内）
#include "AssetRegistry/AssetData.h"             // FAssetData（拖拽网格入列表时用到）

class AActor;
class UPrimitiveComponent;
class UStaticMesh;
class FAssetThumbnailPool;
class SMultiLineEditableText;
class SVerticalBox;
class SWrapBox;
class FLevelEditorViewportClient;
class FPhysicsPlacerEdMode;

/**
 * 列表里的一个已选物体（工具内部维护，避免直接依赖编辑器选中状态）。
 */
struct FSelectedActorItem : public TSharedFromThis<FSelectedActorItem>
{
	TWeakObjectPtr<AActor> Actor;       // 指向场景里的真实 Actor（可能被删，用弱引用）
	FString DisplayLabel;               // 列表里展示的文字：标签（类名）
};

/**
 * 一份"摆位"存档：记录若干物体当时的 位置 / 旋转 / 缩放，方便物理模拟不理想时回退。
 */
struct FSavedPose : public TSharedFromThis<FSavedPose>
{
	struct FActorTransform
	{
		FString Label;     // 物体标签（Actor Label），回溯时按标签在当前关卡里匹配
		FVector Location;
		FQuat   Rotation;
		FVector Scale;
	};

	FString Name;                       // 这套摆位的名字（用户自定义，也可同名覆盖）
	TArray<FActorTransform> Actors;    // 这套摆位里记录的每个物体
};

/**
 * 生成模式下可生成的一个网格条目（像刷草模式里的一个植被类型）。
 *  - Mesh：要生成的网格，为 nullptr 时用默认立方体；
 *  - bEnabled：左上角勾选框，控制该条目是否参与生成（多选时可通过它开关单个网格）。
 */
struct FSpawnMeshEntry : public TSharedFromThis<FSpawnMeshEntry>
{
	UStaticMesh* Mesh = nullptr;       // 要生成的网格（nullptr=默认立方体）
	bool bEnabled = true;              // 左上角勾选框：是否参与生成

	FString GetName() const;           // 列表里展示的文字：网格名 或 "默认立方体"
};

/**
 * 物理摆放工具。
 *
 * 思路（参考 UE 商城 PhysicalLayout 插件的做法）：
 *   1. 在视口里选中若干场景物体，点"获取编辑器选中"把它们拉进工具列表；
 *   2. 点"启动模拟"：把列表里的物体设为可移动并开启物理（重力+碰撞），
 *      然后每帧直接用 Chaos 求解器推进动力学——
 *        World->GetPhysicsScene()->GetPhysicsSolvers() 拿到求解器，
 *        逐个调用 AdvanceAndDispatch_External(DeltaTime)。
 *      这一步绕开了编辑器世界默认的"物理不模拟"(bShouldSimulatePhysics=false)门控，
 *      与 PhysicalLayout 在 EdMode::Tick 里直接步进求解器是同一思路。
 *      同时把各视口"强制实时"打开，让 FChaosScene::Tick 把求解结果同步回组件、并刷新视口。
 *   3. 点"停止模拟"：关掉物理，物体就停在被物理带到的位置；
 *      停掉时移除"强制实时"覆盖、恢复各视口原来的实时状态，并注销每帧步进。
 *   4. "保存当前位置"把列表里物体的 位置/旋转/缩放 记成一份带名字的摆位（JSON 追加）；
 *      "位置回溯"把某份摆位重新套回当前关卡里标签匹配的物体上。
 *
 * 存档结构与"自动前缀"工具一致：都放在 ToolUserDataSave/PhysicsPlacer/ 下，
 * 同名覆盖、不同名追加、可删除。
 */
class SPhysicsPlacer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPhysicsPlacer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPhysicsPlacer();

private:
	// ---------- 选中的物体 ----------
	TArray<TSharedPtr<FSelectedActorItem>> SelectedActors;
	TSharedPtr<SListView<TSharedPtr<FSelectedActorItem>>> ActorListView;

	void RefreshActorList();
	void CaptureEditorSelection();                 // 把编辑器当前选中的 Actor 拉进列表
	void RemoveActor(TSharedPtr<FSelectedActorItem> Item);
	void ClearActors();
	TSharedRef<ITableRow> OnGenerateActorRow(TSharedPtr<FSelectedActorItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	// ---------- 物理模拟 ----------
	bool bSimulating = false;
	bool bStepping = false;                                          // 防止同帧重入每帧步进
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SimComponents;       // 正在模拟的组件
	// 模拟期间给各视口加"强制实时"覆盖；key->原实时状态用于停止时精准恢复
	TArray<TPair<FLevelEditorViewportClient*, bool>> ViewportRealtimeBackup;
	FTSTicker::FDelegateHandle TickHandle;                          // 每帧步进求解器的 FTSTicker 句柄

	void StartSimulation();
	/** 把单个 Actor 的 PrimitiveComponent 设为可移动/重力/物理模拟，并登记到 SimComponents（去重），供 StopSimulation 统一关掉 */
	void EnableActorPhysics(AActor* A);
	void StopSimulation();
	/** FTSTicker 每帧回调：直接步进编辑器世界的 Chaos 求解器，让物体自由掉落 */
	bool TickPhysics(float DeltaTime);

	// ---------- 生成模式（光标检测地面，点击生成物理物体）----------
	bool bSpawnMode = false;                          // 当前是否处于"生成模式"

	FVector SpawnOffset = FVector(0.f, 0.f, 200.f);   // 基准偏移（命中点之上的固定偏移，默认上方 2 米）
	TArray<TSharedPtr<FSpawnMeshEntry>> SpawnMeshes;  // 可生成的多个网格（像刷草模式的列表）
	TSharedPtr<SWrapBox> SpawnMeshWrapBox;            // 生成网格的横向自动换行磁贴容器
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;    // 网格缩略图池（正方形预览用）
	UStaticMesh* DefaultCubeMesh = nullptr;           // 默认立方体（条目无网格时预览/生成用）

	bool   bRandomSpawn      = false;                 // 勾选后随机选一个启用中的网格，否则按列表顺序轮转
	float  RandomOffsetScale = 0.f;                   // 随机偏移系数（0~500）：给生成位置叠加随机抖动
	float  SimSpeedScale     = 1.f;                   // 模拟速度系数（0.1~2.0）：越小物体掉落越慢（2秒→4秒+）
	int32  SpawnSequenceIndex = 0;                    // 顺序生成时的轮转计数

	void ToggleSpawnMode();                           // 进入/退出生成模式（激活/取消编辑器模式）
	void HandleSpawnedActor(AActor* Spawned);         // 生成模式的物体生成后回调：加入列表并自动开物理
	/** 供 EdMode 查询"这次要生成哪个网格、落在哪个偏移"；bConsume=true 表示真正生成（推进顺序/随机），false 仅用于预览 */
	void GetSpawnRequest(UStaticMesh*& OutMesh, FVector& OutOffset, bool bConsume);
	void AddSpawnMeshEntry();                          // "＋ 添加网格"：追加一个默认立方体条目
	void RemoveSpawnMeshEntry(TSharedPtr<FSpawnMeshEntry> Entry);
	void OnSpawnEntryEnabledChanged(TSharedPtr<FSpawnMeshEntry> Entry, bool bNew);
	/** 从内容浏览器拖入网格到生成列表（仿植被模式）：支持一次拖多个，仅接受 StaticMesh */
	bool OnAreSpawnMeshesAcceptable(TArrayView<FAssetData> Assets) const;
	void OnSpawnMeshesDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets);
	/** 重建横向自动换行的网格磁贴列表 */
	void RefreshSpawnList();
	/** 构建单个网格磁贴：正方形预览 + 左上角勾选框 + 下方小号"移除"按钮 */
	TSharedRef<SWidget> BuildSpawnTile(TSharedPtr<FSpawnMeshEntry> Entry);
	void OnRandomSpawnChanged(ECheckBoxState State);
	void OnRandomOffsetChanged(float NewValue);
	void OnSimSpeedChanged(float NewValue);
	void OnSpawnOffsetChanged(float NewValue, int32 Axis); // 基准偏移 X/Y/Z 改变时推给激活中的模式

	// ---------- 位置回溯（JSON 存档，仿 AutoPrefix）----------
	TArray<TSharedPtr<FSavedPose>> SavedPoses;
	TSharedPtr<FSavedPose> CurrentPose;
	FString EditingPoseName;

	FString GetSaveDirectory() const;
	FString GetFullConfigPath() const;
	void LoadAllPosesFromJson();
	bool WriteAllPosesToJson();
	void SaveCurrentPose();
	void DeletePose(TSharedPtr<FSavedPose> Target);
	void RestorePose();

	TSharedRef<SWidget> OnGeneratePoseComboWidget(TSharedPtr<FSavedPose> InItem);
	void OnPoseSelectionChanged(TSharedPtr<FSavedPose> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetCurrentPoseNameText() const;
	TSharedPtr<SComboBox<TSharedPtr<FSavedPose>>> PoseComboBox;

	// ---------- 杂项 ----------
	TSharedPtr<SMultiLineEditableText> LogBox;
	FString LogText;
	void AppendLog(const FString& Message);
};
