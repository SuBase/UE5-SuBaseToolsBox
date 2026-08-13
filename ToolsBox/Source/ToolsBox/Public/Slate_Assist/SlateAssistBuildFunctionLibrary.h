// Copyright 2026 SuBase. All Rights Reserved.
#pragma once
#include "Templates/SharedPointer.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

class SWidget;

class SlateAssistBuildFunctionLibrary
{
public:

	//这个函数用来循环往工具箱滚动框添加预设工具块
	static TSharedRef<SWidget> MakeToolBlock(const FText& ToolName, const FText& Description, const FName& IconName,const FName& TabID, const FString& URL);


	static TSharedRef<SWindow> MakeInfoWindow();


	static TSharedRef<SWidget> MakeClickableImageLink(const FSlateBrush* InBrush, const FText& InLinkText, const FString& InURL,const FVector2D & IamgeSizeOverride);


	static void SpawnNotifiy(const FText& InText,const float& TimeIn, const float& TimeOut,SNotificationItem::ECompletionState NotificationItem);
};
