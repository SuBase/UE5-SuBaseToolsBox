// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
//复制本示例模版,并改成自己的感觉名字后,最后到Tools.cpp内对应结构体变量登记即可

class  SBlankTemplateTool : public SCompoundWidget
{
public:
	
	
	SLATE_BEGIN_ARGS(SBlankTemplateTool) {}
	SLATE_END_ARGS()

	
	void Construct(const FArguments& InArgs);
	

};
