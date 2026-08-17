#include "CubusCore/Generation/CubusTerrainDeposition.h"

FCubusTerrainRasterTile FCubusTerrainDeposition::DepositTile(
    const FCubusTerrainRasterTile& ErodedTile,
    const FCubusTerrainDepositionSettings& InSettings
)
{
    if (!ErodedTile.IsValid())
    {
        return ErodedTile;
    }

    FCubusTerrainDepositionSettings Settings = InSettings;
    Settings.Iterations = FMath::Clamp(Settings.Iterations, 1, FMath::Max(1, ErodedTile.HaloSamples - 1));
    Settings.MaximumDepositionSlopeDegrees = FMath::Clamp(Settings.MaximumDepositionSlopeDegrees, 1.0f, 35.0f);
    Settings.ConcavityFillStrength = FMath::Clamp(Settings.ConcavityFillStrength, 0.0f, 0.8f);
    Settings.SlopeBreakStrength = FMath::Clamp(Settings.SlopeBreakStrength, 0.0f, 0.6f);
    Settings.MaxDepositPerIterationMeters = FMath::Max(0.0f, Settings.MaxDepositPerIterationMeters);
    Settings.FloodplainDiffusion = FMath::Clamp(Settings.FloodplainDiffusion, 0.0f, 0.2f);

    FCubusTerrainRasterTile Result = ErodedTile;
    const int32 Size = Result.StorageSampleCount;
    const float CellSize = FMath::Max(0.25f, Result.SampleSpacingMeters);
    const float MaxRiseAtSlope = FMath::Tan(FMath::DegreesToRadians(Settings.MaximumDepositionSlopeDegrees)) * CellSize;

    TArray<float> Current = Result.HeightMeters;

    // Preserve the original halo in both buffers once. Each iteration expands
    // the processed interior as Margin shrinks, so every potentially stale cell
    // in the alternate buffer is overwritten before the next swap can expose it.
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
                const float UL = CurrentData[PreviousRow + X - 1];
                const float UR = CurrentData[PreviousRow + X + 1];
                const float DL = CurrentData[FollowingRow + X - 1];
                const float DR = CurrentData[FollowingRow + X + 1];

                const float Mean4 = (Left + Right + Up + Down) * 0.25f;
                const float Mean8 = (Left + Right + Up + Down + UL + UR + DL + DR) * 0.125f;

                const float Dx = (Right - Left) * 0.5f;
                const float Dy = (Down - Up) * 0.5f;
                const float LocalRise = FMath::Sqrt(Dx * Dx + Dy * Dy);
                const float SlopeAcceptance = 1.0f - FMath::Clamp(
                    LocalRise / FMath::Max(0.001f, MaxRiseAtSlope),
                    0.0f,
                    1.0f
                );

                // Positive where the sample sits below its surrounding terrain.
                const float Concavity = FMath::Max(0.0f, Mean8 - Centre);

                // A simple slope-break proxy: neighbouring cardinal slopes are
                // substantially steeper than the slope through this sample.
                const float NeighbourRelief = FMath::Max(
                    FMath::Max(FMath::Abs(Centre - Left), FMath::Abs(Centre - Right)),
                    FMath::Max(FMath::Abs(Centre - Up), FMath::Abs(Centre - Down))
                );
                const float SlopeBreak = FMath::Max(0.0f, NeighbourRelief - LocalRise);

                float Deposit = Concavity * Settings.ConcavityFillStrength * SlopeAcceptance;
                Deposit += SlopeBreak * Settings.SlopeBreakStrength * SlopeAcceptance;
                Deposit = FMath::Min(Settings.MaxDepositPerIterationMeters, Deposit);

                float NewHeight = Centre + Deposit;
                if (SlopeAcceptance > 0.2f)
                {
                    const float Diffusion = Settings.FloodplainDiffusion * SlopeAcceptance;
                    NewHeight = FMath::Lerp(NewHeight, Mean4, Diffusion);
                }

                // Never erase the broad relief by allowing this local process to
                // jump above its immediate four-neighbour envelope.
                const float LocalCeiling = FMath::Max(FMath::Max(Left, Right), FMath::Max(Up, Down));
                NextData[Index] = FMath::Min(NewHeight, LocalCeiling);
            }
        }

        Swap(Current, Next);
    }

    Result.HeightMeters = MoveTemp(Current);
    return Result;
}
