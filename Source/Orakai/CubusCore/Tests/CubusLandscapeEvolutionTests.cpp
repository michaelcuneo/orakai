#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusLandscapeEvolution.h"

using namespace CubusLandscapeEvolution;

namespace
{
FString DescribeMaximumStep(const FGlobalDem& Dem)
{
	float MaxStep = 0.0f;
	int32 MaxA	  = 0;
	int32 MaxB	  = 0;
	for (int32 Y = 0; Y < Dem.Resolution; ++Y)
	{
		for (int32 X = 0; X < Dem.Resolution; ++X)
		{
			const int32 Cell = Dem.Index(X, Y);
			for (const int32 Neighbour :
				 {X + 1 < Dem.Resolution ? Cell + 1 : INDEX_NONE, Y + 1 < Dem.Resolution ? Cell + Dem.Resolution : INDEX_NONE})
			{
				if (Neighbour != INDEX_NONE && FMath::Abs(Dem.ElevationM[Cell] - Dem.ElevationM[Neighbour]) > MaxStep)
				{
					MaxStep = FMath::Abs(Dem.ElevationM[Cell] - Dem.ElevationM[Neighbour]);
					MaxA	= Cell;
					MaxB	= Neighbour;
				}
			}
		}
	}
	return FString::Printf(TEXT("Max step %.2f m: cells %d/%d, elevation %.2f/%.2f, uplift %.2f/%.2f, plates %d/%d"), MaxStep, MaxA, MaxB,
						   Dem.ElevationM[MaxA], Dem.ElevationM[MaxB], Dem.UpliftM[MaxA], Dem.UpliftM[MaxB], Dem.PlateId[MaxA],
						   Dem.PlateId[MaxB]);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubusLandscapeEvolutionDeterminismTest, "Orakai.Cubus.LandscapeEvolution.DeterministicSkeleton",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionDeterminismTest::RunTest(const FString& Parameters)
{
	(void) Parameters;
	FSettings Settings;
	Settings.Seed			 = 918273;
	Settings.Resolution		 = 65;
	Settings.WorldSizeMeters = 500000.0;
	Settings.PlateCount		 = 12;

	FGlobalDem A;
	FGlobalDem B;
	FString	   Error;
	TestTrue(TEXT("First skeleton generates"), FGenerator::GenerateSkeleton(Settings, A, nullptr, &Error));
	TestTrue(TEXT("Second skeleton generates"), FGenerator::GenerateSkeleton(Settings, B, nullptr, &Error));
	if (!A.IsValid() || !B.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Cell counts match"), A.ElevationM.Num(), B.ElevationM.Num());
	for (int32 I = 0; I < A.ElevationM.Num(); I += 97)
	{
		TestEqual(TEXT("Elevation is deterministic"), A.ElevationM[I], B.ElevationM[I]);
		TestEqual(TEXT("Plate ownership is deterministic"), A.PlateId[I], B.PlateId[I]);
		TestEqual(TEXT("Province ownership is deterministic"), A.ProvinceId[I], B.ProvinceId[I]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubusLandscapeEvolutionWorldCompositionTest, "Orakai.Cubus.LandscapeEvolution.WorldComposition",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionWorldCompositionTest::RunTest(const FString& Parameters)
{
	(void) Parameters;
	for (const int32 Seed : {1337, 44771, 918273})
	{
		FSettings Settings;
		Settings.Seed			 = Seed;
		Settings.Resolution		 = 129;
		Settings.WorldSizeMeters = 500000.0;
		Settings.PlateCount		 = 18;

		FGlobalDem		 Dem;
		FGenerationStats Stats;
		FString			 Error;
		if (!TestTrue(TEXT("Composition skeleton generates"), FGenerator::GenerateSkeleton(Settings, Dem, &Stats, &Error)))
		{
			return false;
		}

		int32 OceanicPlateCount = 0;
		for (const FPlate& Plate : Dem.Plates)
		{
			OceanicPlateCount += Plate.bOceanic ? 1 : 0;
		}
		TestTrue(TEXT("World contains oceanic crust"), OceanicPlateCount > 0);
		TestTrue(TEXT("World contains continental crust"), OceanicPlateCount < Dem.Plates.Num());
		TestTrue(TEXT("World retains substantial land"), Stats.LandFraction > 0.12f);
		TestTrue(TEXT("World contains substantial ocean"), Stats.LandFraction < 0.88f);
		TestTrue(TEXT("Macro skeleton does not cover most land in extreme slopes"), Stats.SteepLandFraction < 0.35f);
		for (int32 Edge = 0; Edge < Dem.Resolution; ++Edge)
		{
			TestEqual(TEXT("North edge reaches the configured ocean floor"), Dem.ElevationM[Dem.Index(Edge, 0)], Settings.OceanFloorM);
			TestEqual(TEXT("South edge reaches the configured ocean floor"), Dem.ElevationM[Dem.Index(Edge, Dem.Resolution - 1)],
					  Settings.OceanFloorM);
			TestEqual(TEXT("West edge reaches the configured ocean floor"), Dem.ElevationM[Dem.Index(0, Edge)], Settings.OceanFloorM);
			TestEqual(TEXT("East edge reaches the configured ocean floor"), Dem.ElevationM[Dem.Index(Dem.Resolution - 1, Edge)],
					  Settings.OceanFloorM);
		}
		if (Stats.MaximumNeighbourStepM >= 1600.0f)
		{
			AddInfo(DescribeMaximumStep(Dem));
		}
		TestTrue(*FString::Printf(TEXT("Macro skeleton maximum neighbour step is %.2f m"), Stats.MaximumNeighbourStepM),
				 Stats.MaximumNeighbourStepM < 1600.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubusLandscapeEvolutionGlobalSpacingTest, "Orakai.Cubus.LandscapeEvolution.Global4097Spacing",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionGlobalSpacingTest::RunTest(const FString& Parameters)
{
	(void) Parameters;
	FGlobalDem Dem;
	Dem.Resolution		= 4097;
	Dem.WorldSizeMeters = 500000.0;
	Dem.CellSizeMeters	= Dem.WorldSizeMeters / static_cast<double>(Dem.Resolution - 1);
	TestTrue(TEXT("500 km / 4096 intervals is approximately 122.0703125 m"),
			 FMath::IsNearlyEqual(Dem.CellSizeMeters, 122.0703125, 0.000001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubusLandscapeEvolutionHydrologyTest, "Orakai.Cubus.LandscapeEvolution.HydrologyMonotonicity",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionHydrologyTest::RunTest(const FString& Parameters)
{
	(void) Parameters;
	FSettings Settings;
	Settings.Seed				= 44771;
	Settings.Resolution			= 65;
	Settings.WorldSizeMeters	= 64000.0;
	Settings.PlateCount			= 10;
	Settings.RiverSourceAreaKm2 = 4.0f;

	FGlobalDem		 Dem;
	FGenerationStats Stats;
	FString			 Error;
	if (!TestTrue(TEXT("Skeleton generates"), FGenerator::GenerateSkeleton(Settings, Dem, &Stats, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Hydrology solves"), FGenerator::SolveHydrology(Settings, Dem, &Stats, &Error)))
	{
		return false;
	}

	TestTrue(TEXT("Hydrology buffers exist"), Dem.HasHydrology());
	TestTrue(TEXT("At least one basin exists"), Stats.BasinCount > 0);
	TestEqual(TEXT("Flow order contains every cell"), Dem.FlowOrder.Num(), Dem.NumCells());

	TArray<int32> OrderPosition;
	OrderPosition.SetNumUninitialized(Dem.NumCells());
	for (int32 I = 0; I < Dem.FlowOrder.Num(); ++I)
	{
		OrderPosition[Dem.FlowOrder[I]] = I;
	}

	const float CellAreaKm2 = static_cast<float>((Dem.CellSizeMeters * Dem.CellSizeMeters) / 1000000.0);
	for (int32 Cell = 0; Cell < Dem.NumCells(); ++Cell)
	{
		TestTrue(TEXT("Accumulation contains at least the local cell area"), Dem.DrainageAreaKm2[Cell] + KINDA_SMALL_NUMBER >= CellAreaKm2);
		const int32 Receiver = Dem.Receiver[Cell];
		if (Receiver != INDEX_NONE)
		{
			TestTrue(TEXT("Receiver is hydrologically downhill"), Dem.HydrologyElevationM[Receiver] < Dem.HydrologyElevationM[Cell]);
			TestTrue(TEXT("Donor appears before receiver in flow order"), OrderPosition[Cell] < OrderPosition[Receiver]);
			TestTrue(TEXT("Receiver drainage area includes donor area"),
					 Dem.DrainageAreaKm2[Receiver] + KINDA_SMALL_NUMBER >= Dem.DrainageAreaKm2[Cell]);
			TestEqual(TEXT("Donor and receiver share basin"), Dem.BasinId[Cell], Dem.BasinId[Receiver]);
			TestTrue(TEXT("Distance to outlet decreases downstream"), Dem.DistanceToOutletKm[Cell] > Dem.DistanceToOutletKm[Receiver]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubusLandscapeEvolutionStreamPowerTest, "Orakai.Cubus.LandscapeEvolution.StreamPowerEvolution",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionStreamPowerTest::RunTest(const FString& Parameters)
{
	(void) Parameters;
	FSettings Settings;
	Settings.Seed					  = 73291;
	Settings.Resolution				  = 65;
	Settings.WorldSizeMeters		  = 500000.0;
	Settings.PlateCount				  = 10;
	Settings.RiverSourceAreaKm2		  = 4.0f;
	Settings.EvolutionIterations	  = 4;
	Settings.EvolutionStepYears		  = 10000.0f;
	Settings.HydrologyRefreshInterval = 1;

	FGlobalDem		 A;
	FGlobalDem		 B;
	FGenerationStats StatsA;
	FGenerationStats StatsB;
	FString			 Error;
	if (!TestTrue(TEXT("Evolution skeleton A generates"), FGenerator::GenerateSkeleton(Settings, A, nullptr, &Error)) ||
		!TestTrue(TEXT("Evolution skeleton B generates"), FGenerator::GenerateSkeleton(Settings, B, nullptr, &Error)))
	{
		return false;
	}
	const FString SkeletonMaximumStep = DescribeMaximumStep(A);
	if (!TestTrue(TEXT("Evolution A succeeds"), FGenerator::EvolveLandscape(Settings, A, &StatsA, &Error)) ||
		!TestTrue(TEXT("Evolution B succeeds"), FGenerator::EvolveLandscape(Settings, B, &StatsB, &Error)))
	{
		return false;
	}

	TestEqual(TEXT("Requested evolution iterations execute"), StatsA.EvolutionIterations, Settings.EvolutionIterations);
	TestTrue(TEXT("Stream incision diagnostic exists"), A.StreamIncisionM.Num() == A.NumCells());
	TestTrue(TEXT("At least some stream incision occurs"), StatsA.MaximumStreamIncisionM > 0.0f);
	TestTrue(TEXT("One evolution iteration cannot erase a mountain"),
			 StatsA.MaximumStreamIncisionM <= Settings.MaximumIncisionPerIterationM + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Stream-power evolution cannot raise terrain by kilometres"), StatsA.MaximumTerrainRaisingM < 100.0f);
	const float MaximumAllowedStepM = FMath::Max(750.0f, static_cast<float>(A.CellSizeMeters) * 0.25f);
	if (StatsA.MaximumNeighbourStepM >= MaximumAllowedStepM)
	{
		AddInfo(FString::Printf(TEXT("Before evolution: %s"), *SkeletonMaximumStep));
		AddInfo(DescribeMaximumStep(A));
	}
	TestTrue(*FString::Printf(TEXT("Evolved DEM maximum neighbour step is %.2f m (limit %.2f m)"), StatsA.MaximumNeighbourStepM,
							  MaximumAllowedStepM),
			 StatsA.MaximumNeighbourStepM < MaximumAllowedStepM);
	TestEqual(TEXT("Evolution remains deterministic"), A.ElevationM[A.Index(31, 31)], B.ElevationM[B.Index(31, 31)]);

	for (int32 Cell = 0; Cell < A.NumCells(); Cell += 37)
	{
		const int32 Receiver = A.Receiver[Cell];
		if (Receiver != INDEX_NONE)
		{
			TestTrue(TEXT("Final evolved drainage remains hydrologically downhill"),
					 A.HydrologyElevationM[Receiver] < A.HydrologyElevationM[Cell]);
		}
		TestTrue(TEXT("Incision cannot be negative"), A.StreamIncisionM[Cell] >= 0.0f);
	}
	return true;
}

#endif
