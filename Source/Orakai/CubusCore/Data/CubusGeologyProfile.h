#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusGeologyProfile.generated.h"

class UPCGGraphInterface;

USTRUCT(BlueprintType)
struct FCubusStrataLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 MaterialId = 1;

    UPROPERTY(EditAnywhere