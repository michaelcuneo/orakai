#pragma once

#include "CoreMinimal.h"

class UCubusGeologyProfile;

/** Plain worker-thread-safe settings for the deterministic landmark field. */
struct ORAKAI_API FCubusLandmarkFieldSettings
{
    bool bEnabled = false;
    int32 Seed = 0;
    float CellSizeVoxels = 320.0f;
    float SpawnChance = 0.75f;
    float MinimumRadiusVoxels = 18.0f;
    float MaximumRadiusVoxels = 38.0f;
    float MinimumHeightVoxels = 18.0f;
    float MaximumHeightVoxels = 42.0f;
    float PlateauRadiusFraction = 0.34f;
    int32 TerraceSteps = 5;
    float TerraceStrength = 0.32f;
    int32 SurfaceMaterialId = 3;
};

/** One sampled weathered-mesa landmark contribution. */
struct ORAKAI_API FCubusLandmarkSample
{
    float Influence = 0.0f;
    float HeightOffset = 0.0f;
    FIntPoint CellCoordinate = FIntPoint::ZeroValue;

    bool IsInside() const
    {
        return Influence > KINDA_SMALL_NUMBER && HeightOffset > KINDA_SMALL_NUMBER;
    }
};

/**
 * Sparse cellular landmark field shared by block and density generation.
 *
 * Each eligible world cell owns at most one broad, terraced stone mesa. The
 * cell hash controls presence, centre, radius and height, so the result is
 * stable across chunk boundaries and reconstructs from the world seed alone.
 */
class ORAKAI_API FCubusLandmarkField
{
public:
    static FCubusLandmarkFieldSettings MakeSettings(
        const UCubusGeologyProfile* GeologyProfile,
        int32 LandmarkSeed
    );

    static FCubusLandmarkSample Sample(
        float WorldX,
        float WorldY,
        const FCubusLandmarkFieldSettings& Settings
    );

private:
    static uint32 HashCell(
        int32 CellX,
        int32 CellY,
        int32 Seed,
        uint32 Salt
    );

    static float HashToUnitFloat(uint32 Hash);

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
