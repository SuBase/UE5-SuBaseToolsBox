// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/PhysicsPlacer/PhysicsPlacer.h"

#include "Editor.h"                                       // GEditor
#include "EditorModeManager.h"                            // GLevelEditorModeTools
#include "EditorModeRegistry.h"                           // FEditorModeRegistry
#include "Engine/World.h"                                 // UWorld, ELevelTick, TActorIterator, ETeleportType
#include "Engine/Selection.h"                             // USelection::GetSelectedObjects
#include "Engine/StaticMesh.h"                             // UStaticMesh
#include "GameFramework/Actor.h"                          // AActor, GetActorLabel, SetActorTransform
#include "Components/PrimitiveComponent.h"                // SetSimulatePhysics / SetEnableGravity / WakeAllRigidBodies
#include "Components/SceneComponent.h"                   // SetMobility
#include "LevelEditorViewport.h"                          // FLevelEditorViewportClient
#include "SAssetDropTarget.h"                             // SAssetDropTarget（从内容浏览器拖入网格，仿植被模式）
#include "Widgets/Layout/SWrapBox.h"                      // SWrapBox（生成网格横向自动换行磁贴）
#include "Tools/PhysicsPlacer/PhysicsPlacerEdMode.h"      // FPhysicsPlacerEdMode
#include "UObject/SoftObjectPath.h"                       // FSoftObjectPath（网格选择器路径）
#include "Widgets/Input/SSpinBox.h"                      // SSpinBox（生成偏移 X/Y/Z）
#include "Widgets/Input/SSlider.h"                        // SSlider（模拟速度系数）
#include "Widgets/Input/SCheckBox.h"                     // SCheckBox（启用 / 随机生成 勾选框）
#include "Widgets/SOverlay.h"                            // SOverlay（缩略图 + 勾选框叠加，仿刷草）
#include "Widgets/Layout/SBox.h"                         // SBox（正方形预览框）
#include "AssetThumbnail.h"                              // FAssetThumbnail / FAssetThumbnailPool（网格缩略图）

#include "Physics/Experimental/PhysScene_Chaos.h"        // FPhysScene_Chaos / FPhysicsSolverBase 等
#include "Chaos/Framework/PhysicsSolverBase.h"           // Chaos::FPhysicsSolverBase::AdvanceAndDispatch_External
#include "PBDRigidsSolver.h"                             // Chaos::FPBDRigidsSolver 完整定义（GetSolver 返回类型）

#include "Containers/Ticker.h"                            // FTSTicker
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Tools/ToolUserSaveHelper.h"

#define LOCTEXT_NAMESPACE "PhysicsPlacerTool"

namespace
{
	/** JSON 文件名：所有摆位都追加在同一个文件里 */
	const TCHAR* PhysicsPlacerJsonFileName = TEXT("PhysicsPlacerPoses.json");

	/** 模拟期间给视口临时加的实时覆盖，名字用来 RemoveRealtimeOverride 时精准移除 */
	const FText SimRealtimeOverrideName = LOCTEXT("SimRealtimeOverride", "物理摆放模拟");
}

FString FSpawnMeshEntry::GetName() const
{
	if (Mesh)
	{
		return Mesh->GetName();
	}
	return TEXT("默认立方体");
}

