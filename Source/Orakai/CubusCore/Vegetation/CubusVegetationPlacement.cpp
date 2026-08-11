#include "CubusCore/Vegetation/CubusVegetationPlacement.h"

#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsGroundDetailType(const int32 TypeId)
{
    return
        TypeId == CubusVegetationType::Grass ||
        TypeId == CubusVegetationType::StoneClutter ||
        TypeId == CubusVegetationType::OrganicClutter;
}

float ResolveGroundDetailEndDistance()
{
    constexpr float DefaultGroundDetailEndDistance = 16000.0f;

    IConsoleVariable* Variable =
        IConsoleManager::Get().FindConsoleVariable(
            TEXT("cubus.Vegetation.GroundDetailEndDistance")
        );

    if (Variable == nullptr)
    {
        return DefaultGroundDetailEndDistance;
    }

    const int32 ConfiguredDistance =
        Variable->GetInt();

    return
        ConfiguredDistance > 0
            ? static_cast<float>(ConfiguredDistance)
            : 0.0f;
}
}

void FCubusVegetationPlacement::Reset()
{
    SamplesByPlant.Reset();

    bStreamInitialized = false;
    SeedSnapshot = 0;
    SettingsHashSnapshot = 0;
}

uint32 FCubusVegetationPlacement::
CalculateRandomizationSettingsHash(
    const FCubusVegetationRandomizationSettings& Settings
) const
{
    uint32 Hash = GetTypeHash(Settings.bEnabled);

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(Settings.Seed)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(Settings.PruneProbability)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(Settings.ScaleJitterMin)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(Settings.ScaleJitterMax)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(
            Settings.PositionJitterVoxelFraction
        )
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(Settings.YawJitterDegrees)
    );

    return Hash;
}

void FCubusVegetationPlacement::EnsureRandomizationStream(
    const FCubusVegetationRandomizationSettings& Settings
)
{
    if (!Settings.bEnabled)
    {
        return;
    }

    const uint32 CurrentSettingsHash =
        CalculateRandomizationSettingsHash(Settings);

    if (
        bStreamInitialized &&
        SeedSnapshot == Settings.Seed &&
        SettingsHashSnapshot == CurrentSettingsHash
    )
    {
        return;
    }

    constexpr uint32 VegetationRandomizationSalt =
        0x7A31C4D9u;

    const uint32 StreamSeed = HashCombineFast(
        GetTypeHash(Settings.Seed),
        VegetationRandomizationSalt
    );

    RandomStream.Initialize(
        static_cast<int32>(StreamSeed)
    );

    SamplesByPlant.Reset();

    SeedSnapshot = Settings.Seed;
    SettingsHashSnapshot = CurrentSettingsHash;
    bStreamInitialized = true;
}

