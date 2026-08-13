// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetData.h"
#include "EdGraph/EdGraph.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class SMultiLineEditableText;
class SComboButton;

/**
 * 一个"可复印的东西"：蓝图里能Copy的五种资产。
 * 用统一的结构描述，方便两个面板共用同一套代码。
 */
enum class ECopyItemType : uint8
{
	Variable,        // 成员变量
	EventDispatcher, // 事件分发器（在 UE5.8 里其实是一个特殊类型的成员变量 + 一张签名图）
	Function,       // 函数
	Macro,          // 宏
	EventGraph,     // 事件图表
};

struct FCopyItem
{
	ECopyItemType Type = ECopyItemType::Variable;
	FName Name;                 // 在源蓝图里的名字
	FString Display;            // 列表里展示的名字（类型不靠前缀，靠分类区显示）

	// 变量 / 事件分发器用：把整份描述存下来，点确定时原样搬过去
	FBPVariableDescription VarDesc;

	// 函数 / 宏 / 事件图表用：记下源图指针，点确定时按它去复制/移动
	UEdGraph* SourceGraph = nullptr;

	// 这个函数/宏内部还用到了哪些别的东西（函数、宏、成员变量）。
	// 用途一：在界面上提示"用到了哪些函数"；用途二：确定时自动连带复制过去。
	TArray<TPair<ECopyItemType, FName>> UsedItems;

	// 暂存状态：true=剪切（应用后会从源蓝图移除），false=复制
	bool bIsCut = false;

	// 这个项来自哪个面板（0 或 1）。拖拽/确定时用来去对面/源面板找依赖。
	int32 SourcePanelIndex = 0;
};

/** 拖拽时带着的一批东西（支持多选一起拖） */
class FCopyItemDragDrop : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FCopyItemDragDrop, FDecoratedDragDropOp)

	// 被拖着的源项（已经把数据快照好了，拖的过程中源蓝图怎么变都不影响）
	TArray<FCopyItem> Items;
	// 这次拖拽是剪切还是复制（Alt 按下=剪切）
	bool bIsCut = false;
	// 从哪个面板拖出来的（0 或 1），落点面板和它不同才接受
	int32 SourcePanelIndex = 0;

	static TSharedRef<FCopyItemDragDrop> New(const TArray<FCopyItem>& InItems, bool bInCut, int32 InSourcePanel);

	// 拖拽时不显示跟随鼠标的小卡片（按需求移除），仅保留拖拽功能
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};

/** 暂存到某个面板、等待"确定"才落地的项 */
struct FPendingItem
{
	TSharedPtr<FCopyItem> Item;     // 要落地的东西（快照）
	int32 SourcePanelIndex = 0;    // 它来自哪个面板（即来自哪个蓝图）
	bool bIsCut = false;            // 复制还是剪切
};

/**
 * 分类树的一个节点：要么是"分类区"（变量/函数/宏…），要么是"叶子"（具体的某个变量/函数）。
 * 源列表和待处理列表都复用这同一个节点结构，靠 SourceItem / PendingItem 来区分。
 */
struct FPanelTreeNode : public TSharedFromThis<FPanelTreeNode>
{
	bool bIsCategory = false;                 // true=分类区，false=具体某项
	ECopyItemType CategoryType = ECopyItemType::Variable;
	FText CategoryLabel;                      // 分类区显示的文字（带数量，如"函数 (3)"）
	TSharedPtr<FCopyItem> SourceItem;         // 叶子：指向源列表里那一项
	TSharedPtr<FPendingItem> PendingItem;     // 叶子：指向待处理里那一项
	TArray<TSharedPtr<FPanelTreeNode>> Children; // 分类区的子节点
};

/** 一个面板 = 一个蓝图 + 它的内容分类树 + 等别人拖进来的暂存分类树 */
struct FPanel
{
	int32 Index = 0;
	UBlueprint* Blueprint = nullptr;                                  // 这个面板当前选中的蓝图
	TArray<TSharedPtr<FCopyItem>> Items;                              // 这个蓝图里现有的东西（扁平，供逻辑用）
	TArray<TSharedPtr<FPendingItem>> Pending;                        // 从对面拖进来、还没确定的东西
	TArray<TSharedPtr<FPanelTreeNode>> SourceTree;                   // 源列表的分类树（5 个分类区）
	TArray<TSharedPtr<FPanelTreeNode>> PendingTree;                  // 待处理区的分类树（5 个分类区）
	TSharedPtr<STreeView<TSharedPtr<FPanelTreeNode>>> SourceListView;
	TSharedPtr<STreeView<TSharedPtr<FPanelTreeNode>>> PendingListView;
};