void SPhysicsPlacer::Construct(const FArguments& InArgs)
{
	// 载入之前保存的摆位
	LoadAllPosesFromJson();
	if (SavedPoses.Num() == 0)
	{
		// 给一个空提示用的默认项，避免下拉框完全空白
		CurrentPose.Reset();
	}
	else
	{
		CurrentPose = SavedPoses[0];
	}
	EditingPoseName = CurrentPose.IsValid() ? CurrentPose->Name : TEXT("摆位1");

	// 生成网格缩略图池（每个条目一个 64x64 的正方形预览）
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);
	// 默认立方体：条目无具体网格时用于预览/生成（引擎自带基础形状）
	DefaultCubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));

	ChildSlot
	[
		// 整体包一层滚动框，窗口拉小时仍可滚动查看下方所有内容
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)

		// ---------- 1. 说明 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("Help",
					"物理摆放工具：\n"
					"  1. 进入  生成模式  后，在视口里移动光标会从光标向前检测地面，点击即在该处生成物理物体（带偏移，默认上方 2 米掉落）。Alt+点击不生成。\n"
					"  2. 生成模式支持多个网格：从内容浏览器把 StaticMesh 拖到  生成网格列表  即可加入\n"
					"  3. 或在视口里框选/点选若干物体对象，点 获取编辑器选中 加入下方列表，再 启动模拟 让它们自由掉落；停止模拟 停下并保留当前位置。\n"
					"  4. 模拟前/中/后都可  保存当前位置  记成一份带名字的摆位；不理想时用 位置回溯 返回保存时位置。\n"
					"  （物体需带网格等 PrimitiveComponent；模拟时物体会被设为可移动并开启重力）"))
			]
		]

		// ---------- 2. 选择物体 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(STextBlock).Text(LOCTEXT("SelTitle", "选择场景物体"))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(5, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CaptureBtn", "获取编辑器选中"))
						.ToolTipText(LOCTEXT("CaptureBtnTip", "把当前视口里选中的 Actor 加入下方列表"))
						.OnClicked_Lambda([this]() { CaptureEditorSelection(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ClearBtn", "清空列表"))
						.OnClicked_Lambda([this]() { ClearActors(); return FReply::Handled(); })
					]
				]

				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(5)
				[
					SNew(SBox).HeightOverride(160.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(ActorListView, SListView<TSharedPtr<FSelectedActorItem>>)
							.ListItemsSource(&SelectedActors)
							.OnGenerateRow(this, &SPhysicsPlacer::OnGenerateActorRow)
							.SelectionMode(ESelectionMode::None)
						]
					]
				]
			]
		]

		// ---------- 2.5 生成模式（光标检测地面，点击生成物理物体）----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(STextBlock).Text(LOCTEXT("SpawnTitle", "生成模式"))
					.ToolTipText(LOCTEXT("SpawnTip","光标检测地面，点击生成物理物体"))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(5, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text_Lambda([this]()
						{
							return bSpawnMode
								? LOCTEXT("ExitSpawnBtn", "退出生成模式")
								: LOCTEXT("EnterSpawnBtn", "进入生成模式");
						})
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.ToolTipText(LOCTEXT("SpawnBtnTip", "进入后在视口点击地面即生成物理物体；Alt+点击不生成"))
						.OnClicked_Lambda([this]() { ToggleSpawnMode(); return FReply::Handled(); })
					]
				]

		// ---- 生成网格列表（可多选，像刷草模式的植被列表；支持从内容浏览器拖入）----
		+ SVerticalBox::Slot().AutoHeight().Padding(5, 4, 5, 0)
		[
			SNew(STextBlock).Text(LOCTEXT("MeshListTitle", "生成网格列表"))
			.ToolTipText(LOCTEXT("MeshListTip", "可多选，勾选框控制是否生成；可从内容浏览器拖入网格"))
		]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(5, 0)
		[
			// 落点区域：从内容浏览器拖入 StaticMesh 即加入生成列表（仿植被模式）
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop(this, &SPhysicsPlacer::OnAreSpawnMeshesAcceptable)
			.OnAssetsDropped(this, &SPhysicsPlacer::OnSpawnMeshesDropped)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SBox).HeightOverride(190.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							// 生成网格横向排列、自动换行的磁贴容器（仿植被模式磁贴）
							SAssignNew(SpawnMeshWrapBox, SWrapBox)
							.UseAllottedSize(true)
							.InnerSlotPadding(FVector2D(4.f, 4.f))
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("AddMeshBtn", "+ 添加网格"))
						.ToolTipText(LOCTEXT("AddMeshBtnTip", "追加一个默认立方体条目（也可从内容浏览器拖入具体网格）"))

						.OnClicked_Lambda([this]() { AddSpawnMeshEntry(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DropHint", "可从内容浏览器中拖入添加"))
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		]

			// ---- 生成选项 ----
			+ SVerticalBox::Slot().AutoHeight().Padding(5, 8, 5, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("SpawnOptTitle", "生成选项"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(5, 2, 5, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bRandomSpawn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SPhysicsPlacer::OnRandomSpawnChanged)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("RandomSpawnTxt", "随机生成网格"))
					.ToolTipText(LOCTEXT("RandomSpawnTip", "不勾选则按列表顺序生成"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(5, 2, 5, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("RandOffLabel", "随机偏移系数: ")).MinDesiredWidth(100)
					.ToolTipText(LOCTEXT("RandOffUnit", "（给生成位置叠加 ±该值的随机抖动，单位厘米）"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<float>)
					.MinValue(0.f).MaxValue(1000.f).SliderExponent(2.f)
					.Value_Lambda([this]() { return RandomOffsetScale; })
					.OnValueChanged_Lambda([this](float V) { OnRandomOffsetChanged(V); })
					.MinDesiredWidth(80)
				]
				
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(5, 2, 5, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("SimSpeedLabel", "模拟速度系数: ")).MinDesiredWidth(100)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 4, 0)
				[
					SNew(SSlider)
					.MinValue(0.1f).MaxValue(2.0f)
					.Value_Lambda([this]() { return SimSpeedScale; })
					.OnValueChanged_Lambda([this](float V) { OnSimSpeedChanged(V); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%.2f 倍（越小掉得越慢）"), SimSpeedScale)); })
				]
			]

			// ---- 基准偏移（相对命中点的固定偏移）----
			+ SVerticalBox::Slot().AutoHeight().Padding(5, 8, 5, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("BaseOffTitle", "基准偏移(米)"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(5, 2, 5, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("OffX", "X"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<float>)
					.MinValue(-10000.f).MaxValue(10000.f)
					.Value_Lambda([this]() { return SpawnOffset.X; })
					.OnValueChanged_Lambda([this](float V) { OnSpawnOffsetChanged(V, 0); })
					.MinDesiredWidth(60)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 4, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("OffY", "Y"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<float>)
					.MinValue(-10000.f).MaxValue(10000.f)
					.Value_Lambda([this]() { return SpawnOffset.Y; })
					.OnValueChanged_Lambda([this](float V) { OnSpawnOffsetChanged(V, 1); })
					.MinDesiredWidth(60)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 4, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("OffZ", "Z"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<float>)
					.MinValue(-10000.f).MaxValue(10000.f)
					.Value_Lambda([this]() { return SpawnOffset.Z; })
					.OnValueChanged_Lambda([this](float V) { OnSpawnOffsetChanged(V, 2); })
					.MinDesiredWidth(60)
				]
			]
			]
		]

		// ---------- 3. 物理模拟 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(STextBlock).Text(LOCTEXT("SimTitle", "物理模拟"))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text_Lambda([this]()
						{
							return bSimulating
								? LOCTEXT("SimulatingBtn", "正在模拟...")
								: LOCTEXT("StartBtn", "启动模拟");
						})
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.IsEnabled_Lambda([this]() { return !bSimulating; })
						.ToolTipText(LOCTEXT("StartBtnTip", "让列表里的物体开启物理、开始自由掉落"))
						.OnClicked_Lambda([this]() { StartSimulation(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("StopBtn", "停止模拟"))
						.IsEnabled_Lambda([this]() { return bSimulating; })
						.ToolTipText(LOCTEXT("StopBtnTip", "停下物理模拟，物体保留在当前（掉落到的）位置"))
						.OnClicked_Lambda([this]() { StopSimulation(); return FReply::Handled(); })
					]
				]
			]
		]

		// ---------- 4. 位置回溯（摆位存档）----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(STextBlock).Text(LOCTEXT("PoseTitle", "位置回溯（摆位存档）"))
				]

				// 存档下拉框
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("PoseLabel", "选择存档: ")).MinDesiredWidth(80)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(PoseComboBox, SComboBox<TSharedPtr<FSavedPose>>)
						.OptionsSource(&SavedPoses)
						.OnGenerateWidget(this, &SPhysicsPlacer::OnGeneratePoseComboWidget)
						.OnSelectionChanged(this, &SPhysicsPlacer::OnPoseSelectionChanged)
						[
							SNew(STextBlock).Text(this, &SPhysicsPlacer::GetCurrentPoseNameText)
						]
					]
				]

				// 存档名输入
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("PoseNameLabel", "存档名称: ")).MinDesiredWidth(80)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
						.HintText(LOCTEXT("PoseNameHint", "保存时使用的名字，可自定义"))
						.Text_Lambda([this]() { return FText::FromString(EditingPoseName); })
						.OnTextChanged_Lambda([this](const FText& InText) { EditingPoseName = InText.ToString(); })
					]
				]

				// 操作按钮
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SavePoseBtn", "保存当前位置"))
						.ToolTipText(LOCTEXT("SavePoseBtnTip", "把列表里物体的 位置/旋转/缩放 记成一份带名字的摆位（同名覆盖，不同名追加）"))
						.OnClicked_Lambda([this]() { SaveCurrentPose(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("RestoreBtn", "位置回溯"))
						.ToolTipText(LOCTEXT("RestoreBtnTip", "把选中的那套摆位重新套回当前关卡里标签匹配的物体"))
						.OnClicked_Lambda([this]() { RestorePose(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("DeletePoseBtn", "删除存档"))
						.OnClicked_Lambda([this]()
						{
							if (CurrentPose.IsValid()) { DeletePose(CurrentPose); }
							else { AppendLog(TEXT("请先在下拉框选择要删除的存档")); }
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("OpenFolderBtn", "打开配置文件夹"))
						.OnClicked_Lambda([this]()
						{
							FString Dir = GetSaveDirectory();
							IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
							if (!PF.DirectoryExists(*Dir)) { PF.CreateDirectoryTree(*Dir); }
							const FString NativeDir = FPaths::ConvertRelativePathToFull(Dir).Replace(TEXT("/"), TEXT("\\"));
							FPlatformProcess::ExploreFolder(*NativeDir);
							AppendLog(TEXT("已打开配置文件夹: ") + NativeDir);
							return FReply::Handled();
						})
					]
				]
			]
		]

		// ---------- 5. 日志 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5, 10, 10)
		[
			SNew(SBox).HeightOverride(110.f)
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(LogBox, SMultiLineEditableText)
						.IsReadOnly(true)
						.Text(FText::GetEmpty())
					]
				]
			]
		]
		]
	];

	if (PoseComboBox.IsValid() && CurrentPose.IsValid())
	{
		PoseComboBox->SetSelectedItem(CurrentPose);
	}

	AppendLog(FString::Printf(TEXT("已载入 %d 份摆位，已就绪"), SavedPoses.Num()));
}

