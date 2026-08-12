// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/PhysicsPlacer/PhysicsPlacerEdMode.h"

#include "Editor.h"                                  // GEditor
#include "EditorModeManager.h"                        // GLevelEditorModeTools / FEditorModeTools
#include "EditorViewportClient.h"                     // FViewportCursorLocation, CalcSceneView
#include "SceneView.h"                                // FSceneViewFamilyContext, FSceneView
#include "Engine/World.h"                             // UWorld, LineTraceSingleByChannel, ECC_Visibility
#include "Engine/StaticMeshActor.h"                   // AStaticMeshActor
#include "Engine/StaticMesh.h"                        // UStaticMesh
#include "Components/StaticMeshComponent.h"           // UStaticMeshComponent
#include "PrimitiveDrawingUtils.h"                    // DrawWireBox

#define LOCTEXT_NAMESPACE "PhysicsPlacerEdMode"

const FEditorModeID FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId = TEXT("EM_PhysicsPlacerEdMode");

FPhysicsPlacerEdMode::FPhysicsPlacerEdMode()
{
}

FPhysicsPlacerEdMode::~FPhysicsPlacerEdMode()
{
	OnActorSpawned.Clear();
}

void FPhysicsPlacerEdMode::Enter()
{
	FEdMode::Enter();
	bHasPreview = false;
}

void FPhysicsPlacerEdMode::Exit()
{
	OnActorSpawned.Clear();
	bHasPreview = false;
	FEdMode::Exit();
}

bool FPhysicsPlacerEdMode::Trace(FHitResult& OutHit, FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		return false;
	}

	// 用当前视口构造一份场景视图，从而把光标屏幕坐标反投影成世界空间的一条射线
	FSceneViewFamilyContext ViewContext(FSceneViewFamilyContext::ConstructionValues(
		ViewportClient->Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));

	FSceneView* View = ViewportClient->CalcSceneView(&ViewContext);
	if (!View)
	{
		return false;
	}

	// 直接用视口当前光标位置（比依赖 MouseMove 缓存更可靠，避免点下时光标位置过时）
	const int32 CursorX = ViewportClient->Viewport->GetMouseX();
	const int32 CursorY = ViewportClient->Viewport->GetMouseY();
	FViewportCursorLocation Cursor(View, ViewportClient, CursorX, CursorY);

	// 从光标射线原点向方向延伸整个世界的半程，命中地面或其它可碰撞物体
	FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
	Params.bTraceComplex = false;

	return GetWorld()->LineTraceSingleByChannel(
		OutHit,
		Cursor.GetOrigin(),
		Cursor.GetOrigin() + Cursor.GetDirection() * HALF_WORLD_MAX,
		ECC_Visibility,
		Params);
}

void FPhysicsPlacerEdMode::SpawnAt(const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 通过面板提供的委托决定"这次生成哪个网格、落在哪个偏移"（支持多网格随机/顺序 + 随机偏移）
	UStaticMesh* Mesh = SpawnMesh;     // 兜底：未绑委托时用固定网格
	FVector Offset = SpawnOffset;
	if (OnGetSpawnRequest.IsBound())
	{
		OnGetSpawnRequest.Execute(Mesh, Offset, /*bConsume=*/true);
	}

	// 委托没给出网格时用引擎自带的立方体，保证一定能生成
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
	}
	if (!Mesh)
	{
		return;
	}

	const FVector SpawnLocation = Hit.ImpactPoint + Offset;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	AStaticMeshActor* SMA = World->SpawnActor<AStaticMeshActor>(SpawnLocation, SpawnRotation);
	if (!SMA)
	{
		return;
	}

	UStaticMeshComponent* MC = SMA->GetStaticMeshComponent();
	MC->SetStaticMesh(Mesh);
	MC->SetMobility(EComponentMobility::Movable);
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetEnableGravity(true);
	MC->SetSimulatePhysics(true);   // 生成即开启物理：只要面板里物理步进在跑，物体就会立刻掉落
	MC->WakeAllRigidBodies();
	// 显式重建物理体，确保带"模拟"标记的物理体被真正创建（否则编辑器中不进求解）
	MC->RecreatePhysicsState();
	SMA->SetActorLabel(TEXT("物理摆放物体"));

	OnActorSpawned.Broadcast(SMA);
}

void FPhysicsPlacerEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	FHitResult Hit;
	if (Trace(Hit, ViewportClient))
	{
		// 预览也走委托获取"下一个将生成的网格 + 偏移"（bConsume=false 不推进顺序/随机，避免预览时乱跳）
		UStaticMesh* PreviewMesh = SpawnMesh;
		FVector PreviewOff = SpawnOffset;
		if (OnGetSpawnRequest.IsBound())
		{
			OnGetSpawnRequest.Execute(PreviewMesh, PreviewOff, /*bConsume=*/false);
		}
		PreviewLocation = Hit.ImpactPoint + PreviewOff;
		if (PreviewMesh)
		{
			PreviewExtent = PreviewMesh->GetBounds().BoxExtent;
		}
		else
		{
			PreviewExtent = FVector(50.f);
		}
		bHasPreview = true;
	}
	else
	{
		bHasPreview = false;
	}
}

bool FPhysicsPlacerEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
	{
		// 按住 Alt 时不生成，把点击交给编辑器（例如框选）
		const bool bAltDown = Viewport &&
			(Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt));
		if (!bAltDown)
		{
			FHitResult Hit;
			if (Trace(Hit, ViewportClient))
			{
				SpawnAt(Hit);
				bHandled = true;
			}
		}
	}

	return bHandled ? bHandled : FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FPhysicsPlacerEdMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	CursorPosition = FIntPoint(x, y);
	return FEdMode::MouseMove(ViewportClient, Viewport, x, y);
}

bool FPhysicsPlacerEdMode::ProcessCapturedMouseMoves(FEditorViewportClient* InViewportClient, FViewport* InViewport, const TArrayView<FIntPoint>& CapturedMouseMoves)
{
	bool bHandled = FEdMode::ProcessCapturedMouseMoves(InViewportClient, InViewport, CapturedMouseMoves);
	for (const FIntPoint& Move : CapturedMouseMoves)
	{
		CursorPosition = FIntPoint(Move.X, Move.Y);
	}
	return bHandled;
}

void FPhysicsPlacerEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	if (!bHasPreview)
	{
		return;
	}

	// 在"将生成"的位置画一个黄色线框盒子作为预览
	FBox PreviewBox(PreviewLocation - PreviewExtent, PreviewLocation + PreviewExtent);
	DrawWireBox(PDI, PreviewBox, FLinearColor::Yellow, (uint8)SDPG_Foreground, 2.0f);
}

#undef LOCTEXT_NAMESPACE
