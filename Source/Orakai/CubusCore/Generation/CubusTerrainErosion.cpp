#include "CubusCore/Generation/CubusTerrainErosion.h"

FCubusTerrainRasterTile FCubusTerrainErosion::ErodeTile(
    const FCubusTerrainRasterTile& CarvedTile,
    const FCubusTerrainErosionSettings& InSettings
)
{
    if (!CarvedTile.IsValid())
    {
        return CarvedTile;
    }

    FCubusTerrainErosionSettings Settings = InSettings;
    Settings.Iterations = FMath::Clamp(Settings.Iterations, 1, FMath::Max(1, CarvedTile.HaloSamples - 1));
    Settings.TalusAngleDegrees = FMath::Clamp(Settings.TalusAngleDegrees, 5.0f, 60.0f);
    Settings.ThermalTransport = FMath::Clamp(Settings.ThermalTransport, 0.0f, 0.45f);
    Settings.ConcavityIncision = FMath::Clamp(Settings.ConcavityIncision, 0.0f, 1.0f);
    Settings.MaxIncisionPerIterationMeters = FMath::Max(0.0f, Settings.MaxIncisionPerIterationMeters);
    Settings.MinimumIncisionSlopeDegrees = FMath::Clamp(Settings.MinimumIncisionSlopeDegrees, 0.0f, 45.0f);
    Settings.HillslopeDiffusion = FMath::Clamp(Settings.HillslopeDiffusion, 0.0f, 0.2f);

    FCubusTerrainRasterTile Result = CarvedTile;
    const int32 Size = Result.StorageSampleCount;
    const float CellSize = FMath::Max(0.25f, Result.SampleSpacingMeters);
    const float TalusRise = FMath::Tan(FMath::DegreesToRadians(Settings.TalusAngleDegrees)) * CellSize;
    const float MinimumIncisionRise = FMath::Tan(FMath::DegreesToRadians(Settings.MinimumIncisionSlopeDegrees)) * CellSize;

    TArray<float> Current = Result.HeightMeters;

    // Both buffers start from the same source. Each successive iteration processes
    // a larger interior region as Margin shrinks, so every value that can be stale
    // in the alternate buffer is overwritten before it becomes observable. This
    // avoids copying the entire DEM once per iteration while preserving the halo.
    TArray<float> Next = Current;

    for (int32 Iteration = 0; Iteration < Settings.Iterations; ++Iteration)
    {
        const int32 Margin = Settings.Iterations - Iteration;
        const float* CurrentData = Current.GetData();
        float* NextData = Next.GetData();

        for (int32 Y = Margin; Y < Size - Margin; ++Y)
        {
            const int32 Row = Y * Size;
            const int32 PreviousRow = Row - Size;
            const int32 FollowingRow = Row + Size;

            for (int32 X = Margin; X < Size - Margin; ++X)
            {
                const int32 Index = Row + X;
                const float Centre = CurrentData[Index];
                const float Left = CurrentData[Index - 1];
                const float Right = CurrentData[Index + 1];
                const float Up = CurrentData[PreviousRow + X];
                const float Down = CurrentData[FollowingRow + X];

                const float MinimumNeighbour = FMath::Min(FMath::Min(Left, Right), FMath::Min(Up, Down));
                const float MaximumNeighbour = FMath::Max(FMath::Max(Left, Right), FMath::Max(Up, Down));
                const float MeanNeighbour = (Left + Right + Up + Down) * 0.25f;

                float NewHeight = Centre;

                const float ExcessTalus = Centre - MinimumNeighbour - TalusRise;
                if (ExcessTalus > 0.0f)
                {
                    NewHeight -= ExcessTalus * Settings.ThermalTransport;
                }

                const float LocalSlope = FMath::Max(
                    FMath::Max(FMath::Abs(Centre - Left), FMath::Abs(Centre - Right)),
                    FMath::Max(FMath::Abs(Centre - Up), FMath::Abs(Centre - Down))
                );

                const float Concavity = FMath::Max(0.0f, MeanNeighbour - Centre);
                if (LocalSlope > MinimumIncisionRise && Concavity > 0.0f)
                {
                    const float Incision = FMath::Min(
                        Settings.MaxIncisionPerIterationMeters,
                        Concavity * Settings.ConcavityIncision
                    );
                    NewHeight -= Incision;
                }

                if (Centre > MeanNeighbour || MaximumNeighbour - MinimumNeighbour > TalusRise * 0.5f)
                {
                    NewHeight = FMath::Lerp(NewHeight, MeanNeighbour, Settings.HillslopeDiffusion);
                }

                NextData[Index] = NewHeight;
            }
        }

        Swap(Current, Next);
    }

    Result.HeightMeters = MoveTemp(Current);
    return Result;
}