SPhysicsPlacer::~SPhysicsPlacer()
{
	// 工具被关掉时，如果还在模拟，务必停掉，避免物体残留"模拟中"状态
	StopSimulation();
}

// ============================ 选择物体 ============================

void SPhysicsPlacer::RefreshActorList()
{
	if (ActorListView.IsValid())
	{
		ActorListView->RequestListRefresh();
	}
}

void SPhysicsPlacer::CaptureEditorSelection()
{
	if (!GEditor)
	{
		AppendLog(TEXT("获取失败：找不到编辑器"));
		return;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		AppendLog(TEXT("获取失败：没有选中任何物体"));
		return;
	}

	TArray<AActor*> Selected;
	Selection->GetSelectedObjects<AActor>(Selected);

	int32 Added = 0;
	for (AActor* A : Selected)
	{
		if (!A)
		{
			continue;
		}
		// 去重：已经在列表里的不再加
		bool bExists = false;
		for (const TSharedPtr<FSelectedActorItem>& It : SelectedActors)
		{
			if (It->Actor.Get() == A)
			{
				bExists = true;
				break;
			}
		}
		if (bExists)
		{
			continue;
		}

		TSharedPtr<FSelectedActorItem> NewItem = MakeShared<FSelectedActorItem>();
		NewItem->Actor = A;
		NewItem->DisplayLabel = A->GetActorLabel() + TEXT("  (") + A->GetClass()->GetName() + TEXT(")");
		SelectedActors.Add(NewItem);
		++Added;
	}

	RefreshActorList();
	AppendLog(FString::Printf(TEXT("已从编辑器选中加入 %d 个物体，列表共 %d 个"), Added, SelectedActors.Num()));
}

void SPhysicsPlacer::RemoveActor(TSharedPtr<FSelectedActorItem> Item)
{
	if (!Item.IsValid())
	{
		return;
	}
	SelectedActors.Remove(Item);
	RefreshActorList();
}

void SPhysicsPlacer::ClearActors()
{
	SelectedActors.Empty();
	RefreshActorList();
	AppendLog(TEXT("已清空物体列表"));
}

TSharedRef<ITableRow> SPhysicsPlacer::OnGenerateActorRow(TSharedPtr<FSelectedActorItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FSelectedActorItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4, 2)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					// 物体被删了就在列表里标灰提示
					if (!Item.IsValid() || !Item->Actor.IsValid())
					{
						return FText::FromString(TEXT("<已失效>"));
					}
					return FText::FromString(Item->DisplayLabel);
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveActorBtn", "移除"))
				.OnClicked_Lambda([this, Item]()
				{
					RemoveActor(Item);
					return FReply::Handled();
				})
			]
		];
}

// ============================ 物理模拟 ============================

