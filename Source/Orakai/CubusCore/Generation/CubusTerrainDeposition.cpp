#include "CubusCore/Generation/CubusTerrainDeposition.h"

namespace CubusTerrainDeposition
{
    float HeightAt(const TArray<float>& Heights, const int32 Size, const int32 X, const int32 Y)
    {
        const int32 SafeX = FMath::Clamp(X, 0, Size - 1);
        const int32 SafeY = FMath::Clamp(Y, 0, Size - 1);
        return Heights[SafeY * Size + SafeX];
    }
}

FCubusTerrainRasterTile FCubusTerrainDeposition::DepositTile(
    const FCubusTerrainRasterTile& ErodedTile,
    const FCubusTerrainDepositionSettings& InSettings
)
{
    using namespace CubusTerrainDeposition;

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
    TArray<float> Next;
    Next.SetNumUninitialized(Current.Num());

    for (int32 Iteration = 0; Iteration < Settings.Iterations; ++Iteration)
    {
        Next = Current;
        const int32 Margin = Settings.Iterations - Iteration;

        for (int32 Y = Margin; Y < Size - Margin; ++Y)
        {
            for (int32 X = Margin; X < Size - Margin; ++X)
            {
                const int32 Index = Y * Size + X;
                const float Centre = Current[Index];
                const float Left = HeightAt(Current, Size, X - 1, Y);
                const float Right = HeightAt(Current, Size, X + 1, Y);
                const float Up = HeightAt(Current, Size, X, Y - 1);
                const float Down = HeightAt(Current, Size, X, Y + 1);
                const float UL = HeightAt(Current, Size, X - 1, Y - 1);
                const float UR = HeightAt(Current, Size, X + 1, Y - 1);
                const float DL = HeightAt(Current, Size, X - 1, Y + 1);
                const float DR = HeightAt(Current, Size, X + 1, Y + 1);

                const float Mean4 = (Left + Right + Up + Down) * 0.25f;
                const float Mean8 = (Left + Right + Up + Down + UL + UR + DL + DR) * 0.125f;

                const float Dx = (Right - Left) * 0.5f;
                const float Dy = (Down - Up) * 0.5f;
                const float LocalRise = FMath::Sqrt(Dx * Dx + Dy * Dy);
                const float SlopeAcceptance = 1.0f - FMath::Clamp(LocalRise / FMath::Max(0.001f, MaxRiseAtSlope), 0.0f, 1.0f);

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
                Next[Index] = FMath::Min(NewHeight, LocalCeiling);
            }
        }

        Swap(Current, Next);
    }

    Result.HeightMeters = MoveTemp(Current);
    return Result;
}
