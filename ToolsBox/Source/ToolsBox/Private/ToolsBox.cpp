#include "ToolsBox.h"

#include "Editor.h"
#include "ToolMenus.h"
#include "OpenToolsBox_Command.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h" // 必须包含这个才能用 MapAction
#include "Slate_Assist/FIconStyle.h"
#include "Slate_Assist/SlateAssistBuildFunctionLibrary.h"
#include "Tools/Tools.h"
#include "Tools/AutoPrefix/AutoPrefixHook.h"
#include "Tools/PhysicsPlacer/PhysicsPlacerEdMode.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "FToolsBoxModule"
#include "EditorAssetLibrary.h"
#include "Slate_Assist/LanguagesStitch.h"
#include "Widgets/Input/SComboBox.h"

void FToolsBoxModule::StartupModule()
{

	FIconStyle::Initialize();

	FOpenToolsBox_Command::Register();

	FAutoPrefixHook::Register();

	OpenToolsBoxDockTab_CommandList = MakeShareable(new FUICommandList);
	OpenToolsBoxDockTab_CommandList->MapAction
	(
		FOpenToolsBox_Command::Get().ToolsBox_OpenToolsBox,
		FExecuteAction::CreateRaw(this, &FToolsBoxModule::OnButtonClick),
		FCanExecuteAction()
	);


	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ToolsBox", FOnSpawnTab::CreateRaw(this, &FToolsBoxModule::OnSpawnToolsBoxTab))
		.SetDisplayName(LOCTEXT("ToolsBox", "工具箱"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FIconStyle::Get_IconsName(), "ToolsBox.Icon_Anon"));
	//在启动时一次性注册所有子工具窗口
	for (const FTool& Tool : Tools::Get_ToolsData())
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			Tool.ToolTabID, 
			FOnSpawnTab::CreateRaw(this, &FToolsBoxModule::OnSpawnToolTab, Tool.WidgetFactory) 
		)
		.SetDisplayName(Tool.ToolTitle)
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FIconStyle::Get_Icons().GetStyleSetName(), Tool.ToolDockTabIcon));
		
	}
 
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FToolsBoxModule::RegisterMenu));

	// 注册「物理摆放 - 生成模式」编辑器模式（供物理摆放工具激活，用光标检测地面生成物体）
	FEditorModeRegistry::Get().RegisterMode<FPhysicsPlacerEdMode>(
		FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId,
		LOCTEXT("PhysicsPlacerSpawnMode", "物理摆放生成"));

	// 启动即按 uplugin 的 Language 字段加载插件翻译，确保任何工具窗口打开前翻译就已就位
	{
		FString StoredLang = LanguagesStitch::GetLanguageFromUnplugin();
		if (StoredLang.IsEmpty()) StoredLang = TEXT("zh-Hans");
		LanguagesStitch::LoadPluginLocRes(StoredLang);
	}

}



void FToolsBoxModule::RegisterMenu()
{
	
	FToolMenuOwnerScoped OwnerScoped(this);
	
	
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	
	
	FToolMenuSection& Section = Menu->FindOrAddSection("SuBase_LevelEditorToolBar");
	
	
	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FOpenToolsBox_Command::Get().ToolsBox_OpenToolsBox));
	
	
	Entry.SetCommandList(OpenToolsBoxDockTab_CommandList);
}

