#include "CubusCore/Generation/CubusLandscapeEvolution.h"

#include "HAL/PlatformTime.h"

namespace CubusLandscapeEvolution
{
namespace
{
float ReceiverDistanceMeters(const FGlobalDem& Dem, const int32 Cell, const int32 Receiver)
{
	const FIntPoint A = Dem.Coordinates(Cell);
	const FIntPoint B = Dem.Coordinates(Receiver);
	return static_cast<float>(Dem.CellSizeMeters * ((A.X != B.X && A.Y != B.Y) ? UE_SQRT_2 : 1.0));
}

float ProvinceAgeMultiplier(const FGlobalDem& Dem, const int32 Cell)
{
	if (!Dem.ProvinceId.IsValidIndex(Cell))
	{
		return 1.0f;
	}
	const EProvinceType Province = static_cast<EProvinceType>(Dem.ProvinceId[Cell]);
	const FProvinceParameters Parameters = FGenerator::GetDefaultProvinceParameters(Province);
	return FMath::Lerp(0.75f, 1.35f, Parameters.GeologicalAge01);
}

float NormalizedUpliftRate(const FSettings& Settings, const FGlobalDem& Dem, const int32 Cell)
{
	if (!Dem.UpliftM.IsValidIndex(Cell))
	{
		return 0.0f;
	}
	const float Reference = FMath::Max(1.0f, Settings.ConvergentUpliftM);
	const float SignedStrength = FMath::Clamp(Dem.UpliftM[Cell] / Reference, -1.0f, 1.5f);
	return Settings.BaseUpliftRateMPerYear * SignedStrength;
}

void UpdateElevationStats(const FGlobalDem& Dem, FGenerationStats* Stats)
{
	if (!Stats || Dem.ElevationM.IsEmpty())
	{
		return;
	}
	float Minimum = TNumericLimits<float>::Max();
	float Maximum = TNumericLimits<float>::Lowest();
	for (const float Elevation : Dem.ElevationM)
	{
		Minimum = FMath::Min(Minimum, Elevation);
		Maximum = FMath::Max(Maximum, Elevation);
	}
	Stats->MinimumElevationM = Minimum;
	Stats->MaximumElevationM = Maximum;
}
}

bool FGenerator::EvolveLandscape(const FSettings& Settings, FGlobalDem& InOutDem, FGenerationStats* OutStats, FString* OutError)
{
	if (!InOutDem.IsValid())
	{
		if (OutError)
		{
			*OutError = TEXT("GenerateSkeleton must succeed before EvolveLandscape.");
		}
		return false;
	}
	if (Settings.EvolutionIterations <= 0 || Settings.EvolutionStepYears <= 0.0f)
	{
		if (OutError)
		{
			*OutError = TEXT("Landscape evolution iterations and timestep must be positive.");
		}
		return false;
	}

	const double Start = FPlatformTime::Seconds();
	const int32 N = InOutDem.NumCells();
	const TArray<float> StartingElevation = InOutDem.ElevationM;
	InOutDem.StreamIncisionM.Init(0.0f, N);
	InOutDem.EvolutionDeltaM.Init(0.0f, N);
	TArray<float> BeforeIncision;
	TArray<float> Diffused;
	BeforeIncision.SetNumUninitialized(N);
	Diffused.SetNumUninitialized(N);

	double TotalIncision = 0.0;
	float MaximumIncision = 0.0f;
	int64 IncisionSamples = 0;

	for (int32 Iteration = 0; Iteration < Settings.EvolutionIterations; ++Iteration)
	{
		const bool bRefreshHydrology = !InOutDem.HasHydrology() || Iteration == 0 ||
			(Settings.HydrologyRefreshInterval > 0 && (Iteration % Settings.HydrologyRefreshInterval) == 0);
		if (bRefreshHydrology)
		{
			FGenerationStats HydrologyStats;
			if (!SolveHydrology(Settings, InOutDem, &HydrologyStats, OutError))
			{
				return false;
			}
			if (OutStats)
			{
				OutStats->BasinCount = HydrologyStats.BasinCount;
				OutStats->RiverCellCount = HydrologyStats.RiverCellCount;
				OutStats->MaximumDrainageAreaKm2 = HydrologyStats.MaximumDrainageAreaKm2;
			}
		}

		BeforeIncision = InOutDem.ElevationM;

		TArray<int32> DownstreamFirst;
		DownstreamFirst.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			DownstreamFirst[I] = I;
		}
		DownstreamFirst.Sort([&InOutDem](const int32 A, const int32 B)
		{
			return InOutDem.HydrologyElevationM[A] < InOutDem.HydrologyElevationM[B];
		});

		for (const int32 Cell : DownstreamFirst)
		{
			const FIntPoint Coordinates = InOutDem.Coordinates(Cell);
			if (InOutDem.IsBoundaryCell(Coordinates.X, Coordinates.Y) || InOutDem.ElevationM[Cell] <= Settings.OceanLevelM)
			{
				continue;
			}

			const int32 Receiver = InOutDem.Receiver.IsValidIndex(Cell) ? InOutDem.Receiver[Cell] : INDEX_NONE;
			const float UpliftRate = NormalizedUpliftRate(Settings, InOutDem, Cell);
			const float TectonicElevation = InOutDem.ElevationM[Cell] + UpliftRate * Settings.EvolutionStepYears;
			if (Receiver == INDEX_NONE)
			{
				InOutDem.ElevationM[Cell] = TectonicElevation;
				continue;
			}

			const float DistanceM = FMath::Max(1.0f, ReceiverDistanceMeters(InOutDem, Cell, Receiver));
			const double AreaM2 = FMath::Max(1.0, static_cast<double>(InOutDem.DrainageAreaKm2[Cell]) * 1000000.0);
			const EProvinceType Province = InOutDem.ProvinceId.IsValidIndex(Cell)
				? static_cast<EProvinceType>(InOutDem.ProvinceId[Cell])
				: EProvinceType::StablePlain;
			const FProvinceParameters ProvinceParameters = GetDefaultProvinceParameters(Province);
			const double K = static_cast<double>(Settings.StreamPowerK) * ProvinceParameters.Erodibility * ProvinceAgeMultiplier(InOutDem, Cell);
			const double Alpha = FMath::Max(0.0,
				K * FMath::Pow(AreaM2, static_cast<double>(Settings.StreamPowerM)) * Settings.EvolutionStepYears / DistanceM);
			const float DownstreamElevation = InOutDem.ElevationM[Receiver];
			const float ErodedElevation = static_cast<float>((TectonicElevation + Alpha * DownstreamElevation) / (1.0 + Alpha));
			const float NewElevation = FMath::Max(DownstreamElevation + Settings.PriorityFloodEpsilonM, ErodedElevation);
			const float Incision = FMath::Max(0.0f, TectonicElevation - NewElevation);
			InOutDem.ElevationM[Cell] = NewElevation;
			InOutDem.StreamIncisionM[Cell] += Incision;
			MaximumIncision = FMath::Max(MaximumIncision, Incision);
			TotalIncision += Incision;
			++IncisionSamples;
		}

		Diffused = InOutDem.ElevationM;
		const double Dx = FMath::Max(1.0, InOutDem.CellSizeMeters);
		const double Dx2 = Dx * Dx;
		for (int32 Y = 1; Y < InOutDem.Resolution - 1; ++Y)
		{
			for (int32 X = 1; X < InOutDem.Resolution - 1; ++X)
			{
				const int32 Cell = InOutDem.Index(X, Y);
				if (InOutDem.ElevationM[Cell] <= Settings.OceanLevelM)
				{
					continue;
				}
				const EProvinceType Province = InOutDem.ProvinceId.IsValidIndex(Cell)
					? static_cast<EProvinceType>(InOutDem.ProvinceId[Cell])
					: EProvinceType::StablePlain;
				const FProvinceParameters ProvinceParameters = GetDefaultProvinceParameters(Province);
				const float Center = InOutDem.ElevationM[Cell];
				const float Ex = InOutDem.ElevationM[InOutDem.Index(X + 1, Y)];
				const float Wx = InOutDem.ElevationM[InOutDem.Index(X - 1, Y)];
				const float Ny = InOutDem.ElevationM[InOutDem.Index(X, Y + 1)];
				const float Sy = InOutDem.ElevationM[InOutDem.Index(X, Y - 1)];
				const float GradX = static_cast<float>((Ex - Wx) / (2.0 * Dx));
				const float GradY = static_cast<float>((Ny - Sy) / (2.0 * Dx));
				const float Slope = FMath::Sqrt(GradX * GradX + GradY * GradY);
				const float Critical = FMath::Max(0.05f, ProvinceParameters.CriticalSlope);
				const float Ratio = FMath::Clamp(Slope / Critical, 0.0f, 0.98f);
				const float NonlinearBoost = FMath::Clamp(1.0f / FMath::Max(0.05f, 1.0f - Ratio * Ratio), 1.0f, Settings.MaxNonlinearDiffusionBoost);
				const float ChannelMultiplier = InOutDem.RiverMask.IsValidIndex(Cell) && InOutDem.RiverMask[Cell] != 0
					? Settings.ChannelDiffusionMultiplier
					: 1.0f;
				const double D = Settings.HillslopeDiffusivityM2PerYear * ProvinceParameters.HillslopeDiffusivity * NonlinearBoost * ChannelMultiplier;
				const double StableDt = D > SMALL_NUMBER ? 0.24 * Dx2 / D : Settings.EvolutionStepYears;
				const double Dt = FMath::Min<double>(Settings.EvolutionStepYears, StableDt);
				const double Laplacian = (Ex + Wx + Ny + Sy - 4.0 * Center) / Dx2;
				Diffused[Cell] = Center + static_cast<float>(D * Dt * Laplacian);
			}
		}
		InOutDem.ElevationM = MoveTemp(Diffused);
		Diffused.SetNumUninitialized(N);
	}

