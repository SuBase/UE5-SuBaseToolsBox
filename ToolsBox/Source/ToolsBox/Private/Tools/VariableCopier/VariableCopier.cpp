// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VariableCopier/VariableCopier.h"

#include "PropertyCustomizationHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Guid.h"


#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "EdGraphUtilities.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/SMultiLineEditableText.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "VariableCopier"


class SDropTarget : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SDropTarget)
		: _OnDrop()
		, _OnDragOver()
		, _BorderImage(nullptr)
	{}
		SLATE_ARGUMENT(FOnDrop, OnDrop)
		SLATE_ARGUMENT(FOnDragOver, OnDragOver)
		SLATE_ARGUMENT(const FSlateBrush*, BorderImage)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDropEvent = InArgs._OnDrop;
		OnDragOverEvent = InArgs._OnDragOver;
		SBorder::Construct(SBorder::FArguments()
			.BorderImage(InArgs._BorderImage)
			.Padding(4.f)
			[
				InArgs._Content.Widget
			]);
	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (OnDropEvent.IsBound())
		{
			return OnDropEvent.Execute(MyGeometry, DragDropEvent);
		}
		return SBorder::OnDrop(MyGeometry, DragDropEvent);
	}

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (OnDragOverEvent.IsBound())
		{
			return OnDragOverEvent.Execute(MyGeometry, DragDropEvent);
		}
		return SBorder::OnDragOver(MyGeometry, DragDropEvent);
	}

private:
	FOnDrop OnDropEvent;
	FOnDragOver OnDragOverEvent;
};

TSharedRef<FCopyItemDragDrop> FCopyItemDragDrop::New(const TArray<FCopyItem>& InItems, bool bInCut, int32 InSourcePanel)
{
	TSharedRef<FCopyItemDragDrop> Op = MakeShared<FCopyItemDragDrop>();
	Op->Items = InItems;
	Op->bIsCut = bInCut;
	Op->SourcePanelIndex = InSourcePanel;
	// 不显示跟随鼠标的小卡片（按需求移除），拖拽本身照常工作
	return Op;
}

// 拖拽时不显示任何跟随鼠标的小卡片（按需求移除，只保留拖拽功能本身）
TSharedPtr<SWidget> FCopyItemDragDrop::GetDefaultDecorator() const
{
	return nullptr;
}

// =====================================================================
// 界面搭建
// =====================================================================

