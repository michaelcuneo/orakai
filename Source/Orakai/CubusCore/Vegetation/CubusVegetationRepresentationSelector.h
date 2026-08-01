#pragma once

#include "CoreMinimal.h"

struct FCubusVegetationRepresentationCandidate
{
    int64 PrimaryBatchKey = 0;
    int64 StaticFallbackBatchKey = 0;

    FTransform LocalTransform;
    float DistanceSquared = MAX_flt;

    bool bHasStaticFallback = false;
};

class ORAKAI_API FCubusVegetationRepresentationSelector
{
public:
    static void RouteCandidates(
        TArray<FCubusVegetationRepresentationCandidate>& Candidates,
        int32 HeroLimit,
        float HeroMaxDistance,
        TMap<int64, TArray<FTransform>>& TransformsByBatchKey
    );
};