FCubusResolvedVegetationPlacement
FCubusVegetationPlacement::Resolve(
    const FCubusVegetationInstance& Instance,
    const FVector& BaseWorldLocation,
    const float VoxelSize,
    const float BaseScale,
    const FCubusVegetationRandomizationSettings& Settings
)
{
    FCubusResolvedVegetationPlacement Result;

    Result.Location = BaseWorldLocation;
    Result.Scale = FMath::Max(0.01f, BaseScale);
    Result.Yaw = Instance.RotationYaw;

    /*
     * Grass and terrain scatter are local ground detail, not world-scale
     * vegetation. Keep them out of the tree/shrub residency path entirely.
     *
     * The world vegetation actor may retain placement records for every
     * streamed terrain chunk, but ground-detail transforms are rejected here
     * before they reach an ISM batch when they lie outside the dedicated local
     * range. This remains active even when runtime randomization is disabled.
     *
     * Decals can use this same ground-detail range when a decal placement type
     * is introduced; the current generated vegetation type set contains no
     * decal type yet.
     */
    if (IsGroundDetailType(Instance.TypeId))
    {
        const float GroundDetailEndDistance =
            ResolveGroundDetailEndDistance();

        if (GroundDetailEndDistance > 0.0f)
        {
            UWorld* World = GWorld;

            if (IsValid(World))
            {
                const APawn* PlayerPawn =
                    UGameplayStatics::GetPlayerPawn(
                        World,
                        0
                    );

                if (IsValid(PlayerPawn))
                {
                    const FVector PlayerLocation =
                        PlayerPawn->GetActorLocation();

                    const FVector2D DetailLocation(
                        BaseWorldLocation.X,
                        BaseWorldLocation.Y
                    );

                    const FVector2D PlayerLocation2D(
                        PlayerLocation.X,
                        PlayerLocation.Y
                    );

                    if (
                        FVector2D::DistSquared(
                            DetailLocation,
                            PlayerLocation2D
                        ) >
                        FMath::Square(
                            GroundDetailEndDistance
                        )
                    )
                    {
                        Result.bPruned = true;
                        return Result;
                    }
                }
            }
        }
    }

    if (!Settings.bEnabled)
    {
        return Result;
    }

    EnsureRandomizationStream(Settings);

    const uint32 RandomKeyA = HashCombineFast(
        GetTypeHash(Instance.WorldVoxel),
        GetTypeHash(Instance.TypeId)
    );

    const uint32 RandomKeyB = HashCombineFast(
        GetTypeHash(Instance.RotationYaw),
        GetTypeHash(Instance.Scale)
    );

    const uint64 PlantRandomKey =
        (
            static_cast<uint64>(RandomKeyA) << 32
        ) |
        static_cast<uint64>(RandomKeyB);

    FRandomizationSample* Sample =
        SamplesByPlant.Find(PlantRandomKey);

    if (Sample == nullptr)
    {
        FRandomizationSample NewSample;

        const float JitterMin = FMath::Max(
            0.01f,
            FMath::Min(
                Settings.ScaleJitterMin,
                Settings.ScaleJitterMax
            )
        );

        const float JitterMax = FMath::Max(
            0.01f,
            FMath::Max(
                Settings.ScaleJitterMin,
                Settings.ScaleJitterMax
            )
        );

        NewSample.bPruned =
            RandomStream.FRand() <
            FMath::Clamp(
                Settings.PruneProbability,
                0.0f,
                1.0f
            );

        NewSample.ScaleMultiplier =
            RandomStream.FRandRange(
                JitterMin,
                JitterMax
            );

        NewSample.PositionJitterUnit =
            FVector2f(
                RandomStream.FRandRange(-1.0f, 1.0f),
                RandomStream.FRandRange(-1.0f, 1.0f)
            );

        NewSample.YawJitterUnit =
            RandomStream.FRandRange(-1.0f, 1.0f);

        SamplesByPlant.Add(
            PlantRandomKey,
            NewSample
        );

        Sample =
            SamplesByPlant.Find(PlantRandomKey);
    }

    if (Sample == nullptr)
    {
        return Result;
    }

    Result.bRandomized = true;
    Result.bPruned = Sample->bPruned;
    Result.AppliedRandomScale =
        Sample->ScaleMultiplier;

    if (Result.bPruned)
    {
        return Result;
    }

    Result.Scale = FMath::Max(
        0.01f,
        Result.Scale * Sample->ScaleMultiplier
    );

    const float JitterFraction = FMath::Clamp(
        Settings.PositionJitterVoxelFraction,
        0.0f,
        0.49f
    );

    if (JitterFraction > 0.0f)
    {
        const float JitterExtent =
            FMath::Max(1.0f, VoxelSize) *
            JitterFraction;

        Result.Location.X +=
            static_cast<float>(
                Sample->PositionJitterUnit.X
            ) *
            JitterExtent;

        Result.Location.Y +=
            static_cast<float>(
                Sample->PositionJitterUnit.Y
            ) *
            JitterExtent;
    }

    Result.Yaw +=
        Sample->YawJitterUnit *
        FMath::Clamp(
            Settings.YawJitterDegrees,
            0.0f,
            180.0f
        );

    return Result;
}