void SVariableCopier::Construct(const FArguments& InArgs)
{
	Panels[0].Index = 0;
	Panels[1].Index = 1;

	// 用一个工厂函数生成"单个面板"的内容，避免左右两份代码重复
	auto MakePanel = [this](int32 PanelIdx) -> TSharedRef<SWidget>
	{
		return SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// 标题
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(STextBlock).Text(FText::FromString(
					FString::Printf(TEXT("蓝图 %d"), PanelIdx + 1)))
			]

			// 蓝图选择器
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UBlueprint::StaticClass())
				.ObjectPath(TAttribute<FString>::CreateLambda([this, PanelIdx]() { return GetPanelPath(PanelIdx); }))
				.OnObjectChanged_Lambda([this, PanelIdx](const FAssetData& A) { OnPanelPicked(PanelIdx, A); })
				.AllowClear(true)
				.DisplayUseSelected(true)
				.DisplayBrowse(true)
			]

			
			// 本蓝图现有内容（分类树，可折叠）
			+ SVerticalBox::Slot().FillHeight(0.6f).Padding(5, 5, 5, 0)
			[
				SAssignNew(Panels[PanelIdx].SourceListView, STreeView<TSharedPtr<FPanelTreeNode>>)
				.TreeItemsSource(&Panels[PanelIdx].SourceTree)
				.OnGenerateRow(this, &SVariableCopier::OnGenerateSourceRow, PanelIdx)
				.OnGetChildren(this, &SVariableCopier::OnGetSourceChildren)
				.SelectionMode(ESelectionMode::Multi)
			]

			// 待处理区（对面拖进来的东西停这里，等"确定"才落地）
			+ SVerticalBox::Slot().FillHeight(0.4f).Padding(5, 5, 5, 0)
			[
				SNew(SDropTarget)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.OnDrop(FOnDrop::CreateLambda([this, PanelIdx](const FGeometry& G, const FDragDropEvent& E) { return HandleDrop(PanelIdx, G, E); }))
				.OnDragOver(FOnDragOver::CreateLambda([this, PanelIdx](const FGeometry& G, const FDragDropEvent& E) { return HandleDragOver(PanelIdx, G, E); }))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(3)
					[
						SNew(STextBlock).Text(LOCTEXT("PendingTitle", "待处理（拖到这里的，点确定才生效）："))
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SAssignNew(Panels[PanelIdx].PendingListView, STreeView<TSharedPtr<FPanelTreeNode>>)
						.TreeItemsSource(&Panels[PanelIdx].PendingTree)
						.OnGenerateRow(this, &SVariableCopier::OnGeneratePendingRow, PanelIdx)
						.OnGetChildren(this, &SVariableCopier::OnGetPendingChildren)
						.SelectionMode(ESelectionMode::None)
					]
				]
			]
		];
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("AutoPrefixHelp",
					"使用方法：\n"
					"  1. 两边选择要互相复制的蓝图类（拖动前按住shift为复制，alt为剪切）\n"
					"  2. 将要复制或剪切的变量，宏，函数等拖进对方待处理区域内\n"
					"  3. 点击 确定 即可生效\n"
					))
			
			]
		]

		// ---------- 上半部分：左右两个面板 ----------
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(10, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 5, 0)[ MakePanel(0) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5, 0, 0, 0)[ MakePanel(1) ]
		]

		// ---------- 底部中间一排按钮：确定 / 刷新 / 重置 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 0, 10, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)            // 左弹簧，把按钮挤到中间
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.35f, 0.9f))) // 系统蓝
				.Text(LOCTEXT("ConfirmBtn", "确定"))
				.ToolTipText(LOCTEXT("ConfirmTip", "把两个面板待处理里的东西真正写进各自蓝图"))
				.OnClicked_Lambda([this]() { OnConfirm(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "刷新"))
				.ToolTipText(LOCTEXT("RefreshTip", "重新扫描两个蓝图，检查内容是否变化"))
				.OnClicked_Lambda([this]() { OnRefresh(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetBtn", "重置"))
				.ToolTipText(LOCTEXT("ResetTip", "清空两个面板的待处理区（不影响蓝图）"))
				.OnClicked_Lambda([this]() { OnReset(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)            // 右弹簧
			[
				SNew(SSpacer)
			]
		]

		// ---------- 最底：日志 ----------
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 5, 10, 10)
		[
			SNew(SBox).HeightOverride(90.f)
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
	];


}

// =====================================================================
// 类型标记 & 面板列表枚举
// =====================================================================

FString SVariableCopier::TypeTag(ECopyItemType T)
{
	switch (T)
	{
	case ECopyItemType::Variable:        return TEXT("变量");
	case ECopyItemType::EventDispatcher:  return TEXT("事件分发器");
	case ECopyItemType::Function:         return TEXT("函数");
	case ECopyItemType::Macro:            return TEXT("宏");
	case ECopyItemType::EventGraph:       return TEXT("事件图表");
	}
	return TEXT("");
}

// 5 个分类区的顺序：变量 / 事件分发器 / 函数 / 宏 / 事件图表
static constexpr int32 CatCount = 5;
static ECopyItemType CatTypes[CatCount] =
{
	ECopyItemType::Variable,
	ECopyItemType::EventDispatcher,
	ECopyItemType::Function,
	ECopyItemType::Macro,
	ECopyItemType::EventGraph,
};
static const FText CatNames[CatCount] =
{
	LOCTEXT("NVar", "变量"),
	LOCTEXT("NDisp", "事件分发器"),
	LOCTEXT("NFn", "函数"),
	LOCTEXT("NMacro", "宏"),
	LOCTEXT("NEnv", "事件图表"),
};

// 把"类型"映射到 5 个分类区里对应的下标
static int32 CatIndexForType(ECopyItemType T)
{
	for (int32 i = 0; i < CatCount; ++i)
	{
		if (CatTypes[i] == T) return i;
	}
	return 0;
}

void SVariableCopier::RefreshPanel(int32 PanelIdx)
{
	FPanel& Panel = Panels[PanelIdx];
	Panel.Items.Empty();
	if (Panel.Blueprint)
	{
		// 变量 与 事件分发器 都躺在 NewVariables 里，靠 VarType 的 PinCategory 区分：
		// 事件分发器是 PINCategory == PC_MCDelegate 的特殊变量。
		for (const FBPVariableDescription& V : Panel.Blueprint->NewVariables)
		{
			const bool bDispatcher = V.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate;
			Panel.Items.Add(MakeVarItem(V, bDispatcher));
		}
		for (UEdGraph* G : Panel.Blueprint->FunctionGraphs)   Panel.Items.Add(MakeGraphItem(G, ECopyItemType::Function));
		for (UEdGraph* G : Panel.Blueprint->MacroGraphs)      Panel.Items.Add(MakeGraphItem(G, ECopyItemType::Macro));
		for (UEdGraph* G : Panel.Blueprint->UbergraphPages)   Panel.Items.Add(MakeGraphItem(G, ECopyItemType::EventGraph));

		// 记下每个项来自哪个面板，并一次性把"用到了哪些函数"算好
		// （确定时要靠它去源面板找依赖，算早了才能展开嵌套依赖）
		for (TSharedPtr<FCopyItem>& It : Panel.Items)
		{
			It->SourcePanelIndex = PanelIdx;
			CollectUsed(*It, Panel.Blueprint, Panel.Items);
		}
	}
	BuildSourceTree(PanelIdx);
}

void SVariableCopier::RefreshPending(int32 PanelIdx)
{
	BuildPendingTree(PanelIdx);
}

void SVariableCopier::BuildSourceTree(int32 PanelIdx)
{
	FPanel& Panel = Panels[PanelIdx];
	Panel.SourceTree.Empty();

	// 先造 5 个分类区节点
	TSharedPtr<FPanelTreeNode> Cats[CatCount];
	for (int32 i = 0; i < CatCount; ++i)
	{
		Cats[i] = MakeShared<FPanelTreeNode>();
		Cats[i]->bIsCategory = true;
		Cats[i]->CategoryType = CatTypes[i];
		Cats[i]->CategoryLabel = CatNames[i];
	}

	// 每个具体项挂到对应的分类区下
	for (const TSharedPtr<FCopyItem>& It : Panel.Items)
	{
		TSharedPtr<FPanelTreeNode> Leaf = MakeShared<FPanelTreeNode>();
		Leaf->bIsCategory = false;
		Leaf->CategoryType = It->Type;
		Leaf->SourceItem = It;
		Cats[CatIndexForType(It->Type)]->Children.Add(Leaf);
	}

	// 分类区标题带上数量，并收进面板树（没有内容的分类就不显示，免得列表臃肿）
	for (int32 i = 0; i < CatCount; ++i)
	{
		if (Cats[i]->Children.Num() == 0)
		{
			continue;
		}
		Cats[i]->CategoryLabel = FText::Format(LOCTEXT("CatFmt", "{0} ({1})"), CatNames[i], Cats[i]->Children.Num());
		Panel.SourceTree.Add(Cats[i]);
	}

	if (Panel.SourceListView.IsValid())
	{
		Panel.SourceListView->RequestTreeRefresh();
		// 默认把分类区都展开，免得用户看不到内容
		for (TSharedPtr<FPanelTreeNode>& Cat : Panel.SourceTree)
		{
			Panel.SourceListView->SetItemExpansion(Cat, true);
		}
	}
}

void SVariableCopier::BuildPendingTree(int32 PanelIdx)
{
	FPanel& Panel = Panels[PanelIdx];
	Panel.PendingTree.Empty();

	TSharedPtr<FPanelTreeNode> Cats[CatCount];
	for (int32 i = 0; i < CatCount; ++i)
	{
		Cats[i] = MakeShared<FPanelTreeNode>();
		Cats[i]->bIsCategory = true;
		Cats[i]->CategoryType = CatTypes[i];
		Cats[i]->CategoryLabel = CatNames[i];
	}

	for (const TSharedPtr<FPendingItem>& P : Panel.Pending)
	{
		TSharedPtr<FPanelTreeNode> Leaf = MakeShared<FPanelTreeNode>();
		Leaf->bIsCategory = false;
		Leaf->CategoryType = P->Item->Type;
		Leaf->PendingItem = P;
		Cats[CatIndexForType(P->Item->Type)]->Children.Add(Leaf);
	}

	for (int32 i = 0; i < CatCount; ++i)
	{
		if (Cats[i]->Children.Num() == 0)
		{
			continue;
		}
		Cats[i]->CategoryLabel = FText::Format(LOCTEXT("CatFmtP", "{0} ({1})"), CatNames[i], Cats[i]->Children.Num());
		Panel.PendingTree.Add(Cats[i]);
	}

	if (Panel.PendingListView.IsValid())
	{
		Panel.PendingListView->RequestTreeRefresh();
		for (TSharedPtr<FPanelTreeNode>& Cat : Panel.PendingTree)
		{
			Panel.PendingListView->SetItemExpansion(Cat, true);
		}
	}
}

void SVariableCopier::OnGetSourceChildren(TSharedPtr<FPanelTreeNode> Node, TArray<TSharedPtr<FPanelTreeNode>>& Out)
{
	if (Node->bIsCategory)
	{
		Out = Node->Children;
	}
}

void SVariableCopier::OnGetPendingChildren(TSharedPtr<FPanelTreeNode> Node, TArray<TSharedPtr<FPanelTreeNode>>& Out)
{
	if (Node->bIsCategory)
	{
		Out = Node->Children;
	}
}

TSharedPtr<FCopyItem> SVariableCopier::MakeVarItem(const FBPVariableDescription& Var, bool bIsDispatcher)
{
	TSharedPtr<FCopyItem> It = MakeShared<FCopyItem>();
	It->Type = bIsDispatcher ? ECopyItemType::EventDispatcher : ECopyItemType::Variable;
	It->Name = Var.VarName;
	It->VarDesc = Var;
	It->Display = Var.VarName.ToString();
	return It;
}

TSharedPtr<FCopyItem> SVariableCopier::MakeGraphItem(UEdGraph* Graph, ECopyItemType Type)
{
	TSharedPtr<FCopyItem> It = MakeShared<FCopyItem>();
	It->Type = Type;
	It->Name = Graph->GetFName();
	It->SourceGraph = Graph;
	It->Display = Graph->GetName();
	return It;
}

TSharedPtr<FCopyItem> SVariableCopier::FindItemInPanel(int32 PanelIdx, ECopyItemType Type, FName Name) const
{
	for (const TSharedPtr<FCopyItem>& It : Panels[PanelIdx].Items)
	{
		if (It->Type == Type && It->Name == Name)
		{
			return It;
		}
	}
	return nullptr;
}

// 扫描一个图，找出它内部用到的 函数 / 宏 / 成员变量（都限"属于本蓝图"的）
void SVariableCopier::CollectUsed(FCopyItem& Item, UBlueprint* SourceBP, const TArray<TSharedPtr<FCopyItem>>& SourceItems)
{
	Item.UsedItems.Empty();
	if (!Item.SourceGraph || !SourceBP)
	{
		return;
	}

	TSet<TPair<ECopyItemType, FName>> Seen;
	for (UEdGraphNode* Node : Item.SourceGraph->Nodes)
	{
		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			// 调用了本蓝图里某个自定义函数
			const FName Called = Call->FunctionReference.GetMemberName();
			if (SourceBP->FunctionGraphs.ContainsByPredicate(
				[&](UEdGraph* G) { return G->GetFName() == Called; }))
			{
				Seen.Add({ ECopyItemType::Function, Called });
			}
		}
		else if (UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
		{
			// 用了本蓝图里某个宏
			UEdGraph* MG = Macro->GetMacroGraph();
			if (MG && SourceBP->MacroGraphs.Contains(MG))
			{
				Seen.Add({ ECopyItemType::Macro, MG->GetFName() });
			}
		}
		else if (UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node))
		{
			// 读取了本蓝图的成员变量
			const FName VN = VG->GetVarName();
			if (SourceBP->NewVariables.ContainsByPredicate(
				[&](const FBPVariableDescription& V) { return V.VarName == VN; }))
			{
				Seen.Add({ ECopyItemType::Variable, VN });
			}
		}
		else if (UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node))
		{
			const FName VN = VS->GetVarName();
			if (SourceBP->NewVariables.ContainsByPredicate(
				[&](const FBPVariableDescription& V) { return V.VarName == VN; }))
			{
				Seen.Add({ ECopyItemType::Variable, VN });
			}
		}
	}

	for (const TPair<ECopyItemType, FName>& P : Seen)
	{
		Item.UsedItems.Add(P);
	}
}

// =====================================================================
// 拖拽：从一个面板的列表拖出 -> 落入对面面板的待处理区
// =====================================================================

FReply SVariableCopier::HandleItemDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 PanelIdx, TSharedPtr<FCopyItem> RowItem)
{
	// 拖的可能是"选中多项里的某一个"：如果这一行在选中集合里，就整组一起拖；否则只拖它自己
	TArray<TSharedPtr<FPanelTreeNode>> Selected = Panels[PanelIdx].SourceListView->GetSelectedItems();
	TArray<TSharedPtr<FCopyItem>> SelectedItems;
	for (const TSharedPtr<FPanelTreeNode>& N : Selected)
	{
		if (N->SourceItem.IsValid())
		{
			SelectedItems.Add(N->SourceItem);
		}
	}

	TArray<TSharedPtr<FCopyItem>> ToDrag;
	if (SelectedItems.Contains(RowItem))
	{
		ToDrag = SelectedItems;
	}
	else
	{
		ToDrag.Add(RowItem);
	}

	// 拖拽开始那一刻看修饰键：Alt=剪切，否则（含 Shift、普通）=复制
	const FModifierKeysState& Mods = FSlateApplication::Get().GetModifierKeys();
	const bool bCut = Mods.IsAltDown();

	TArray<FCopyItem> Snapshot;
	Snapshot.Reserve(ToDrag.Num());
	for (const TSharedPtr<FCopyItem>& S : ToDrag)
	{
		// 顺手把"用到了哪些函数"算好（基于这个面板当前的蓝图）
		CollectUsed(*S, Panels[PanelIdx].Blueprint, Panels[PanelIdx].Items);
		Snapshot.Add(*S);
	}

	TSharedRef<FCopyItemDragDrop> Op = FCopyItemDragDrop::New(Snapshot, bCut, PanelIdx);
	return FReply::Handled().BeginDragDrop(Op);
}

