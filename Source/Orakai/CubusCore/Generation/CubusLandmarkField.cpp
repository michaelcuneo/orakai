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
    Settings.MinimumAspectRatio = GeologyProfile->LandmarkMinimumAspectRatio;
    Settings.MaximumAspectRatio = GeologyProfile->LandmarkMaximumAspectRatio;
    Settings.OutlineIrregularity = GeologyProfile->LandmarkOutlineIrregularity;
    Settings.ShoulderStrength = GeologyProfile->LandmarkShoulderStrength;
    Settings.GullyStrength = GeologyProfile->LandmarkGullyStrength;
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
    const float MinimumAspectRatio = FMath::Clamp(
        FMath::Min(
            InSettings.MinimumAspectRatio,
            InSettings.MaximumAspectRatio
        ),
        1.0f,
        3.0f
    );
    const float MaximumAspectRatio = FMath::Clamp(
        FMath::Max(
            InSettings.MinimumAspectRatio,
            InSettings.MaximumAspectRatio
        ),
        MinimumAspectRatio,
        3.0f
    );
    const float OutlineIrregularity = FMath::Clamp(
        InSettings.OutlineIrregularity,
        0.0f,
        0.45f
    );
    const float ShoulderStrength = FMath::Clamp(
        InSettings.ShoulderStrength,
        0.0f,
        1.0f
    );
    const float GullyStrength = FMath::Clamp(
        InSettings.GullyStrength,
        0.0f,
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
            const float MaximumReach = FMath::Min(
                Radius * FMath::Sqrt(MaximumAspectRatio),
                CellSize * 0.48f
            ) * (1.0f + OutlineIrregularity);

            if (
                DeltaX * DeltaX + DeltaY * DeltaY >=
                MaximumReach * MaximumReach
            )
            {
                continue;
            }

            const float AspectRatio = FMath::Lerp(
                MinimumAspectRatio,
                MaximumAspectRatio,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0xA24BAED5u)
                )
            );
            const float AspectRoot = FMath::Sqrt(AspectRatio);
            const float MajorRadius = FMath::Min(
                Radius * AspectRoot,
                CellSize * 0.48f
            );
            const float MinorRadius = FMath::Max(
                2.0f,
                Radius / AspectRoot
            );
            const float Rotation = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0x9FB21C65u)
            ) * 2.0f * PI;
            float RotationSin = 0.0f;
            float RotationCos = 1.0f;
            FMath::SinCos(&RotationSin, &RotationCos, Rotation);
            const float LocalX =
                DeltaX * RotationCos + DeltaY * RotationSin;
            const float LocalY =
                -DeltaX * RotationSin + DeltaY * RotationCos;
            const float EllipseX = LocalX / MajorRadius;
            const float EllipseY = LocalY / MinorRadius;
            const float PolarAngle = FMath::Atan2(EllipseY, EllipseX);
            const float OutlinePhaseA = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0xB7E15163u)
            ) * 2.0f * PI;
            const float OutlinePhaseB = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0xC6EF3720u)
            ) * 2.0f * PI;
            const float OutlineWave =
                FMath::Sin(PolarAngle * 3.0f + OutlinePhaseA) * 0.52f +
                FMath::Sin(PolarAngle * 5.0f + OutlinePhaseB) * 0.31f +
                FMath::Sin(
                    PolarAngle * 7.0f + OutlinePhaseA - OutlinePhaseB
                ) * 0.17f;
            const float BoundaryScale = FMath::Max(
                0.62f,
                1.0f + OutlineWave * OutlineIrregularity
            );
            const float NormalizedDistance = FMath::Sqrt(
                EllipseX * EllipseX + EllipseY * EllipseY
            ) / BoundaryScale;

            if (NormalizedDistance >= 1.0f)
            {
                continue;
            }

            // The summit does not sit at the geometric centre. Decoupling it
            // from the footprint removes the even, conical falloff that made
            // the old landmark read as a haystack.
            const float SummitOffsetX = FMath::Lerp(
                -0.12f,
                0.18f,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0x165667B1u)
                )
            );
            const float SummitOffsetY = FMath::Lerp(
                -0.10f,
                0.10f,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0xD3A2646Cu)
                )
            );
            const float SummitX = EllipseX - SummitOffsetX;
            const float SummitY = EllipseY - SummitOffsetY;
            const float SummitDistance = FMath::Sqrt(
                SummitX * SummitX + SummitY * SummitY
            ) / BoundaryScale;
            const float CliffOuterEdge = FMath::Min(
                0.88f,
                PlateauFraction + 0.24f
            );
            const float Escarpment = 1.0f - SmoothStep(
                PlateauFraction,
                CliffOuterEdge,
                SummitDistance
            );
            const float Apron = 0.16f * (
                1.0f - SmoothStep(
                    FMath::Min(0.78f, PlateauFraction + 0.12f),
                    1.0f,
                    NormalizedDistance
                )
            );
            float SmoothInfluence = FMath::Max(Escarpment, Apron);

            // Two lower, offset rock masses break the single-peak silhouette
            // and form readable shoulders and saddles around the main cap.
            const float ShoulderSide = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0x27D4EB2Fu)
            ) >= 0.5f ? 1.0f : -1.0f;
            const float ShoulderAX = EllipseX - ShoulderSide * 0.48f;
            const float ShoulderAY = EllipseY - 0.18f;
            const float ShoulderADistance = FMath::Sqrt(
                ShoulderAX * ShoulderAX / (0.48f * 0.48f) +
                ShoulderAY * ShoulderAY / (0.58f * 0.58f)
            );
            const float ShoulderA = ShoulderStrength * 0.72f * (
                1.0f - SmoothStep(0.12f, 1.0f, ShoulderADistance)
            );
            const float ShoulderBX = EllipseX + ShoulderSide * 0.34f;
            const float ShoulderBY = EllipseY + 0.42f;
            const float ShoulderBDistance = FMath::Sqrt(
                ShoulderBX * ShoulderBX / (0.38f * 0.38f) +
                ShoulderBY * ShoulderBY / (0.42f * 0.42f)
            );
            const float ShoulderB = ShoulderStrength * 0.48f * (
                1.0f - SmoothStep(0.08f, 1.0f, ShoulderBDistance)
            );
            SmoothInfluence = FMath::Max(
                SmoothInfluence,
                FMath::Max(ShoulderA, ShoulderB)
            );

            // Seeded radial gullies cut only the sloped faces. The summit and
            // outer apron remain readable while the rim loses its uniformity.
            const float GullyAngleA = HashToUnitFloat(
                HashCell(CellX, CellY, InSettings.Seed, 0x85EBCA77u)
            ) * 2.0f * PI;
            const float GullyAngleB = GullyAngleA + FMath::Lerp(
                1.65f,
                2.75f,
                HashToUnitFloat(
                    HashCell(CellX, CellY, InSettings.Seed, 0xC2B2AE3Du)
                )
            );
            const auto SampleGully = [
                EllipseX,
                EllipseY,
                NormalizedDistance,
                PlateauFraction
            ](const float Direction, const float Width)
            {
                float DirectionSin = 0.0f;
                float DirectionCos = 1.0f;
                FMath::SinCos(
                    &DirectionSin,
                    &DirectionCos,
                    Direction
                );
                const float Along =
                    EllipseX * DirectionCos + EllipseY * DirectionSin;
                const float Across = FMath::Abs(
                    -EllipseX * DirectionSin + EllipseY * DirectionCos
                );
                const float Channel = 1.0f - SmoothStep(
                    Width,
                    Width * 2.4f,
                    Across
                );
                const float Outward = SmoothStep(
                    PlateauFraction * 0.35f,
                    PlateauFraction * 0.85f,
                    Along
                );
                const float SlopeBand = SmoothStep(
                    PlateauFraction * 0.72f,
                    PlateauFraction + 0.16f,
                    NormalizedDistance
                ) * (1.0f - SmoothStep(
                    0.90f,
                    1.02f,
                    NormalizedDistance
                ));
                return Channel * Outward * SlopeBand;
            };
            const float GullyMask = FMath::Max(
                SampleGully(GullyAngleA, 0.055f),
                SampleGully(GullyAngleB, 0.045f) * 0.78f
            );
            SmoothInfluence *= 1.0f - GullyMask * GullyStrength;
            SmoothInfluence = FMath::Clamp(SmoothInfluence, 0.0f, 1.0f);

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
                    HashCell(CellX, CellY, InSettings.Seed, 0x7F4A7C15u)
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
