#pragma once


#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"
#include "Widgets/SWidget.h"
#include "./Slate_Assist/LanguagesStitch.h"



#define LOCTEXT_NAMESPACE "Tools"
struct FTool
{

	FText ToolTitle;      // 可翻译显示标题（唯一标题，已去掉冗余的原始 FName 标题）
	FText ToolDescription;
	FName ToolImage;
	FName ToolTabID;      // 身份标识（去重/哈希，每个工具唯一）
	FName ToolDockTabIcon;
	FString ToolURL; // 工具对应的帮助/说明页面链接（点击右上角图标按钮打开）
    

	TFunction<TSharedRef<SWidget>()> WidgetFactory;
 
	FTool(const FText& InTitle, const FText& InDesc, FName InImage, FName InTabID, FName Icon, FString InURL, TFunction<TSharedRef<SWidget>()> InFactory)
		: ToolTitle(InTitle), ToolDescription(InDesc), ToolImage(InImage), ToolTabID(InTabID), ToolDockTabIcon(Icon), ToolURL(InURL), WidgetFactory(InFactory) {}
	
	bool operator==(const FTool& Other) const
	{
		return ToolTabID == Other.ToolTabID;
	}
};


FORCEINLINE uint32 GetTypeHash(const FTool& Thing)
{
	return GetTypeHash(Thing.ToolTabID);
}


class Tools
{

	
public:

	static const TSet<FTool> Get_ToolsData();

	
};


#undef LOCTEXT_NAMESPACE