FReply SVariableCopier::HandleDragOver(int32 PanelIdx, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// 只要拖的是咱们自己的东西、且不是拖回自己这个面板，就允许落下
	TSharedPtr<FCopyItemDragDrop> Op = DragDropEvent.GetOperationAs<FCopyItemDragDrop>();
	if (Op && Op->SourcePanelIndex != PanelIdx)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SVariableCopier::HandleDrop(int32 PanelIdx, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FCopyItemDragDrop> Op = DragDropEvent.GetOperationAs<FCopyItemDragDrop>();
	if (!Op)
	{
		return FReply::Unhandled();
	}
	// 不能拖回自己这个面板（自己到自己对拷没意义）
	if (Op->SourcePanelIndex == PanelIdx)
	{
		return FReply::Unhandled();
	}
	StageItemToPanel(PanelIdx, Op->Items, Op->bIsCut, Op->SourcePanelIndex);
	return FReply::Handled();
}

void SVariableCopier::StageItemToPanel(int32 DstPanel, const TArray<FCopyItem>& Items, bool bCut, int32 SrcPanel)
{
	int32 Count = 0;
	for (const FCopyItem& Src : Items)
	{
		TSharedPtr<FPendingItem> P = MakeShared<FPendingItem>();
		P->Item = MakeShared<FCopyItem>(Src);   // 快照一份，之后源蓝图怎么变都不影响
		P->SourcePanelIndex = SrcPanel;
		P->bIsCut = bCut;
		Panels[DstPanel].Pending.Add(P);
		++Count;
	}
	RefreshPending(DstPanel);
	AppendLog(FString::Printf(TEXT("蓝图 %d 待处理区 +%d 项（来自蓝图 %d，%s）"),
		DstPanel + 1, Count, SrcPanel + 1, bCut ? TEXT("剪切") : TEXT("复制")));
}

// =====================================================================
// 待处理项的下拉操作（复制 / 剪切 / 移除）
// =====================================================================

void SVariableCopier::SetPendingCut(TSharedPtr<FPendingItem> Item, bool bCut)
{
	if (!Item) return;
	Item->bIsCut = bCut;
	// 不知道它在哪个面板，两个都刷一下（开销可忽略）
	RefreshPending(0);
	RefreshPending(1);
}

void SVariableCopier::RemovePending(TSharedPtr<FPendingItem> Item, int32 PanelIdx)
{
	if (!Item) return;
	Panels[PanelIdx].Pending.Remove(Item);
	RefreshPending(PanelIdx);
}

TSharedRef<SWidget> SVariableCopier::MakePendingMenu(TSharedPtr<FPendingItem> Item, int32 PanelIdx)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("M_Copy", "复制"),
		LOCTEXT("M_CopyTip", "这一项应用时会复制（源蓝图保留）"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Item]() { SetPendingCut(Item, false); })));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("M_Cut", "剪切"),
		LOCTEXT("M_CutTip", "这一项应用时会剪切（从源蓝图移除）"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Item]() { SetPendingCut(Item, true); })));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("M_Del", "移除"),
		LOCTEXT("M_DelTip", "从待处理区里拿掉这一项"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Item, PanelIdx]() { RemovePending(Item, PanelIdx); })));

	return MenuBuilder.MakeWidget();
}

