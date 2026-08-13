// Copyright 2026 SuBase. All Rights Reserved.
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorActionUtility.h"
#include "ActorAction.generated.h"

/**
 * 
 */
UCLASS()
class TOOLSBOX_API UActorAction : public UActorActionUtility
{
	GENERATED_BODY()

public:





	/** 
	 * 将选中的 Actors 对齐到地面并根据法线调整旋转
	 */
	UFUNCTION(CallInEditor,DisplayName="吸附适应地面")
	void AlignSelectedActorsToGround(float GroundOffset);
};
