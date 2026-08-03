#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CubusTreeSpecies.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class ECubusTreeCanopyShape : uint8
{
    ClusteredBroadleaf,
    LayeredConifer,
    Sparse,
    Dead
};

/**
 * Authoring rules for one deterministic stylised tree family.
 */
UCLASS(BlueprintType)
class ORAKAI_API UCubusTreeSpecies final : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName SpeciesName = TEXT("Broadleaf");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    TObjectPtr<UMaterialInterface> BarkMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    TObjectPtr<UMaterialInterface> CanopyMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "100.0"))
    float MinimumHeight = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "100.0"))
    float MaximumHeight = 850.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "5.0"))
    float BaseRadius = 55.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float TopRadiusRatio = 0.28f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "3", ClampMax = "12"))
    int32 TrunkSides = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "2", ClampMax = "16"))
    int32 TrunkSegments = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trunk", meta = (ClampMin = "0.0"))
    float TrunkBend = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "0", ClampMax = "32"))
    int32 MinimumBranches = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "0", ClampMax = "32"))
    int32 MaximumBranches = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BranchStartHeightRatio = 0.42f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "20.0"))
    float MinimumBranchLength = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "20.0"))
    float MaximumBranchLength = 330.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BranchUpwardBias = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branches", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float BranchRadiusRatio = 0.32f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
    ECubusTreeCanopyShape CanopyShape = ECubusTreeCanopyShape::ClusteredBroadleaf;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy", meta = (ClampMin = "0", ClampMax = "32"))
    int32 MinimumCanopyClusters = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy", meta = (ClampMin = "0", ClampMax = "32"))
    int32 MaximumCanopyClusters = 9;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
    FVector MinimumCanopyScale = FVector(180.0, 180.0, 140.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
    FVector MaximumCanopyScale = FVector(300.0, 300.0, 230.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
    FLinearColor BarkTint = FLinearColor(0.34f, 0.19f, 0.09f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canopy")
    FLinearColor CanopyTint = FLinearColor(0.18f, 0.48f, 0.12f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TrunkWindStrength = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BranchWindStrength = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CanopyWindStrength = 1.0f;
};