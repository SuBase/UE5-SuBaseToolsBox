// Copyright 2026 SuBase. All Rights Reserved.
#include "Slate_Assist/FIconStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"

TSharedPtr<FSlateStyleSet> FIconStyle::Icons = nullptr;
TSharedPtr<FSlateStyleSet> FIconStyle::Images = nullptr;
 
void FIconStyle::Initialize()
{
	FString ContentDir_Image = IPluginManager::Get().FindPlugin(TEXT("ToolsBox"))->GetBaseDir() / TEXT("Resources/Image");
	FString ContentDir_Icon = IPluginManager::Get().FindPlugin(TEXT("ToolsBox"))->GetBaseDir() / TEXT("Resources/Icon");
 
	// 优先初始化 Images
	if (!Images.IsValid())
	{
		Images = MakeShareable(new FSlateStyleSet("ToolsBoxDockTabStyle"));
		Images->SetContentRoot(ContentDir_Image);
		Images->Set("ToolsBox.Image_Anon_254px",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Icon_Anon"), TEXT(".png")), FVector2D(254.0f, 254.0f)));
		Images->Set("ToolsBox.Image_Anon_1K",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_Anon_1K"), TEXT(".png")), FVector2D(1125.0f, 1119.0f)));
		Images->Set("Info.Info_Bilibili",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Info_Bilibili"), TEXT(".png")), FVector2D(947, 116)));

		Images->Set("Info.Info_github",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_GitHub"), TEXT(".jpeg")), FVector2D(947, 116)));


		Images->Set("Info.Info_zf",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_zf"), TEXT(".png")), FVector2D(821, 605)));

		Images->Set("Info.Info_Paypal",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Paypal_QR"), TEXT(".png")), FVector2D(240, 290)));


		//孤独摇滚
		Images->Set("ToolsBox.Image_GotohHitori",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_GotohHitori"), TEXT(".jpg")), FVector2D(1080, 1080)));

		Images->Set("ToolsBox.Image_IjichiNijika",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_IjichiNijika"), TEXT(".jpg")), FVector2D(1080, 1080)));

		Images->Set("ToolsBox.Image_KitaIkuyo",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_KitaIkuyo"), TEXT(".jpg")), FVector2D(1080, 1080)));

		Images->Set("ToolsBox.Image_YamadaRyo",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_YamadaRyo"), TEXT(".jpg")), FVector2D(1080, 1080)));


		Images->Set("ToolsBox.Image_Mustumi",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_Mustumi"), TEXT(".png")), FVector2D(500, 500)));


		Images->Set("ToolsBox.Image_Tomori",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_Tomori"), TEXT(".jpg")), FVector2D(980, 908)));


		Images->Set("ToolsBox.Image_AnonBigHead",
			new FSlateImageBrush(Images->RootToContentDir(TEXT("Image_AnonBigHead"), TEXT(".jpg")), FVector2D(1106, 1219)));
		
		FSlateStyleRegistry::RegisterSlateStyle(*Images);
	}
 
	// 后初始化 Icons
	if (!Icons.IsValid())
	{
		Icons = MakeShareable(new FSlateStyleSet("EditorToolsBoxIconStyle"));
		Icons->SetContentRoot(ContentDir_Icon);

		// 这个 Key 的名字必须是 "Context名.变量名"
		// 对应 FOpenToolsBox_Command 里的定义
		Icons->Set("ToolsBox.ToolsBox_OpenToolsBox",// 如果图标要作用在按钮上命名规则为  [按钮的ID名，如TEXT("ToolsBox")] . [按钮指针变量名，如TSharedPtr<FUICommandInfo> ToolsBox_CommandInfo]
			new FSlateImageBrush(Icons->RootToContentDir(TEXT("Icon_SuBaRu"), TEXT(".png")), FVector2D(20.0f, 20.0f)));

		
		// 修正：使用 Icons 自己的指针去拼路径，不要跨变量引用
		Icons->Set("ToolsBox.Icon_Anon",
			new FSlateImageBrush(Icons->RootToContentDir(TEXT("Icon_Anon"), TEXT(".png")), FVector2D(20.0f, 20.0f)));

		Icons->Set("ToolsBox.Icon_Anon2",
			new FSlateImageBrush(Icons->RootToContentDir(TEXT("Icon_Anon2"), TEXT(".png")), FVector2D(20.0f, 20.0f)));
 
		FSlateStyleRegistry::RegisterSlateStyle(*Icons);
	}
}
 
void FIconStyle::Shutdown()
{
	// 消除 (Eliminate) 注册信息，清空内存
	FSlateStyleRegistry::UnRegisterSlateStyle(*Icons);
	FSlateStyleRegistry::UnRegisterSlateStyle(*Images);
	Icons.Reset();
	Images.Reset();
}
 
FName FIconStyle::Get_IconsName() { return Icons->GetStyleSetName(); }

FName FIconStyle::Get_ImagesName(){ return Images->GetStyleSetName();}

const ISlateStyle& FIconStyle::Get_Icons() { return *Icons; }

const ISlateStyle& FIconStyle::Get_Images(){ return *Images;}