TSharedRef<SDockTab> FToolsBoxModule::OnSpawnToolsBoxTab(const FSpawnTabArgs& SpawnTabArgs)
{
  
    TSharedPtr<SVerticalBox> ListContainer;
	TSharedPtr<LanguagesStitch> LanguageStitchInstance=MakeShared<LanguagesStitch>();

    SAssignNew(ListContainer, SVerticalBox);
 

    auto RefreshList = [ListContainer](FString SearchText = TEXT(""))
    {
        if (!ListContainer.IsValid()) return;
 
        ListContainer->ClearChildren();
 
        for (const auto& Tool : Tools::Get_ToolsData())
        {
            FString NameStr = Tool.ToolTitle.ToString();
            FString DescStr = Tool.ToolDescription.ToString();
 
            if (SearchText.IsEmpty() || NameStr.Contains(SearchText) || DescStr.Contains(SearchText))
            {
                ListContainer->AddSlot()
                .AutoHeight()
                [
                    SlateAssistBuildFunctionLibrary::MakeToolBlock(
                        Tool.ToolTitle,
                        Tool.ToolDescription,
                        Tool.ToolImage,
                        Tool.ToolTabID,
                        Tool.ToolURL
                    )
                ];
            }
        }
    };
 
    TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [
        SNew(SVerticalBox)
 
        // 1. 顶部搜索栏
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.0f, 10.0f, 10.0f, 5.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SSearchBox)
                .HintText(NSLOCTEXT("ToolsBox", "SearchHint", "工具名称"))
                .OnTextChanged_Lambda([RefreshList](const FText& InText) {
                    RefreshList(InText.ToString()); 
                })
            ]

	        + SHorizontalBox::Slot()
        	.AutoWidth()
			.Padding(2.0f,4.0f, 0, 0)
	        [		        SNew(STextBlock)
		        .Text(LOCTEXT("ToolBoxAuthor", "详情"))
	        ]
 
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0, 0, 0)
            [
            	
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([]()
                {

                	TSharedRef<SWindow> Info= SlateAssistBuildFunctionLibrary::MakeInfoWindow();

                	FSlateApplication::Get().AddWindow(Info);
                	return FReply::Handled();
                })
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.Info"))
                    .DesiredSizeOverride(FVector2D(18, 18))
                ]
            ]
	        + SHorizontalBox::Slot()
        	.AutoWidth()
			.Padding(2.0f, 0, 2.0f, 0)
	        [
		       SAssignNew(LanguageStitchInstance->LanguageComboBox, SComboBox<TSharedPtr<FString>>)
		        .OptionsSource(&LanguageStitchInstance->LanguageOptions)
		        .InitiallySelectedItem(LanguageStitchInstance->SelectedLanguage)
	        	.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) {
					return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : TEXT("")));
				})
	        	.OnSelectionChanged_Lambda([this, LanguageStitchInstance](TSharedPtr<FString> NewSelected, ESelectInfo::Type SelectInfo) {
	        		  // 打开下拉框时 SComboBox 可能回调一个无效的 NewSelected，必须先判空再解引用
	        		  if (NewSelected.IsValid())
	        		  {
	        			  LanguageStitchInstance->SwitchLanguage(*NewSelected);
						  LanguageStitchInstance->SelectedLanguage = NewSelected;
	        		  }
				  })
				  [
					  // ComboBox 闭合时显示的文本
					  SNew(STextBlock)
					  .Text_Lambda([LanguageStitchInstance]() {
						  return FText::FromString(LanguageStitchInstance->SelectedLanguage.IsValid() ? *LanguageStitchInstance->SelectedLanguage : TEXT("Select..."));
					  })
				  ]
			]
        ]
        // 2. 下方滚动列表
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(10.0f, 5.0f, 10.0f, 10.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                ListContainer.ToSharedRef() 
            ]
        ]
    ];
 

    RefreshList();
 
    return SpawnedTab;
}


void FToolsBoxModule::OnButtonClick()
{
	// 打开工具箱
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("ToolsBox"));
}

TSharedRef<SDockTab> FToolsBoxModule::OnSpawnToolTab(const FSpawnTabArgs& SpawnTabArgs, TFunction<TSharedRef<SWidget>()> NewToolTab)
{
	
	TSharedRef<SWidget> Content = NewToolTab ? NewToolTab() : SNullWidget::NullWidget;
 
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Content
		];
}

void FToolsBoxModule::ShutdownModule()
{
	
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("ToolsBox");
	FOpenToolsBox_Command::Unregister();
	FAutoPrefixHook::Unregister();
	FIconStyle::Shutdown();
	// 注销「物理摆放 - 生成模式」编辑器模式
	FEditorModeRegistry::Get().UnregisterMode(FPhysicsPlacerEdMode::EM_PhysicsPlacerEdModeId);
	for (const FTool& Tool : Tools::Get_ToolsData())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Tool.ToolTabID);
	}
    
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("ToolsBox");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FToolsBoxModule, ToolsBox)