void SPhysicsPlacer::EnableActorPhysics(AActor* A)
{
	if (!A)
	{
		return;
	}

	// 必须可移动才能被物理驱动
	if (USceneComponent* Root = A->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}

	TArray<UPrimitiveComponent*> PrimComps;
	A->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* PC : PrimComps)
	{
		if (!PC)
		{
			continue;
		}

		// 去重：避免重复登记（例如同一个 Actor 多次进入模拟）
		bool bAlready = false;
		for (const TWeakObjectPtr<UPrimitiveComponent>& WC : SimComponents)
		{
			if (WC.Get() == PC)
			{
				bAlready = true;
				break;
			}
		}

		PC->SetMobility(EComponentMobility::Movable);
		PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PC->SetEnableGravity(true);
		PC->SetSimulatePhysics(true);
		PC->WakeAllRigidBodies();
		// 确保带"模拟"标记的物理体被（重新）创建出来，否则在编辑器中刚开启
		// 模拟时物体不会真正进入动力学求解
		PC->RecreatePhysicsState();

		if (!bAlready)
		{
			SimComponents.Add(PC);
		}
	}
}

void SPhysicsPlacer::StartSimulation()
{
	if (bSimulating)
	{
		AppendLog(TEXT("已经在模拟中，先点  停止模拟  再重启"));
		return;
	}
	if (SelectedActors.Num() == 0)
	{
		// 列表为空也允许开启物理步进：生成模式下点击生成的物体会立刻掉落
		AppendLog(TEXT("提示：当前列表没有物体，已开启物理步进（生成模式下的物体会掉落）；可先加入物体或点击地面生成"));
	}

	UWorld* World = (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
	if (!World)
	{
		AppendLog(TEXT("启动失败：找不到编辑器世界"));
		return;
	}

	SimComponents.Empty();
	int32 SimCount = 0;
	for (const TSharedPtr<FSelectedActorItem>& Item : SelectedActors)
	{
		AActor* A = Item.IsValid() ? Item->Actor.Get() : nullptr;
		if (!A)
		{
			continue;
		}
		EnableActorPhysics(A);
		++SimCount;
	}

	// 注意：列表为空也照常启动步进。生成模式下点击生成的物体自身会开启物理，
	// 由下面的每帧求解器步进驱动掉落；因此不能因为 SimCount==0 就直接退出（旧逻辑会
	// 导致"进入生成模式"后物体根本不掉落）。
	if (SimCount == 0)
	{
		AppendLog(TEXT("提示：当前列表没有物体，已开启物理步进（生成模式下的物体会掉落）；可先加入物体或点击地面生成"));
	}

	// 模拟期间把各关卡视口"强制实时"打开：让 FChaosScene::Tick 把求解器推进出来的
	// 结果同步回组件，并刷新视口显示。同时记下每个视口原本的实时状态，停止时再精准恢复。
	// （物体真正"掉落"靠下面注册的每帧求解器步进，而不是依赖编辑器世界的物理 tick 组）
	ViewportRealtimeBackup.Empty();
	if (GEditor)
	{
		for (FLevelEditorViewportClient* VC : GEditor->GetLevelViewportClients())
		{
			if (VC)
			{
				ViewportRealtimeBackup.Add(TPair<FLevelEditorViewportClient*, bool>(VC, VC->IsRealtime()));
				VC->AddRealtimeOverride(true, SimRealtimeOverrideName);
			}
		}
	}

	// 注册每帧回调：直接步进编辑器世界的 Chaos 求解器（参考 PhysicalLayout 做法）。
	// 用 FTSTicker 是因为本工具是普通 Slate 面板，没有 FEdMode::Tick 那样现成的每帧入口。
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SPhysicsPlacer::TickPhysics));

	bSimulating = true;
	AppendLog(FString::Printf(TEXT("已启动物理模拟：%d 个组件开始自由掉落（直接步进 Chaos 求解器）"), SimCount));
}

bool SPhysicsPlacer::TickPhysics(float DeltaTime)
{
	if (!bSimulating)
	{
		return false; // 取消注册，停止每帧回调
	}

	// 同帧重入守卫（FTSTicker 理论上不会重入，但求稳）
	if (bStepping)
	{
		return true;
	}
	bStepping = true;

	UWorld* World = (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
	if (!World)
	{
		bStepping = false;
		StopSimulation();
		return false;
	}

	// 直接步进世界里的 Chaos 求解器，让开启了 SimulatePhysics 的刚体真正下落。
	// 这一步绕开了编辑器世界默认的 bShouldSimulatePhysics=false 门控——编辑器自带的
	// FChaosScene::StartFrame 对"非模拟世界"用 dt=0 步进（零移动），而 PhysicalLayout
	// 插件正是靠每帧手动 AdvanceAndDispatch_External 才真正掉落的。
	// 同步回组件（OnSyncBodies）则交给"强制实时"下编辑器自己的 FChaosScene::EndFrame。
	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (PhysScene)
	{
		if (auto* Solver = PhysScene->GetSolver())
		{
			// 与 PhysicalLayout 一致：先拉碰撞数据，再按真实帧时间步进。
			// GetSolver 返回基类指针，StartingSceneSimulation 在 FPBDRigidsSolver 上，故向下转换。
			if (auto* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(Solver))
			{
				RigidsSolver->StartingSceneSimulation();
			}
			// 夹紧单帧步长，避免首帧/卡顿时一次跳太大导致穿模或数值爆炸。
			// 再乘 SimSpeedScale：<1 让求解器推进更少的仿真时间 → 物体掉落更慢
			// （例如 0.5 倍 ≈ 原来 2 秒落地变成 4 秒，方便观察空中掉落的时机）。
			const Chaos::FReal StepDt = static_cast<Chaos::FReal>(FMath::Min(DeltaTime, 1.f / 30.f) * SimSpeedScale);
			Solver->AdvanceAndDispatch_External(StepDt);
		}
	}

	// 实时视口会随求解结果刷新；保险起见再强制重绘一次
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}

	bStepping = false;
	return true; // 继续每帧
}

