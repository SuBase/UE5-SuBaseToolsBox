// Copyright 2026 SuBase. All Rights Reserved.
#include "Slate_Assist/SlateAssistBuildFunctionLibrary.h"

#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Slate_Assist/FIconStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Text.h"

TSharedRef<SWidget> SlateAssistBuildFunctionLibrary::MakeToolBlock(const FText& ToolName, const FText& Description, const FName& IconName, const FName& TabID, const FString& URL)
{
    TSharedRef<SOverlay> BlockOverlay = SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")) // 使用编辑器面板边框
            .Padding(10.0f)
            [
                SNew(SHorizontalBox)
 
                // 1. 左侧：图标
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0, 0, 15.0f, 0)
                [
                    SNew(SImage)
                    .Image(FIconStyle::Get_Images().GetBrush(IconName))
                    .DesiredSizeOverride(FVector2D(64.0f, 64.0f)) // 缩减图标大小以适应横条
                ]
 
                // 2. 中间：文本区域（名字在上，描述在下）
                + SHorizontalBox::Slot()
                .FillWidth(1.0f) // 占据剩余所有空间
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
 
                    // 工具名
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, 0, 0, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(ToolName)
                        .ToolTipText(ToolName)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12.0f))
                        .ColorAndOpacity(FLinearColor::White)
                    ]
 
                    // 描述
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(Description)
                        .ToolTipText(Description)
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold",8.5f))
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]
 
                // 3. 右侧：操作按钮
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(10.0f, 0, 0, 0)
                [
                    SNew(SBox)
                    .WidthOverride(100.0f)
                    .HeightOverride(40.0f)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ButtonStyle(FAppStyle::Get(), "PrimaryButton") 
                        .OnClicked_Lambda([TabID]()
                        {
                            FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TabID));
                            return FReply::Handled();
                        })
                        [
                            SNew(STextBlock)
                            .Text(NSLOCTEXT("ToolsBox", "OpenBtn", "打开工具"))
                            .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                        ]
                    ]
                ]
            ]
        ];

    // 工具块右上角：打开作者页面的小链接图标按钮（仅当配置了 URL 时显示）
    if (!URL.IsEmpty())
    {
        BlockOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            .Padding(0.0f, 0.0f, 3.0f, 3.0f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(NSLOCTEXT("ToolsBox", "OpenAuthorURL", "打开作者页面"))
                .OnClicked_Lambda([URL]()
                {
                    FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
                    return FReply::Handled();
                })
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.Link"))
                    .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                ]
            ];
    }

    return SNew(SBox)
        .Padding(FMargin(5.0f, 2.0f))
        [
            BlockOverlay
        ];
}

TSharedRef<SWindow> SlateAssistBuildFunctionLibrary::MakeInfoWindow()
{
    // --- 创建悬浮窗口 ---
    TSharedRef<SWindow> DetailsWindow = SNew(SWindow)
        .Title(NSLOCTEXT("Info", "DetailsTitle", "关于"))
        .ClientSize(FVector2D(947, 600))
        [
            SNew(SScrollBox)
            .Orientation(Orient_Vertical)
            
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    MakeClickableImageLink(FIconStyle::Get_Images().GetBrush("Info.Info_Bilibili"), NSLOCTEXT("Info", "GoBilibili", "前往bilibili"), "https://space.bilibili.com/391627131/",FVector2D(947.0f,116.0f))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [

                    MakeClickableImageLink(FIconStyle::Get_Images().GetBrush("Info.Info_github"), NSLOCTEXT("Info", "GoGithub", "前往github"), "https://github.com/SuBase/UE5-SuBaseToolsBox",FVector2D(1143.0f,295.0f))

                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [

                    MakeClickableImageLink(FIconStyle::Get_Images().GetBrush("Info.Info_zf"), NSLOCTEXT("Info", "Donate", "制作不易，恳求打赏，谢谢喵ฅ^•ﻌ•^ฅ"), "",FVector2D(400, 300))

                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [

                    MakeClickableImageLink(FIconStyle::Get_Images().GetBrush("Info.Info_Paypal"), NSLOCTEXT("Info", "PaypalDonate", "PayPal Scan OR Click"), "https://www.paypal.com/ncp/payment/99AXLB4W99D5A", FVector2D(240, 290))

                ]
            ]
            
        ];

    return DetailsWindow;
}

TSharedRef<SWidget> SlateAssistBuildFunctionLibrary::MakeClickableImageLink(const FSlateBrush* InBrush,
    const FText& InLinkText, const FString& InURL,const FVector2D & IamgeSizeOverride)
{

    FSlateFontInfo Font(FCoreStyle::GetDefaultFont(),30,NAME_None);
    
    return SNew(SButton)
        .ButtonStyle(FCoreStyle::Get(), "NoBorder") 
        .OnClicked_Lambda([InURL]() -> FReply {
           
            FPlatformProcess::LaunchURL(*InURL, nullptr, nullptr);
            return FReply::Handled();
        })
        [
            // 垂直布局：上图片，下文字
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center) // 图片水平居中
            [
                SNew(SImage)
                .Image(InBrush)
                .DesiredSizeOverride(IamgeSizeOverride)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0) 
            .HAlign(HAlign_Center) 
            [
                SNew(STextBlock)
                .Text(InLinkText)
                .ColorAndOpacity(FLinearColor::White)
                .Font(Font)
            ]
        ];
}

void SlateAssistBuildFunctionLibrary::SpawnNotifiy(const FText& InText, const float& TimeIn, const float& TimeOut,SNotificationItem::ECompletionState NotificationItem)
{
    
    FNotificationInfo Info(InText);
    
    Info.ExpireDuration = 10.0f;


    
    Info.FadeInDuration = TimeIn;
    Info.FadeOutDuration = TimeOut;
    
    Info.bUseThrobber = false; 

    Info.bUseSuccessFailIcons = true;
    
    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(NotificationItem);
     
}
