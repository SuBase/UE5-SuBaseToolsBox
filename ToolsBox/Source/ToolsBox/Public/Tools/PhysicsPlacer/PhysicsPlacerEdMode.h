// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class UStaticMesh;

/**
 * 物理摆放「生成模式」编辑器模式（参考 UE 商城 PhysicalLayout 插件的 FEdMode 写法）。
 *
 * 进入该模式后：
 *   - 鼠标在视口里移动时，每帧从光标位置向前做射线检测（地面或任意可碰撞物体），
 *     命中点（可叠加偏移，例如上方一定距离）处用黄色线框预览将要生成的物体；
 *   - 左键点击即在命中点生成一个带物理的网格物体（默认立方体，可在面板里换网格 / 设偏移）。
 *   - 按住 Alt 点击则不生成，照常交给编辑器处理（方便在生成模式里仍能框选/操作）。
 *
 * 生成出的物体会通过 OnActorSpawned 广播给 Slate 面板，由面板把它加入模拟列表并
 * 自动开启物理步进，于是物体一生成就开始自由掉落。
 */
class FPhysicsPlacerEdMode : public FEdMode
{
public:
	FPhysicsPlacerEdMode();
	virtual ~FPhysicsPlacerEdMode();

	/** 本模式的唯一 ID（注册 / 激活 / 取消激活都靠它） */
	static const FEditorModeID EM_PhysicsPlacerEdModeId;

	// ---- 由 Slate 面板在激活模式时写入的设置 ----
	UStaticMesh* SpawnMesh   = nullptr;   // 要生成的网格；为 nullptr 时用默认立方体
	FVector     SpawnOffset  = FVector::ZeroVector; // 命中点之上的偏移（世界坐标，例如 Z=200 表示上方 2 米）

	/** 生成出一个物体时广播，参数即新生成的 Actor */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActorSpawned, AActor*);
	FOnActorSpawned OnActorSpawned;

	/**
	 * 生成前由面板提供"这次要生成哪个网格、落在哪个偏移"。
	 *   - OutMesh：被选中的网格（nullptr 表示用默认立方体）；
	 *   - OutOffset：相对命中点的偏移（已含基准偏移+随机抖动）；
	 *   - bConsume：true=真正生成（推进顺序/随机），false=仅用于每帧预览（不推进状态）。
	 */
	DECLARE_DELEGATE_ThreeParams(FOnGetSpawnRequest, UStaticMesh*&, FVector&, bool /*bConsume*/);
	FOnGetSpawnRequest OnGetSpawnRequest;

	// ---- FEdMode 接口 ----
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool ProcessCapturedMouseMoves(FEditorViewportClient* InViewportClient, FViewport* InViewport, const TArrayView<FIntPoint>& CapturedMouseMoves) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

private:
	/** 从光标位置向前做射线检测，返回命中的地面/物体 */
	bool Trace(FHitResult& OutHit, FEditorViewportClient* ViewportClient);
	/** 在命中点（含偏移）生成一个带物理的网格物体 */
	void SpawnAt(const FHitResult& Hit);

	FIntPoint CursorPosition = FIntPoint::ZeroValue; // 视口里光标的屏幕坐标
	FVector   PreviewLocation = FVector::ZeroVector;  // 当前预览（将生成）的世界坐标
	FVector   PreviewExtent   = FVector(50.f);        // 预览线框的半尺寸
	bool      bHasPreview     = false;                // 当前光标下是否有有效命中
};