void SPhysicsPlacer::StopSimulation()
{
	// 注销每帧求解器步进
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	// 关掉各组件的物理：组件会保留当前（被物理带到的）变换，物体就停在那里
	for (TWeakObjectPtr<UPrimitiveComponent>& WC : SimComponents)
	{
		if (UPrimitiveComponent* PC = WC.Get())
		{
			PC->SetSimulatePhysics(false);
		}
	}
	SimComponents.Empty();

	// 恢复视口实时状态（移除我们加的"强制实时"覆盖即可回到原来的值）
	for (const TPair<FLevelEditorViewportClient*, bool>& Pair : ViewportRealtimeBackup)
	{
		if (Pair.Key)
		{
			Pair.Key->RemoveRealtimeOverride(SimRealtimeOverrideName);
		}
	}
	ViewportRealtimeBackup.Empty();

	const bool bWasSimulating = bSimulating;
	bSimulating = false;

	// 停止模拟时如果还在生成模式，一并退出（否则生成的物体将不再掉落，容易误解）
	if (bSpawnMode)
	{
		if (FPhysicsPlacerEdMode* Mode = GLevelEditorModeTools().GetActiveModeTyped<FPhysicsPlacerEdMode>(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId))
		{
			Mode->OnActorSpawned.Clear();
		}
		GLevelEditorModeTools().DeactivateMode(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId);
		bSpawnMode = false;
	}

	if (bWasSimulating)
	{
		AppendLog(TEXT("已停止模拟，物体保持在停止时的位置（不满意可  位置回溯  套回之前保存的摆位）"));
	}
}

// ============================ 生成模式（光标检测地面生成）============================

void SPhysicsPlacer::ToggleSpawnMode()
{
	if (!bSpawnMode)
	{
		// 激活编辑器模式（FEditorModeRegistry 里注册的那个），进入后它会每帧从光标做射线检测
		GLevelEditorModeTools().ActivateMode(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId);

		if (FPhysicsPlacerEdMode* Mode = GLevelEditorModeTools().GetActiveModeTyped<FPhysicsPlacerEdMode>(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId))
		{
			// 兜底：先把基准偏移 + 第一个启用网格设进去（真正生成走下方委托）
			Mode->SpawnOffset = SpawnOffset;
			for (const TSharedPtr<FSpawnMeshEntry>& E : SpawnMeshes)
			{
				if (E->bEnabled && E->Mesh)
				{
					Mode->SpawnMesh = E->Mesh;
					break;
				}
			}
			// 生成前由面板决定"生成哪个网格 / 落在哪个偏移"（支持多网格随机/顺序 + 随机偏移）
			Mode->OnGetSpawnRequest = FPhysicsPlacerEdMode::FOnGetSpawnRequest::CreateLambda(
				[WeakThis = TWeakPtr<SPhysicsPlacer>(SharedThis(this))](UStaticMesh*& M, FVector& O, bool bConsume)
				{
					if (TSharedPtr<SPhysicsPlacer> S = WeakThis.Pin())
					{
						S->GetSpawnRequest(M, O, bConsume);
					}
				});
			// 生成物体时回调到本工具：加入列表并自动开启物理步进，让物体立刻掉落
			Mode->OnActorSpawned.AddLambda(
				[WeakThis = TWeakPtr<SPhysicsPlacer>(SharedThis(this))](AActor* A)
				{
					if (TSharedPtr<SPhysicsPlacer> S = WeakThis.Pin())
					{
						S->HandleSpawnedActor(A);
					}
				});
		}

		bSpawnMode = true;
		// 进入生成模式即开始物理步进，使生成的物体一出现就开始自由掉落
		if (!bSimulating)
		{
			StartSimulation();
		}
		AppendLog(TEXT("已进入生成模式：在视口里点击地面即可生成物理物体（Alt+点击不生成）"));
	}
	else
	{
		// 退出生成模式：清理回调并取消激活编辑器模式
		if (FPhysicsPlacerEdMode* Mode = GLevelEditorModeTools().GetActiveModeTyped<FPhysicsPlacerEdMode>(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId))
		{
			Mode->OnActorSpawned.Clear();
			Mode->OnGetSpawnRequest.Unbind();
		}
		GLevelEditorModeTools().DeactivateMode(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId);
		bSpawnMode = false;
		AppendLog(TEXT("已退出生成模式"));
	}
}

void SPhysicsPlacer::HandleSpawnedActor(AActor* Spawned)
{
	if (!Spawned)
	{
		return;
	}

	// 确保物理步进在跑，物体生成后才会真的掉落（进入生成模式时通常已经开，这里兜底）
	if (!bSimulating)
	{
		StartSimulation();
	}

	// 把这个新生成的物体也登记进模拟管理：开启物理并加入 SimComponents，
	// 这样"停止模拟"时会一并把它的物理关掉，避免残留 SimulatePhysics 状态。
	EnableActorPhysics(Spawned);

	// 去重后加入列表
	for (const TSharedPtr<FSelectedActorItem>& It : SelectedActors)
	{
		if (It.IsValid() && It->Actor.Get() == Spawned)
		{
			return;
		}
	}

	TSharedPtr<FSelectedActorItem> NewItem = MakeShared<FSelectedActorItem>();
	NewItem->Actor = Spawned;
	NewItem->DisplayLabel = Spawned->GetActorLabel() + TEXT("  (生成)");
	SelectedActors.Add(NewItem);
	RefreshActorList();

	AppendLog(FString::Printf(TEXT("已生成物体  %s  ，列表共 %d 个"), *Spawned->GetActorLabel(), SelectedActors.Num()));
}

