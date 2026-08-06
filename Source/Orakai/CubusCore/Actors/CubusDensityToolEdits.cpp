#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"

namespace CubusDensityTools
{
    float BrushWeight(const FIntVector& Offset, const int32 Radius)
    {
        if (Radius <= 0)
        {
            return Offset.IsZero() ? 1.0f : 0.0f;
        }

        const float Distance = FVector(Offset).Size();
        if (Distance > static_cast<float>(Radius))
        {
            return 0.0f;
        }

        const float T = 1.0f - Distance / static_cast<float>(Radius + 1);
        return T * T * (3.0f - 2.0f * T);
    }

    void PersistDensityEdit(
        const UObject* Context,
        const FIntVector& WorldSample,
        const FCubusDensityEdit* Edit
    )
    {
        UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(Context);
        if (Persistence == nullptr)
        {
            return;
        }

        if (Edit == nullptr || FMath::IsNearlyZero(Edit->DensityDelta))
        {
            Persistence->ClearDensityEdit(WorldSample);
            return;
        }

        Persistence->RecordDensityEdit(
            WorldSample,
            Edit->DensityDelta,
            Edit->MaterialId
        );
    }
}

int32 ACubusBlockWorldActor::SmoothDensityEditsAtWorldSample(
    const FIntVector CentreWorldSample,
    const int32 BrushRadius,
    const float Strength
)
{
    const int32 SafeRadius = FMath::Max(0, BrushRadius);
    const float BlendStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
    if (BlendStrength <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    const FCubusDensityEditMap SourceEdits = DensityEdits;
    FCubusDensityEditMap PendingEdits;
    TSet<FIntVector> TouchedChunks;

    static const FIntVector Neighbours[] =
    {
        FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0), FIntVector(0, -1, 0),
        FIntVector(0, 0, 1), FIntVector(0, 0, -1)
    };

    for (int32 Z = -SafeRadius; Z <= SafeRadius; ++Z)
    {
        for (int32 Y = -SafeRadius; Y <= SafeRadius; ++Y)
        {
            for (int32 X = -SafeRadius; X <= SafeRadius; ++X)
            {
                const FIntVector Offset(X, Y, Z);
                const float Weight = CubusDensityTools::BrushWeight(Offset, SafeRadius);
                if (Weight <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                const FIntVector Sample = CentreWorldSample + Offset;
                const FCubusDensityEdit* Existing = SourceEdits.Find(Sample);
                const float CurrentDelta = Existing != nullptr ? Existing->DensityDelta : 0.0f;

                float Sum = CurrentDelta;
                int32 Count = 1;
                for (const FIntVector& Neighbour : Neighbours)
                {
                    if (const FCubusDensityEdit* Nearby = SourceEdits.Find(Sample + Neighbour))
                    {
                        Sum += Nearby->DensityDelta;
                    }
                    ++Count;
                }

                FCubusDensityEdit Result;
                Result.DensityDelta = FMath::Lerp(
                    CurrentDelta,
                    Sum / static_cast<float>(Count),
                    BlendStrength * Weight
                );
                Result.MaterialId = Existing != nullptr ? Existing->MaterialId : 0;
                PendingEdits.Add(Sample, Result);
            }
        }
    }

    int32 ChangedCount = 0;
    for (const TPair<FIntVector, FCubusDensityEdit>& Pair : PendingEdits)
    {
        if (FMath::IsNearlyZero(Pair.Value.DensityDelta))
        {
            DensityEdits.Remove(Pair.Key);
            CubusDensityTools::PersistDensityEdit(this, Pair.Key, nullptr);
        }
        else
        {
            DensityEdits.Add(Pair.Key, Pair.Value);
            CubusDensityTools::PersistDensityEdit(this, Pair.Key, &Pair.Value);
        }

        TouchedChunks.Add(OrakaiPersistence::WorldVoxelToChunk(Pair.Key));
        ++ChangedCount;
    }

    for (const FIntVector& Chunk : TouchedChunks)
    {
        QueueDensityEditDependenciesForRebuild(Chunk);
    }
    return ChangedCount;
}

int32 ACubusBlockWorldActor::LevelDensityEditsAtWorldSample(
    const FIntVector CentreWorldSample,
    const int32 BrushRadius,
    const float Strength,
    const int32 MaterialId
)
{
    const int32 SafeRadius = FMath::Max(0, BrushRadius);
    const float BlendStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
    if (BlendStrength <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    TSet<FIntVector> TouchedChunks;
    int32 ChangedCount = 0;

    for (int32 Z = -SafeRadius; Z <= SafeRadius; ++Z)
    {
        for (int32 Y = -SafeRadius; Y <= SafeRadius; ++Y)
        {
            for (int32 X = -SafeRadius; X <= SafeRadius; ++X)
            {
                const FIntVector Offset(X, Y, Z);
                const float Weight = CubusDensityTools::BrushWeight(Offset, SafeRadius);
                if (Weight <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                const FIntVector Sample = CentreWorldSample + Offset;
                FCubusDensityEdit& Edit = DensityEdits.FindOrAdd(Sample);
                const float TargetDelta = static_cast<float>(CentreWorldSample.Z - Sample.Z);
                Edit.DensityDelta = FMath::Lerp(
                    Edit.DensityDelta,
                    TargetDelta,
                    BlendStrength * Weight
                );

                if (Edit.DensityDelta > 0.0f && MaterialId > 0)
                {
                    Edit.MaterialId = MaterialId;
                }

                if (FMath::IsNearlyZero(Edit.DensityDelta))
                {
                    DensityEdits.Remove(Sample);
                    CubusDensityTools::PersistDensityEdit(this, Sample, nullptr);
                }
                else
                {
                    CubusDensityTools::PersistDensityEdit(this, Sample, &Edit);
                }

                TouchedChunks.Add(OrakaiPersistence::WorldVoxelToChunk(Sample));
                ++ChangedCount;
            }
        }
    }

    for (const FIntVector& Chunk : TouchedChunks)
    {
        QueueDensityEditDependenciesForRebuild(Chunk);
    }
    return ChangedCount;
}

int32 ACubusBlockWorldActor::RestoreDensityEditsAtWorldSample(
    const FIntVector CentreWorldSample,
    const int32 BrushRadius,
    const float Strength
)
{
    const int32 SafeRadius = FMath::Max(0, BrushRadius);
    const float BlendStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
    if (BlendStrength <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    TSet<FIntVector> TouchedChunks;
    int32 ChangedCount = 0;

    for (int32 Z = -SafeRadius; Z <= SafeRadius; ++Z)
    {
        for (int32 Y = -SafeRadius; Y <= SafeRadius; ++Y)
        {
            for (int32 X = -SafeRadius; X <= SafeRadius; ++X)
            {
                const FIntVector Offset(X, Y, Z);
                const float Weight = CubusDensityTools::BrushWeight(Offset, SafeRadius);
                if (Weight <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                const FIntVector Sample = CentreWorldSample + Offset;
                FCubusDensityEdit* Existing = DensityEdits.Find(Sample);
                if (Existing == nullptr)
                {
                    continue;
                }

                Existing->DensityDelta = FMath::Lerp(
                    Existing->DensityDelta,
                    0.0f,
                    BlendStrength * Weight
                );

                if (FMath::IsNearlyZero(Existing->DensityDelta, 0.001f))
                {
                    DensityEdits.Remove(Sample);
                    CubusDensityTools::PersistDensityEdit(this, Sample, nullptr);
                }
                else
                {
                    CubusDensityTools::PersistDensityEdit(this, Sample, Existing);
                }

                TouchedChunks.Add(OrakaiPersistence::WorldVoxelToChunk(Sample));
                ++ChangedCount;
            }
        }
    }

    for (const FIntVector& Chunk : TouchedChunks)
    {
        QueueDensityEditDependenciesForRebuild(Chunk);
    }
    return ChangedCount;
}