// =====================================================================
// 行渲染（分类树）
// =====================================================================

TSharedRef<ITableRow> SVariableCopier::OnGenerateSourceRow(TSharedPtr<FPanelTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable, int32 PanelIdx)
{
	if (Node->bIsCategory)
	{
		// 分类区：一行加粗文字，前面的小箭头由 STreeView 自动画出来，点它就能折叠/展开
		return SNew(STableRow<TSharedPtr<FPanelTreeNode>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 3)
			[
				SNew(STextBlock)
				.Text(Node->CategoryLabel)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		];
	}

	// 叶子：一个具体变量/函数/宏/…
	const FString UsedText = BuildUsedText(Node->SourceItem);

	return SNew(STableRow<TSharedPtr<FPanelTreeNode>>, OwnerTable)
		.OnDragDetected(FOnDragDetected::CreateSP(this, &SVariableCopier::HandleItemDragDetected, PanelIdx, Node->SourceItem))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Node->SourceItem->Display))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(UsedText))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Visibility(UsedText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			]
		];
}

TSharedRef<ITableRow> SVariableCopier::OnGeneratePendingRow(TSharedPtr<FPanelTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable, int32 PanelIdx)
{
	if (Node->bIsCategory)
	{
		return SNew(STableRow<TSharedPtr<FPanelTreeNode>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 3)
			[
				SNew(STextBlock)
				.Text(Node->CategoryLabel)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		];
	}

	const TSharedPtr<FPendingItem>& PItem = Node->PendingItem;
	const FString UsedText = BuildUsedText(PItem->Item);

	return SNew(STableRow<TSharedPtr<FPanelTreeNode>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			// 名字 + 复制/剪切标记 + 用到的函数
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(
						PItem->Item->Display + (PItem->bIsCut ? TEXT("  [剪切]") : TEXT("  [复制]"))))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(UsedText))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Visibility(UsedText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			]
			// 复制/剪切/移除 下拉
			+ SHorizontalBox::Slot().AutoWidth().Padding(4)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text_Lambda([PItem]()
					{
						return FText::FromString(PItem->bIsCut ? TEXT("剪切 ▾") : TEXT("复制 ▾"));
					})
				]
				.OnGetMenuContent(FOnGetContent::CreateLambda([this, PItem, PanelIdx]() { return MakePendingMenu(PItem, PanelIdx); }))
			]
		];
}