void SPhysicsPlacer::GetSpawnRequest(UStaticMesh*& OutMesh, FVector& OutOffset, bool bConsume)
{
	// 收集"启用中且有网格"的条目
	TArray<UStaticMesh*> Enabled;
	for (const TSharedPtr<FSpawnMeshEntry>& E : SpawnMeshes)
	{
		if (E->bEnabled && E->Mesh)
		{
			Enabled.Add(E->Mesh);
		}
	}

	if (Enabled.Num() == 0)
	{
		// 没有任何启用网格：用默认立方体，仅套基准偏移
		OutMesh = nullptr;
		OutOffset = SpawnOffset;
		return;
	}

	if (bConsume)
	{
		if (bRandomSpawn)
		{
			OutMesh = Enabled[FMath::RandRange(0, Enabled.Num() - 1)];
		}
		else
		{
			OutMesh = Enabled[SpawnSequenceIndex % Enabled.Num()];
			++SpawnSequenceIndex;
		}
		// 随机偏移：在每个轴叠加 ±RandomOffsetScale 的随机抖动（单位厘米）
		const FVector Jitter(
			FMath::FRandRange(-1.f, 1.f) * RandomOffsetScale,
			FMath::FRandRange(-1.f, 1.f) * RandomOffsetScale,
			FMath::FRandRange(-1.f, 1.f) * RandomOffsetScale);
		OutOffset = SpawnOffset + Jitter;
	}
	else
	{
		// 预览：显示"下一个将生成的网格"与基准偏移（不推进顺序、不加随机抖动，避免预览乱跳）
		const int32 Idx = bRandomSpawn ? 0 : (SpawnSequenceIndex % Enabled.Num());
		OutMesh = Enabled[Idx];
		OutOffset = SpawnOffset;
	}
}

void SPhysicsPlacer::AddSpawnMeshEntry()
{
	TSharedPtr<FSpawnMeshEntry> NewEntry = MakeShared<FSpawnMeshEntry>();
	NewEntry->Mesh = DefaultCubeMesh;   // 默认立方体（有真实网格，可直接参与生成与预览）
	NewEntry->bEnabled = true;
	SpawnMeshes.Add(NewEntry);
	RefreshSpawnList();
}

void SPhysicsPlacer::RemoveSpawnMeshEntry(TSharedPtr<FSpawnMeshEntry> Entry)
{
	if (!Entry.IsValid())
	{
		return;
	}
	SpawnMeshes.Remove(Entry);
	RefreshSpawnList();
}

bool SPhysicsPlacer::OnAreSpawnMeshesAcceptable(TArrayView<FAssetData> Assets) const
{
	// 仅接受全部都是 StaticMesh 的拖拽（支持一次拖多个）
	for (const FAssetData& A : Assets)
	{
		if (!A.IsInstanceOf<UStaticMesh>())
		{
			return false;
		}
	}
	return Assets.Num() > 0;
}

void SPhysicsPlacer::OnSpawnMeshesDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets)
{
	int32 Added = 0;
	for (const FAssetData& A : Assets)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(A.GetAsset()))
		{
			TSharedPtr<FSpawnMeshEntry> NewEntry = MakeShared<FSpawnMeshEntry>();
			NewEntry->Mesh = Mesh;
			NewEntry->bEnabled = true;
			SpawnMeshes.Add(NewEntry);
			++Added;
		}
	}
	if (Added > 0)
	{
		RefreshSpawnList();
		AppendLog(FString::Printf(TEXT("已从内容浏览器拖入 %d 个网格到生成列表"), Added));
	}
}

void SPhysicsPlacer::OnSpawnEntryEnabledChanged(TSharedPtr<FSpawnMeshEntry> Entry, bool bNew)
{
	if (Entry.IsValid())
	{
		Entry->bEnabled = bNew;
	}
}

void SPhysicsPlacer::RefreshSpawnList()
{
	if (!SpawnMeshWrapBox.IsValid())
	{
		return;
	}
	SpawnMeshWrapBox->ClearChildren();
	for (const TSharedPtr<FSpawnMeshEntry>& Entry : SpawnMeshes)
	{
		if (Entry.IsValid())
		{
			SpawnMeshWrapBox->AddSlot().Padding(0)
			[
				BuildSpawnTile(Entry)
			];
		}
	}
}

TSharedRef<SWidget> SPhysicsPlacer::BuildSpawnTile(TSharedPtr<FSpawnMeshEntry> Entry)
{
	// 用于缩略图与生成兜底的网格：优先用选定网格，否则用默认立方体
	UStaticMesh* PreviewMesh = Entry->Mesh;
	if (!PreviewMesh)
	{
		PreviewMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
	}

	TSharedRef<SWidget> ThumbWidget = SNullWidget::NullWidget;
	if (PreviewMesh)
	{
		TSharedPtr<FAssetThumbnail> Thumb = MakeShared<FAssetThumbnail>(PreviewMesh, 64, 64, ThumbnailPool);
		ThumbWidget = Thumb->MakeThumbnailWidget();
	}

	// 一个磁贴：上方正方形预览（左上角勾选框控制是否生成），下方小号"移除"按钮
	return SNew(SBox).WidthOverride(72.f)
	[
		SNew(SVerticalBox)

		// 预览图（64x64）+ 左上角勾选框
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						ThumbWidget
					]
				]

				// 左上角勾选框：控制该网格是否参与生成
				+ SOverlay::Slot()
				.HAlign(HAlign_Left).VAlign(VAlign_Top)
				.Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([Entry]()
					{
						return Entry->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Entry](ECheckBoxState State)
					{
						OnSpawnEntryEnabledChanged(Entry, State == ECheckBoxState::Checked);
					})
				]
			]
		]

		// 下方小号"移除"按钮（紧凑显示）
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3, 0, 0)
		[
			SNew(SButton)
			.ContentPadding(FMargin(4, 1))
			.ToolTipText(LOCTEXT("RemoveSpawnTip", "从生成列表移除该网格"))
			.OnClicked_Lambda([this, Entry]()
			{
				RemoveSpawnMeshEntry(Entry);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RemoveSpawnBtn", "移除"))
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 8))
			]
		]
	];
}