	FGenerationStats FinalHydrologyStats;
	if (!SolveHydrology(Settings, InOutDem, &FinalHydrologyStats, OutError))
	{
		return false;
	}

	double TotalAbsoluteChange = 0.0;
	float MaximumAbsoluteChange = 0.0f;
	float MaximumLowering = 0.0f;
	float MaximumRaising = 0.0f;
	for (int32 Cell = 0; Cell < N; ++Cell)
	{
		const float Delta = InOutDem.ElevationM[Cell] - StartingElevation[Cell];
		InOutDem.EvolutionDeltaM[Cell] = Delta;
		const float Absolute = FMath::Abs(Delta);
		MaximumAbsoluteChange = FMath::Max(MaximumAbsoluteChange, Absolute);
		MaximumLowering = FMath::Max(MaximumLowering, -Delta);
		MaximumRaising = FMath::Max(MaximumRaising, Delta);
		TotalAbsoluteChange += Absolute;
	}

	if (OutStats)
	{
		OutStats->CellCount = N;
		OutStats->EvolutionIterations = Settings.EvolutionIterations;
		OutStats->EvolutionSeconds = FPlatformTime::Seconds() - Start;
		OutStats->MaximumStreamIncisionM = MaximumIncision;
		OutStats->MeanStreamIncisionM = IncisionSamples > 0 ? static_cast<float>(TotalIncision / static_cast<double>(IncisionSamples)) : 0.0f;
		OutStats->MaximumAbsoluteElevationChangeM = MaximumAbsoluteChange;
		OutStats->MeanAbsoluteElevationChangeM = N > 0 ? static_cast<float>(TotalAbsoluteChange / static_cast<double>(N)) : 0.0f;
		OutStats->MaximumTerrainLoweringM = MaximumLowering;
		OutStats->MaximumTerrainRaisingM = MaximumRaising;
		OutStats->BasinCount = FinalHydrologyStats.BasinCount;
		OutStats->RiverCellCount = FinalHydrologyStats.RiverCellCount;
		OutStats->MaximumDrainageAreaKm2 = FinalHydrologyStats.MaximumDrainageAreaKm2;
		UpdateElevationStats(InOutDem, OutStats);
	}
	return true;
}
} // namespace CubusLandscapeEvolution
