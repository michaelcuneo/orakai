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
            return Offset == FIntVector::ZeroValue ? 1.0f : 0.0f;
        }

        const FVector OffsetVector(
            static_cast<double>(Offset.X),
            static_cast<double>(Offset.Y),
            static_cast<double>(Offset.Z)
        );
        const float Distance = static_cast<float>(OffsetVector.Size());
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

    FCubusDensityEditMap SourceEdits;

    const int32 SourceRadius =
        SafeRadius + 1;

    for (
        int32 Z = -SourceRadius;
        Z <= SourceRadius;
        ++Z
    )
    {
        for (
            int32 Y = -SourceRadius;
            Y <= SourceRadius;
            ++Y
        )
        {
            for (
                int32 X = -SourceRadius;
                X <= SourceRadius;
                ++X
            )
            {
                const FIntVector Sample =
                    CentreWorldSample +
                    FIntVector(X, Y, Z);

                const FCubusDensityEdit* Edit =
                    DensityEdits.Find(
                        Sample
                    );

                if (Edit != nullptr)
                {
                    SourceEdits.Add(
                        Sample,
                        *Edit
                    );
                }
            }
        }
    }

    FCubusDensityEditMap PendingEdits;

    FIntVector ChangedSampleMinimum(
        MAX_int32,
        MAX_int32,
        MAX_int32
    );

    FIntVector ChangedSampleMaximum(
        MIN_int32,
        MIN_int32,
        MIN_int32
    );

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
        if (
            FMath::IsNearlyZero(
                Pair.Value.DensityDelta
            )
        )
        {
            DensityEdits.Remove(
                Pair.Key
            );

            ReindexDensityEditSample(
                Pair.Key
            );

            CubusDensityTools::PersistDensityEdit(
                this,
                Pair.Key,
                nullptr
            );
        }
        else
        {
            DensityEdits.Add(
                Pair.Key,
                Pair.Value
            );

            ReindexDensityEditSample(
                Pair.Key
            );

            CubusDensityTools::PersistDensityEdit(
                this,
                Pair.Key,
                &Pair.Value
            );
        }

        ChangedSampleMinimum.X =
            FMath::Min(
                ChangedSampleMinimum.X,
                Pair.Key.X
            );

        ChangedSampleMinimum.Y =
            FMath::Min(
                ChangedSampleMinimum.Y,
                Pair.Key.Y
            );

        ChangedSampleMinimum.Z =
            FMath::Min(
                ChangedSampleMinimum.Z,
                Pair.Key.Z
            );

        ChangedSampleMaximum.X =
            FMath::Max(
                ChangedSampleMaximum.X,
                Pair.Key.X
            );

        ChangedSampleMaximum.Y =
            FMath::Max(
                ChangedSampleMaximum.Y,
                Pair.Key.Y
            );

        ChangedSampleMaximum.Z =
            FMath::Max(
                ChangedSampleMaximum.Z,
                Pair.Key.Z
            );

        ++ChangedCount;
    }

    if (ChangedCount > 0)
    {
        QueueDensityEditDependenciesForRebuild(
            ChangedSampleMinimum,
            ChangedSampleMaximum
        );
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

    FIntVector ChangedSampleMinimum(
        MAX_int32,
        MAX_int32,
        MAX_int32
    );

    FIntVector ChangedSampleMaximum(
        MIN_int32,
        MIN_int32,
        MIN_int32
    );

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

                if (
                    FMath::IsNearlyZero(
                        Edit.DensityDelta
                    )
                )
                {
                    DensityEdits.Remove(
                        Sample
                    );

                    ReindexDensityEditSample(
                        Sample
                    );

                    CubusDensityTools::PersistDensityEdit(
                        this,
                        Sample,
                        nullptr
                    );
                }
                else
                {
                    ReindexDensityEditSample(
                        Sample
                    );

                    CubusDensityTools::PersistDensityEdit(
                        this,
                        Sample,
                        &Edit
                    );
                }

                ChangedSampleMinimum.X =
                    FMath::Min(
                        ChangedSampleMinimum.X,
                        Sample.X
                    );

                ChangedSampleMinimum.Y =
                    FMath::Min(
                        ChangedSampleMinimum.Y,
                        Sample.Y
                    );

                ChangedSampleMinimum.Z =
                    FMath::Min(
                        ChangedSampleMinimum.Z,
                        Sample.Z
                    );

                ChangedSampleMaximum.X =
                    FMath::Max(
                        ChangedSampleMaximum.X,
                        Sample.X
                    );

                ChangedSampleMaximum.Y =
                    FMath::Max(
                        ChangedSampleMaximum.Y,
                        Sample.Y
                    );

                ChangedSampleMaximum.Z =
                    FMath::Max(
                        ChangedSampleMaximum.Z,
                        Sample.Z
                    );

                ++ChangedCount;
            }
        }
    }

    if (ChangedCount > 0)
    {
        QueueDensityEditDependenciesForRebuild(
            ChangedSampleMinimum,
            ChangedSampleMaximum
        );
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

    FIntVector ChangedSampleMinimum(
        MAX_int32,
        MAX_int32,
        MAX_int32
    );

    FIntVector ChangedSampleMaximum(
        MIN_int32,
        MIN_int32,
        MIN_int32
    );

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

                if (
                    FMath::IsNearlyZero(
                        Existing->DensityDelta,
                        0.001f
                    )
                )
                {
                    DensityEdits.Remove(
                        Sample
                    );

                    ReindexDensityEditSample(
                        Sample
                    );

                    CubusDensityTools::PersistDensityEdit(
                        this,
                        Sample,
                        nullptr
                    );
                }
                else
                {
                    ReindexDensityEditSample(
                        Sample
                    );

                    CubusDensityTools::PersistDensityEdit(
                        this,
                        Sample,
                        Existing
                    );
                }
                
                ChangedSampleMinimum.X =
                    FMath::Min(
                        ChangedSampleMinimum.X,
                        Sample.X
                    );

                ChangedSampleMinimum.Y =
                    FMath::Min(
                        ChangedSampleMinimum.Y,
                        Sample.Y
                    );

                ChangedSampleMinimum.Z =
                    FMath::Min(
                        ChangedSampleMinimum.Z,
                        Sample.Z
                    );

                ChangedSampleMaximum.X =
                    FMath::Max(
                        ChangedSampleMaximum.X,
                        Sample.X
                    );

                ChangedSampleMaximum.Y =
                    FMath::Max(
                        ChangedSampleMaximum.Y,
                        Sample.Y
                    );

                ChangedSampleMaximum.Z =
                    FMath::Max(
                        ChangedSampleMaximum.Z,
                        Sample.Z
                    );

                ++ChangedCount;
            }
        }
    }

    if (ChangedCount > 0)
    {
        QueueDensityEditDependenciesForRebuild(
            ChangedSampleMinimum,
            ChangedSampleMaximum
        );
    }

    return ChangedCount;
}