void SPhysicsPlacer::OnRandomSpawnChanged(ECheckBoxState State)
{
	bRandomSpawn = (State == ECheckBoxState::Checked);
}

void SPhysicsPlacer::OnRandomOffsetChanged(float NewValue)
{
	RandomOffsetScale = NewValue;
}

void SPhysicsPlacer::OnSimSpeedChanged(float NewValue)
{
	SimSpeedScale = FMath::Clamp(NewValue, 0.1f, 2.0f);
}

void SPhysicsPlacer::OnSpawnOffsetChanged(float NewValue, int32 Axis)
{
	if (Axis == 0)      { SpawnOffset.X = NewValue; }
	else if (Axis == 1) { SpawnOffset.Y = NewValue; }
	else                { SpawnOffset.Z = NewValue; }

	// 实时推给激活中的生成模式
	if (bSpawnMode)
	{
		if (FPhysicsPlacerEdMode* Mode = GLevelEditorModeTools().GetActiveModeTyped<FPhysicsPlacerEdMode>(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId))
		{
			Mode->SpawnOffset = SpawnOffset;
		}
	}
}

// ============================ 位置回溯（JSON 存档）============================

FString SPhysicsPlacer::GetSaveDirectory() const
{
	return FToolUserSave::GetToolSaveDir(TEXT("PhysicsPlacer"));
}

FString SPhysicsPlacer::GetFullConfigPath() const
{
	return GetSaveDirectory() + PhysicsPlacerJsonFileName;
}

void SPhysicsPlacer::LoadAllPosesFromJson()
{
	SavedPoses.Empty();

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GetFullConfigPath()))
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* PosesArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("Poses"), PosesArray) || !PosesArray)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& PoseValue : *PosesArray)
	{
		const TSharedPtr<FJsonObject>* PoseObject = nullptr;
		if (!PoseValue.IsValid() || !PoseValue->TryGetObject(PoseObject) || !PoseObject)
		{
			continue;
		}

		TSharedPtr<FSavedPose> NewPose = MakeShared<FSavedPose>();
		NewPose->Name = (*PoseObject)->GetStringField(TEXT("Name"));

		const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
		if ((*PoseObject)->TryGetArrayField(TEXT("Actors"), ActorsArray) && ActorsArray)
		{
			for (const TSharedPtr<FJsonValue>& ActorValue : *ActorsArray)
			{
				const TSharedPtr<FJsonObject>* ActorObject = nullptr;
				if (!ActorValue.IsValid() || !ActorValue->TryGetObject(ActorObject) || !ActorObject)
				{
					continue;
				}

				FSavedPose::FActorTransform T;
				T.Label = (*ActorObject)->GetStringField(TEXT("Label"));
				T.Location = FVector(
					(*ActorObject)->GetNumberField(TEXT("LocX")),
					(*ActorObject)->GetNumberField(TEXT("LocY")),
					(*ActorObject)->GetNumberField(TEXT("LocZ")));
				T.Rotation = FQuat(
					(*ActorObject)->GetNumberField(TEXT("RotX")),
					(*ActorObject)->GetNumberField(TEXT("RotY")),
					(*ActorObject)->GetNumberField(TEXT("RotZ")),
					(*ActorObject)->GetNumberField(TEXT("RotW")));
				T.Scale = FVector(
					(*ActorObject)->GetNumberField(TEXT("SclX")),
					(*ActorObject)->GetNumberField(TEXT("SclY")),
					(*ActorObject)->GetNumberField(TEXT("SclZ")));
				NewPose->Actors.Add(T);
			}
		}

		SavedPoses.Add(NewPose);
	}
}

bool SPhysicsPlacer::WriteAllPosesToJson()
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> PosesArray;

	for (const TSharedPtr<FSavedPose>& Pose : SavedPoses)
	{
		if (!Pose.IsValid())
		{
			continue;
		}

		const TSharedRef<FJsonObject> PoseObject = MakeShared<FJsonObject>();
		PoseObject->SetStringField(TEXT("Name"), Pose->Name);

		TArray<TSharedPtr<FJsonValue>> ActorsArray;
		for (const FSavedPose::FActorTransform& T : Pose->Actors)
		{
			const TSharedRef<FJsonObject> ActorObject = MakeShared<FJsonObject>();
			ActorObject->SetStringField(TEXT("Label"), T.Label);
			ActorObject->SetNumberField(TEXT("LocX"), T.Location.X);
			ActorObject->SetNumberField(TEXT("LocY"), T.Location.Y);
			ActorObject->SetNumberField(TEXT("LocZ"), T.Location.Z);
			ActorObject->SetNumberField(TEXT("RotX"), T.Rotation.X);
			ActorObject->SetNumberField(TEXT("RotY"), T.Rotation.Y);
			ActorObject->SetNumberField(TEXT("RotZ"), T.Rotation.Z);
			ActorObject->SetNumberField(TEXT("RotW"), T.Rotation.W);
			ActorObject->SetNumberField(TEXT("SclX"), T.Scale.X);
			ActorObject->SetNumberField(TEXT("SclY"), T.Scale.Y);
			ActorObject->SetNumberField(TEXT("SclZ"), T.Scale.Z);
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObject));
		}
		PoseObject->SetArrayField(TEXT("Actors"), ActorsArray);

		PosesArray.Add(MakeShared<FJsonValueObject>(PoseObject));
	}

	Root->SetArrayField(TEXT("Poses"), PosesArray);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutputString, *GetFullConfigPath());
}