FString SVariableCopier::BuildUsedText(const TSharedPtr<FCopyItem>& Item)
{
	if (Item->UsedItems.Num() == 0)
	{
		return FString();
	}
	FString Out = TEXT("用到了：");
	for (int32 i = 0; i < Item->UsedItems.Num(); ++i)
	{
		if (i > 0) Out += TEXT("、");
		Out += TypeTag(Item->UsedItems[i].Key) + TEXT(" ") + Item->UsedItems[i].Value.ToString();
	}
	return Out;
}

// =====================================================================
// 底部按钮
// =====================================================================

void SVariableCopier::OnReset()
{
	Panels[0].Pending.Empty();
	Panels[1].Pending.Empty();
	RefreshPending(0);
	RefreshPending(1);
	AppendLog(TEXT("已重置：两个面板的待处理区清空（蓝图没有被改动）"));
}

void SVariableCopier::OnRefresh()
{
	int32 Missing = 0;
	for (int32 P = 0; P < 2; ++P)
	{
		RefreshPanel(P);   // 重新扫一遍这个面板的蓝图
		for (const TSharedPtr<FPendingItem>& S : Panels[P].Pending)
		{
			// 暂存项记的是"来自哪个面板"，去那个面板的当前列表里找，找不到了说明源里没了
			const int32 Src = S->SourcePanelIndex;
			if (!FindItemInPanel(Src, S->Item->Type, S->Item->Name))
			{
				++Missing;
			}
		}
	}

	if (Missing > 0)
	{
		AppendLog(FString::Printf(
			TEXT("刷新完成：检测到 %d 个待处理项在源蓝图里已经不存在了（可能改名/删除），应用时会跳过它们"), Missing));
	}
	else
	{
		AppendLog(TEXT("刷新完成：两个蓝图已重新扫描，待处理项都还在。"));
	}
}