/**
 * 蓝图复印机（双面板互拖版）。
 *
 * 用法：
 *   1. 左边、右边各选一个蓝图；
 *   2. 在任一面板里 Ctrl 多选，直接拖到对面面板——
 *        - 普通拖 / Shift 拖 = 复制
 *        - Alt 拖 = 剪切
 *      拖过去的东西会停在对方面板的"待处理"区，并按类型归到对应的可折叠分类区里；
 *   3. 底部中间一排按钮：
 *        - 确定：把两个面板"待处理"里的东西真正写进各自的目标蓝图；
 *        - 刷新：重新扫描两个蓝图，检查源内容是否变了；
 *        - 重置：清空两个面板的待处理区（不动蓝图）。
 *
 * 列表里每一项都按"变量 / 事件分发器 / 函数 / 宏 / 事件图表"分门别类摆在各自的折叠区下，
 * 和蓝图编辑器里"我的蓝图"面板一个样。
 *
 * 关键约束：所有操作都只在你点"确定"之后才落到蓝图里，之前都是"纸上谈兵"。
 * 函数/宏会连带复制它内部用到的子函数/宏/成员变量（先复制子项，避免空函数）。
 */
class SVariableCopier : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVariableCopier) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FPanel Panels[2];

	TSharedPtr<SMultiLineEditableText> LogBox;
	FString LogText;

	// ---------- 面板数据 ----------
	void RefreshPanel(int32 PanelIdx);
	void RefreshPending(int32 PanelIdx);
	// 用 Items / Pending 重新搭出分类树（5 个分类区），并默认展开
	void BuildSourceTree(int32 PanelIdx);
	void BuildPendingTree(int32 PanelIdx);

	// 把蓝图里的数据包装成一个 FCopyItem
	TSharedPtr<FCopyItem> MakeVarItem(const FBPVariableDescription& Var, bool bIsDispatcher);
	TSharedPtr<FCopyItem> MakeGraphItem(UEdGraph* Graph, ECopyItemType Type);
	// 扫描一个图，找出它内部用到的 函数/宏/成员变量，填进 UsedItems
	void CollectUsed(FCopyItem& Item, UBlueprint* SourceBP, const TArray<TSharedPtr<FCopyItem>>& SourceItems);
	// 在某个面板的现有列表里按 类型+名字 找项（用于展开依赖）
	TSharedPtr<FCopyItem> FindItemInPanel(int32 PanelIdx, ECopyItemType Type, FName Name) const;

	// ---------- 分类树 ----------
	void OnGetSourceChildren(TSharedPtr<FPanelTreeNode> Node, TArray<TSharedPtr<FPanelTreeNode>>& Out);
	void OnGetPendingChildren(TSharedPtr<FPanelTreeNode> Node, TArray<TSharedPtr<FPanelTreeNode>>& Out);
	TSharedRef<ITableRow> OnGenerateSourceRow(TSharedPtr<FPanelTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable, int32 PanelIdx);
	TSharedRef<ITableRow> OnGeneratePendingRow(TSharedPtr<FPanelTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable, int32 PanelIdx);

	// ---------- 拖拽 ----------
	FReply HandleItemDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 PanelIdx, TSharedPtr<FCopyItem> RowItem);
	FReply HandleDrop(int32 PanelIdx, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	FReply HandleDragOver(int32 PanelIdx, const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	// 把一批东西放进 DstPanel 的待处理区（来自 SrcPanel）
	void StageItemToPanel(int32 DstPanel, const TArray<FCopyItem>& Items, bool bCut, int32 SrcPanel);

	// ---------- 待处理项的下拉操作（复制 / 剪切 / 移除）----------
	void SetPendingCut(TSharedPtr<FPendingItem> Item, bool bCut);
	void RemovePending(TSharedPtr<FPendingItem> Item, int32 PanelIdx);
	TSharedRef<SWidget> MakePendingMenu(TSharedPtr<FPendingItem> Item, int32 PanelIdx);

	// ---------- 杂项 ----------
	static FString BuildUsedText(const TSharedPtr<FCopyItem>& Item);

	// ---------- 底部按钮 ----------
	void OnConfirm();   // 确定：把两个面板待处理的东西写进各自蓝图
	void OnRefresh();   // 刷新：重新扫描两个蓝图
	void OnReset();     // 重置：清空两个面板的待处理区

	// 真正动手复制/移动一个项；RenamedGraphs 记录"改名后的图"，最后统一修引用
	void ApplyItem(const FCopyItem& Item, UBlueprint* FromBP, UBlueprint* ToBP,
		TMap<FName, FName>& RenamedGraphs, TMap<FName, UEdGraph*>& CopiedGraphs);
	// 复制/移动完所有图之后，把引用到"被改名图"的节点修正过来
	void RemapReferences(const TMap<FName, FName>& RenamedGraphs, const TMap<FName, UEdGraph*>& CopiedGraphs);

	// ---------- 选择器 ----------
	FString GetPanelPath(int32 PanelIdx) const { return Panels[PanelIdx].Blueprint ? Panels[PanelIdx].Blueprint->GetPathName() : FString(); }
	void OnPanelPicked(int32 PanelIdx, const FAssetData& AssetData)
	{
		Panels[PanelIdx].Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		RefreshPanel(PanelIdx);
	}

	static FString TypeTag(ECopyItemType T);
	void AppendLog(const FString& Message);
};
