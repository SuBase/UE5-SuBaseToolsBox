// Copyright 2026 SuBase. All Rights Reserved.
#pragma once
#include "Styling/ISlateStyle.h"
#include "UObject/NameTypes.h"

class FIconStyle
{
public:
	static void Initialize();
	static void Shutdown();   
	static FName Get_IconsName();
	static FName Get_ImagesName();
	static const ISlateStyle& Get_Icons();
	static const ISlateStyle& Get_Images();
 
private:
	static TSharedPtr<FSlateStyleSet> Icons;
	static TSharedPtr<FSlateStyleSet> Images;
};