void SVariableCopier::OnConfirm()
{
	TSet<UBlueprint*> Modified;   // 所有被动过的蓝图（目标 + 被剪切的源），最后统一通知重编译

	for (int32 D = 0; D < 2; ++D)
	{
		FPanel& Panel = Panels[D];
		if (!Panel.Blueprint || Panel.Pending.Num() == 0)
		{
			continue;
		}

		// 1) 展开依赖，排好顺序：被依赖的先处理，自己后处理。
		//    这样复制函数时，它用到的"子函数/宏"会先就位，不会出现空函数。
		TArray<TSharedPtr<FCopyItem>> Ordered;
		TSet<FString> Visited;

		// 递归展开：依赖的子项永远以"复制"身份落地（避免把源里的子函数也删了）
		TFunction<void(TSharedPtr<FCopyItem>, bool)> Expand = [&](TSharedPtr<FCopyItem> It, bool bForceCopy)
		{
			const FString Key = TypeTag(It->Type) + It->Name.ToString();
			if (Visited.Contains(Key))
			{
				return;
			}
			Visited.Add(Key);

			for (const TPair<ECopyItemType, FName>& Dep : It->UsedItems)
			{
				if (TSharedPtr<FCopyItem> DepItem = FindItemInPanel(It->SourcePanelIndex, Dep.Key, Dep.Value))
				{
					Expand(DepItem, true);
				}
			}

			// 依赖项强制复制；顶层项保留用户选的复制/剪切
			if (bForceCopy && It->bIsCut)
			{
				TSharedPtr<FCopyItem> Copy = MakeShared<FCopyItem>(*It);
				Copy->bIsCut = false;
				Ordered.Add(Copy);
			}
			else
			{
				Ordered.Add(It);
			}
		};

		for (const TSharedPtr<FPendingItem>& S : Panel.Pending)
		{
			// S->Item 是拖拽时的快照，它的 SourcePanelIndex 已经记着来自哪个面板，
			// Expand 会据此去源面板的列表里找依赖。
			Expand(S->Item, false);
		}

		// 2) 这些表记录"落地时改了名的东西"，最后统一把引用修回来
		TMap<FName, FName> RenamedGraphs;  // 原图名 -> 目标里的最终名
		TMap<FName, UEdGraph*> CopiedGraphs; // 目标里的最终名 -> 新图指针

		// 先落地 变量 / 事件分发器（它们不依赖图），再落地 函数 / 宏 / 事件图表
		for (const TSharedPtr<FCopyItem>& It : Ordered)
		{
			if (It->Type == ECopyItemType::Variable || It->Type == ECopyItemType::EventDispatcher)
			{
				ApplyItem(*It, Panels[It->SourcePanelIndex].Blueprint, Panel.Blueprint, RenamedGraphs, CopiedGraphs);
			}
		}
		for (const TSharedPtr<FCopyItem>& It : Ordered)
		{
			if (It->Type != ECopyItemType::Variable && It->Type != ECopyItemType::EventDispatcher)
			{
				ApplyItem(*It, Panels[It->SourcePanelIndex].Blueprint, Panel.Blueprint, RenamedGraphs, CopiedGraphs);
			}
		}
		// 3) 修一遍跨图引用（函数调用、宏实例里指向被改名目标的）
		RemapReferences(RenamedGraphs, CopiedGraphs);

		Modified.Add(Panel.Blueprint);
		// 如果有"剪切"，源蓝图也被改了
		for (const TSharedPtr<FPendingItem>& S : Panel.Pending)
		{
			if (S->bIsCut)
			{
				Modified.Add(Panels[S->SourcePanelIndex].Blueprint);
			}
		}

		AppendLog(FString::Printf(TEXT("已把 %d 项写进蓝图 %d（依赖项已自动连带复制）"), Panel.Pending.Num(), D + 1));
	}

	// 4) 统一通知相关蓝图需要重编译
	for (UBlueprint* B : Modified)
	{
		if (B)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);
		}
	}

	// 5) 应用完清空两个待处理区，并重扫两个面板的列表
	Panels[0].Pending.Empty();
	Panels[1].Pending.Empty();
	RefreshPending(0);
	RefreshPending(1);
	RefreshPanel(0);
	RefreshPanel(1);
}

