#include "CubusCore/Generation/CubusLandmarkField.h"

#include "CubusCore/Data/CubusGeologyProfile.h"

FCubusLandmarkFieldSettings FCubusLandmarkField::MakeSettings(
    const UCubusGeologyProfile* GeologyProfile,
    const int32 LandmarkSeed
)
{
    FCubusLandmarkFieldSettings Settings;
    Settings.Seed = LandmarkSeed;

    if (!IsValid(GeologyProfile))
    {
        return Settings;
    }

    Settings.bEnabled = GeologyProfile->bGenerateLandmarks;
    Settings.CellSizeVoxels = GeologyProfile->LandmarkCellSizeVoxels;
    Settings.SpawnChance = GeologyProfile->LandmarkSpawnChance;
    Settings.MinimumRadiusVoxels = GeologyProfile->LandmarkMinimumRadiusVoxels;
    Settings.MaximumRadiusVoxels = GeologyProfile->LandmarkMaximumRadiusVoxels;
    Settings.MinimumHeightVoxels = GeologyProfile->LandmarkMinimumHeightVoxels;
    Settings.MaximumHeightVoxels = GeologyProfile->LandmarkMaximumHeightVoxels;
    Settings.PlateauRadiusFraction = GeologyProfile->LandmarkPlateauRadiusFraction;
    Settings.TerraceSteps = GeologyProfile->LandmarkTerraceSteps;
    Settings.TerraceStrength = GeologyProfile->LandmarkTerraceStrength;
    Settings.SurfaceMaterialId = GeologyProfile->LandmarkSurfaceMaterialId;
    return Settings;
}

FCubusLandmarkSample FCubusLandmarkField::Sample(
    const float WorldX,
    const float WorldY,
    const FCubusLandmarkFieldSettings& InSettings
)
{
    FCubusLandmarkSample Result;

    if (!InSettings.bEnabled)
    {
        return Result;
    }

    const float CellSize = FMath::Max(32.0f, InSettings.CellSizeVoxels);
    const float SpawnChance = FMath::Clamp(InSettings.SpawnChance, 0.0f, 1.0f);
    const float MinimumRadius = FMath::Clamp(
        FMath::Min(
            InSettings.MinimumRadiusVoxels,
            InSettings.MaximumRadiusVoxels
        ),
        2.0f,
        CellSize * 0.45f
    );
    const float MaximumRadius = FMath::Clamp(
        FMath::Max(
            InSettings.MinimumRadiusVoxels,
            InSettings.MaximumRadiusVoxels
        ),
        MinimumRadius,
        CellSize * 0.45f
    );
    const float MinimumHeight = FMath::Max(
        1.0f,
        FMath::Min(
            InSettings.MinimumHeightVoxels,
            InSettings.MaximumHeightVoxels
        )
    );
    const float MaximumHeight = FMath::Max(
        MinimumHeight,
        FMath::Max(
            InSettings.MinimumHeightVoxels,
            InSettings.MaximumHeightVoxels
        )
    );
    const float PlateauFraction = FMath::Clamp(
        InSettings.PlateauRadiusFraction,
        0.05f,
        0.85f
    );
    const int32 TerraceSteps = FMath::Clamp(InSettings.TerraceSteps, 1, 16);
    const float TerraceStrength = FMath::Clamp(
        InSettings.TerraceStrength,
        0.0f,
        1.0f
    );

    const int32 BaseCellX = FMath::FloorToInt(WorldX / CellSize);
    const int32 BaseCellY = FMath::FloorToInt(WorldY / CellSize);

    for (int32 CellOffsetY = -1; CellOffsetY <= 1; ++CellOffsetY)
    {
        const int32 CellY = BaseCellY + CellOffsetY;

        for (int32 CellOffsetX = -1; CellOffsetX <= 1; ++CellOffsetX)
        {
            const int32 CellX = BaseCellX + CellOffsetX;
            const float PresenceRoll = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0x91E10DA5u)
            );

            if (PresenceRoll > SpawnChance)
            {
                continue;
            }

            const float CentreJitterX = FMath::Lerp(
                0.15f,
                0.85f,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0xD1B54A35u)
                )
            );
            const float CentreJitterY = FMath::Lerp(
                0.15f,
                0.85f,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0x94D049BBu)
                )
            );
            const float CentreX = (static_cast<float>(CellX) + CentreJitterX) * CellSize;
            const float CentreY = (static_cast<float>(CellY) + CentreJitterY) * CellSize;
            const float Radius = FMath::Lerp(
                MinimumRadius,
                MaximumRadius,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0x8538ECB5u)
                )
            );
            const float DeltaX = WorldX - CentreX;
            const float DeltaY = WorldY - CentreY;
            const float NormalizedDistance = FMath::Sqrt(
                DeltaX * DeltaX + DeltaY * DeltaY
            ) / Radius;

            if (NormalizedDistance >= 1.0f)
            {
                continue;
            }

            const float SmoothInfluence = 1.0f - SmoothStep(
                PlateauFraction,
                1.0f,
                NormalizedDistance
            );
            const float TerracedInfluence = FMath::FloorToFloat(
                SmoothInfluence * static_cast<float>(TerraceSteps)
            ) / static_cast<float>(TerraceSteps);
            const float Influence = FMath::Lerp(
                SmoothInfluence,
                TerracedInfluence,
                TerraceStrength
            );
            const float Height = FMath::Lerp(
                MinimumHeight,
                MaximumHeight,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0xC2B2AE3Du)
                )
            );
            const float HeightOffset = Influence * Height;

            if (HeightOffset <= Result.HeightOffset)
            {
                continue;
            }

            Result.Influence = Influence;
            Result.HeightOffset = HeightOffset;
            Result.CellCoordinate = FIntPoint(CellX, CellY);
        }
    }

    return Result;
}

uint32 FCubusLandmarkField::HashCell(
    const int32 CellX,
    const int32 CellY,
    const int32 Seed,
    const uint32 Salt
)
{
    uint32 Hash = static_cast<uint32>(CellX) * 0x8DA6B343u;
    Hash ^= static_cast<uint32>(CellY) * 0xD8163841u;
    Hash ^= static_cast<uint32>(Seed);
    Hash ^= Salt;
    Hash ^= Hash >> 16;
    Hash *= 0x7FEB352Du;
    Hash ^= Hash >> 15;
    Hash *= 0x846CA68Bu;
    Hash ^= Hash >> 16;
    return Hash;
}

float FCubusLandmarkField::HashToUnitFloat(const uint32 Hash)
{
    return static_cast<float>(Hash & 0x00FFFFFFu) /
        static_cast<float>(0x01000000u);
}

float FCubusLandmarkField::SmoothStep(
    const float EdgeMinimum,
    const float EdgeMaximum,
    const float Value
)
{
    if (FMath::IsNearlyEqual(EdgeMinimum, EdgeMaximum))
    {
        return Value >= EdgeMaximum ? 1.0f : 0.0f;
    }

    const float Alpha = FMath::Clamp(
        (Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum),
        0.0f,
        1.0f
    );
    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}