void SPhysicsPlacer::SaveCurrentPose()
{
	if (SelectedActors.Num() == 0)
	{
		AppendLog(TEXT("保存失败：列表里没有物体"));
		return;
	}

	const FString Name = EditingPoseName.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		AppendLog(TEXT("保存失败：请输入存档名称"));
		return;
	}

	TSharedPtr<FSavedPose> Pose = MakeShared<FSavedPose>();
	Pose->Name = Name;
	for (const TSharedPtr<FSelectedActorItem>& Item : SelectedActors)
	{
		AActor* A = Item.IsValid() ? Item->Actor.Get() : nullptr;
		if (!A)
		{
			continue;
		}
		FSavedPose::FActorTransform T;
		T.Label = A->GetActorLabel();
		const FTransform Xf = A->GetActorTransform();
		T.Location = Xf.GetLocation();
		T.Rotation = Xf.GetRotation();
		T.Scale = Xf.GetScale3D();
		Pose->Actors.Add(T);
	}

	if (Pose->Actors.Num() == 0)
	{
		AppendLog(TEXT("保存失败：列表里的物体都已失效"));
		return;
	}

	// 同名则覆盖，不同名则追加
	for (int32 i = 0; i < SavedPoses.Num(); ++i)
	{
		if (SavedPoses[i]->Name.Equals(Name, ESearchCase::IgnoreCase))
		{
			SavedPoses[i] = Pose;
			CurrentPose = Pose;
			if (PoseComboBox.IsValid()) { PoseComboBox->RefreshOptions(); PoseComboBox->SetSelectedItem(Pose); }
			if (WriteAllPosesToJson())
			{
				AppendLog(FString::Printf(TEXT("已覆盖存档  %s  ，记录 %d 个物体，文件内共 %d 份 -> %s"),
					*Name, Pose->Actors.Num(), SavedPoses.Num(), *GetFullConfigPath()));
			}
			else
			{
				AppendLog(TEXT("保存失败：无法写入 ") + GetFullConfigPath());
			}
			return;
		}
	}

	SavedPoses.Add(Pose);
	CurrentPose = Pose;
	if (PoseComboBox.IsValid()) { PoseComboBox->RefreshOptions(); PoseComboBox->SetSelectedItem(Pose); }
	if (WriteAllPosesToJson())
	{
		AppendLog(FString::Printf(TEXT("已保存存档  %s  ，记录 %d 个物体，文件内共 %d 份 -> %s"),
			*Name, Pose->Actors.Num(), SavedPoses.Num(), *GetFullConfigPath()));
	}
	else
	{
		AppendLog(TEXT("保存失败：无法写入 ") + GetFullConfigPath());
	}
}

void SPhysicsPlacer::DeletePose(TSharedPtr<FSavedPose> Target)
{
	if (!Target.IsValid())
	{
		return;
	}

	const FString RemovedName = Target->Name;
	SavedPoses.Remove(Target);
	if (CurrentPose == Target)
	{
		CurrentPose = SavedPoses.Num() > 0 ? SavedPoses[0] : nullptr;
	}

	if (WriteAllPosesToJson())
	{
		AppendLog(FString::Printf(TEXT("已删除存档  %s  ，剩余 %d 份"), *RemovedName, SavedPoses.Num()));
	}
	else
	{
		AppendLog(TEXT("删除后写回 JSON 失败"));
	}

	if (PoseComboBox.IsValid())
	{
		PoseComboBox->RefreshOptions();
		if (CurrentPose.IsValid())
		{
			PoseComboBox->SetSelectedItem(CurrentPose);
		}
	}
}

void SPhysicsPlacer::RestorePose()
{
	if (!CurrentPose.IsValid())
	{
		AppendLog(TEXT("回溯失败：请先在上方下拉框选择一份存档"));
		return;
	}

	UWorld* World = (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
	if (!World)
	{
		AppendLog(TEXT("回溯失败：找不到编辑器世界"));
		return;
	}

	// 回溯就是把变换直接套回去，不需要物理；若正在模拟先停掉
	if (bSimulating)
	{
		StopSimulation();
	}

	int32 Applied = 0;
	for (const FSavedPose::FActorTransform& T : CurrentPose->Actors)
	{
		// 按标签在当前世界的所有关卡里找物体（标签通常唯一；若多个同名则都套上同一变换）
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level)
			{
				continue;
			}
			for (AActor* A : Level->Actors)
			{
				if (A && A->GetActorLabel() == T.Label)
				{
					if (USceneComponent* Root = A->GetRootComponent())
					{
						Root->SetMobility(EComponentMobility::Movable);
					}
					A->SetActorTransform(FTransform(T.Rotation, T.Location, T.Scale), false, nullptr, ETeleportType::TeleportPhysics);
					++Applied;
				}
			}
		}
	}

	AppendLog(FString::Printf(TEXT("已回溯到存档  %s  ，匹配并应用 %d 处物体位置"), *CurrentPose->Name, Applied));
}

// ============================ 存档下拉框 ============================

TSharedRef<SWidget> SPhysicsPlacer::OnGeneratePoseComboWidget(TSharedPtr<FSavedPose> InItem)
{
	const FString Label = InItem.IsValid() ? InItem->Name : TEXT("<空>");
	return SNew(STextBlock).Text(FText::FromString(Label));
}

void SPhysicsPlacer::OnPoseSelectionChanged(TSharedPtr<FSavedPose> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid())
	{
		return;
	}
	CurrentPose = NewSelection;
	EditingPoseName = NewSelection->Name;
	AppendLog(FString::Printf(TEXT("已选中存档  %s  （%d 个物体）"), *NewSelection->Name, NewSelection->Actors.Num()));
}

FText SPhysicsPlacer::GetCurrentPoseNameText() const
{
	return FText::FromString(CurrentPose.IsValid() ? CurrentPose->Name : TEXT("<无存档>"));
}

// ============================ 日志 ============================

void SPhysicsPlacer::AppendLog(const FString& Message)
{
	const FString Line = FString::Printf(TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Message);

	LogText = LogText.IsEmpty() ? Line : LogText + LINE_TERMINATOR + Line;

	if (LogBox.IsValid())
	{
		LogBox->SetText(FText::FromString(LogText));
	}
}

#undef LOCTEXT_NAMESPACE