// =====================================================================
// 真正动手：复制 / 移动 单个项
// =====================================================================

void SVariableCopier::ApplyItem(const FCopyItem& Item, UBlueprint* FromBP, UBlueprint* ToBP,
	TMap<FName, FName>& RenamedGraphs, TMap<FName, UEdGraph*>& CopiedGraphs)
{
	if (!FromBP || !ToBP)
	{
		return;
	}
	const bool bCross = FromBP != ToBP;

	// ---------- 变量 / 事件分发器 ----------
	if (Item.Type == ECopyItemType::Variable || Item.Type == ECopyItemType::EventDispatcher)
	{
		// 目标里已经有「同名且同类型」的变量：直接复用，不再复制一份
		// （只比对名字会误复用不同种类的同名变量，导致类型对不上，所以这里连类型一起比）
		const bool bAlreadyThere = ToBP->NewVariables.ContainsByPredicate(
			[&](const FBPVariableDescription& V) { return V.VarName == Item.VarDesc.VarName && V.VarType == Item.VarDesc.VarType; });
		if (bAlreadyThere)
		{
			AppendLog(FString::Printf(TEXT("变量 %s 在目标里已存在，跳过（复用现有）"), *Item.VarDesc.VarName.ToString()));
			return;
		}

		// 目标里同名就自动加后缀，避免冲突
		const FName FinalName = FBlueprintEditorUtils::FindUniqueKismetName(ToBP, Item.VarDesc.VarName.ToString());

		FBPVariableDescription NewDesc = Item.VarDesc;
		NewDesc.VarName = FinalName;
		NewDesc.VarGuid = FGuid::NewGuid();   // 重新生成 GUID，避免和已有变量撞上
		ToBP->NewVariables.Add(NewDesc);

		// 事件分发器还有一张"签名图"，一起复制过去
		if (Item.Type == ECopyItemType::EventDispatcher)
		{
			if (UEdGraph* Sig = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(FromBP, Item.VarDesc.VarName))
			{
				UEdGraph* NSig = FEdGraphUtilities::CloneGraph(Sig, ToBP);
				if (NSig)
				{
					NSig->Rename(*FinalName.ToString(), ToBP, REN_DontCreateRedirectors);
					ToBP->DelegateSignatureGraphs.Add(NSig);
				}
			}
		}

		// 剪切：从源蓝图移除原件（含分发器签名图）
		if (Item.bIsCut && bCross)
		{
			FromBP->NewVariables.RemoveAll(
				[&](const FBPVariableDescription& V) { return V.VarName == Item.VarDesc.VarName; });
			if (Item.Type == ECopyItemType::EventDispatcher)
			{
				FromBP->DelegateSignatureGraphs.RemoveAll(
					[&](UEdGraph* G) { return G->GetFName() == Item.VarDesc.VarName; });
			}
		}
		return;
	}

	// ---------- 函数 / 宏 / 事件图表 ----------
	UEdGraph* Src = Item.SourceGraph;
	if (!Src)
	{
		return;
	}
	const FName OrigName = Src->GetFName();
	const FName FinalName = FBlueprintEditorUtils::FindUniqueKismetName(ToBP, Src->GetName());

	UEdGraph* NG = nullptr;
	if (Item.bIsCut && bCross)
	{
		// 剪切 = 移动原图到目标（不复制）。原图本身只有一份入口节点，直接搬走不会重复。
		Src->Rename(*FinalName.ToString(), ToBP, REN_DontCreateRedirectors);
		NG = Src;
		// 从源图集合移除、加到目标图集合
		if (Item.Type == ECopyItemType::Function)        { FromBP->FunctionGraphs.Remove(Src); ToBP->FunctionGraphs.AddUnique(NG); }
		else if (Item.Type == ECopyItemType::Macro)       { FromBP->MacroGraphs.Remove(Src);   ToBP->MacroGraphs.AddUnique(NG); }
		else                                             { FromBP->UbergraphPages.Remove(Src); ToBP->UbergraphPages.AddUnique(NG); }
	}
	else
	{
		// 复制 = 整图克隆一份（节点、子图都带过来）。
		// DuplicateGraph 已经克隆出带「正确入口/结果节点」的完整图，
		// 注意：千万不要再调用 AddFunctionGraph / AddMacroGraph，
		// 那两个函数内部会再生成一套入口/结果节点 -> 函数图里出现两个入口节点，图直接损坏删不掉。
		// 所以克隆完只管改名并直接塞进目标的图数组。
		NG = Src->GetSchema()->DuplicateGraph(Src);
		if (!NG)
		{
			// DuplicateGraph 只放行函数图/宏图；事件图表走这里（CloneGraph 对事件图可用）
			NG = FEdGraphUtilities::CloneGraph(Src, FromBP);
		}
		if (!NG)
		{
			AppendLog(FString::Printf(TEXT("复制 %s 失败：无法克隆该图，已跳过"), *Src->GetName()));
			return;
		}
		NG->Rename(*FinalName.ToString(), ToBP, REN_DontCreateRedirectors);

		if (Item.Type == ECopyItemType::Function)
		{
			// DuplicateGraph 内部把入口/结果节点的函数引用指向了「克隆时的临时名字」，
			// 改名成最终名字后必须同步改回来，否则函数会指向一个不存在的名字。
			for (UEdGraphNode* Node : NG->Nodes)
			{
				if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					Entry->FunctionReference.SetMemberName(FinalName);
				}
				else if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
				{
					Result->FunctionReference.SetMemberName(FinalName);
				}
			}
			ToBP->FunctionGraphs.AddUnique(NG);
		}
		else if (Item.Type == ECopyItemType::Macro)
		{
			ToBP->MacroGraphs.AddUnique(NG);
		}
		else
		{
			NG->GraphGuid = FGuid::NewGuid();   // 克隆来的事件图可能带着源图的 GUID，刷新一下避免冲突
			ToBP->UbergraphPages.AddUnique(NG);
		}
	}

	RenamedGraphs.Add(OrigName, FinalName);
	CopiedGraphs.Add(FinalName, NG);
}

