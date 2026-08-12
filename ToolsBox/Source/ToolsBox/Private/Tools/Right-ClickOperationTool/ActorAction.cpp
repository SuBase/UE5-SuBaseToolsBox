// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/Right-ClickOperationTool/ActorAction.h"

#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Misc/MessageDialog.h"


void UActorAction::AlignSelectedActorsToGround(float GroundOffset)
{
    if (!GEditor) return;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;
 
    TArray<AActor*> SelectedActors;
    GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
 
    if (SelectedActors.Num() == 0) return;
 
   
 
    FScopedTransaction Transaction(FText::FromString("Align Actors to Ground"));
 
    for (AActor* Actor : SelectedActors)
    {
        if (!Actor) continue;
 
        // ... 获取几何中心逻辑 ...
        FBox SphereBounds = Actor->GetComponentsBoundingBox(true);
        FVector GeometryCenter = SphereBounds.GetCenter();
        
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, GeometryCenter, GeometryCenter - (FVector::UpVector * 10000.f), ECC_WorldStatic))
        {
            Actor->Modify();
 
            // 1. 处理绕几何中心旋转补偿
            FVector TargetNormal = Hit.Normal;
            FQuat DeltaSurfRot = FQuat::FindBetweenVectors(FVector::UpVector, TargetNormal);
            FRotator CurrentRot = Actor->GetActorRotation();
            FQuat TargetRotation = DeltaSurfRot * FQuat(FRotator(0, CurrentRot.Yaw, 0));
 
            FVector PivotToCenter = GeometryCenter - Actor->GetActorLocation();
            FVector RotatedPivotToCenter = TargetRotation.RotateVector(Actor->GetActorQuat().UnrotateVector(PivotToCenter));
            
            Actor->SetActorRotation(TargetRotation);
            Actor->SetActorLocation(GeometryCenter - RotatedPivotToCenter);
 
            // 2. 重新计算旋转后的底部位置并加上用户自定义偏移
            Actor->UpdateComponentTransforms();
            FBox NewBounds = Actor->GetComponentsBoundingBox(true);
            
            float OffsetToBottom = Actor->GetActorLocation().Z - NewBounds.Min.Z;
            
            FVector FinalLocation = Actor->GetActorLocation();
            // 最终高度 = 击中点高度 + 枢轴到底部的垂直距离 + 用户偏移量
            FinalLocation.Z = Hit.Location.Z + OffsetToBottom + GroundOffset;
 
            Actor->SetActorLocation(FinalLocation);
            Actor->PostEditMove(true);
        }
    }
    GEditor->RedrawLevelEditingViewports();
}
