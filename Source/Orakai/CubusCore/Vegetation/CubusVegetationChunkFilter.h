#pragma once

#include "CoreMinimal.h"

class ACubusVoxelVolumeActor;

class ORAKAI_API FCubusVegetationChunkFilter
{
public:
    static bool IsWithinCameraRadius(
        const ACubusVoxelVolumeActor* Chunk,
        const FVector& CameraLocation,
        bool bUseCameraChunkCulling,
        int32 HorizontalRadius,
        int32 VerticalRadius
    );
};