// 复制/移动完所有图之后，把指向"被改名图"的节点引用修正过来
void SVariableCopier::RemapReferences(const TMap<FName, FName>& RenamedGraphs,
	const TMap<FName, UEdGraph*>& CopiedGraphs)
{
	for (const TPair<FName, UEdGraph*>& Pair : CopiedGraphs)
	{
		UEdGraph* G = Pair.Value;
		if (!G)
		{
			continue;
		}
		for (UEdGraphNode* Node : G->Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				// 这个调用指向的，是某个被改了名的函数吗？
				const FName Called = Call->FunctionReference.GetMemberName();
				if (const FName* PF = RenamedGraphs.Find(Called))
				{
					if (UFunction* F = FindObject<UFunction>(G->GetTypedOuter<UBlueprint>()->SkeletonGeneratedClass, *PF->ToString()))
					{
						Call->SetFromFunction(F);
					}
				}
			}
			else if (UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
			{
				// 找这个宏实例当前指向的宏图名字（即源里的原名）
				UEdGraph* MacGraph = Macro->GetMacroGraph();
				if (MacGraph)
				{
					const FName Called = MacGraph->GetFName();
					if (const FName* PF = RenamedGraphs.Find(Called))
					{
						if (const UEdGraph* const* PG = CopiedGraphs.Find(*PF))
						{
							Macro->SetMacroGraph(const_cast<UEdGraph*>(*PG));
						}
					}
				}
			}
		}
	}
}

// =====================================================================
// 日志
// =====================================================================

void SVariableCopier::AppendLog(const FString& Message)
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
