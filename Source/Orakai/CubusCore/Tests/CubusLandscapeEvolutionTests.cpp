#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusLandscapeEvolution.h"

using namespace CubusLandscapeEvolution;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCubusLandscapeEvolutionDeterminismTest,
	"Orakai.Cubus.LandscapeEvolution.DeterministicSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionDeterminismTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSettings Settings;
	Settings.Seed = 918273;
	Settings.Resolution = 65;
	Settings.WorldSizeMeters = 500000.0;
	Settings.PlateCount = 12;

	FGlobalDem A;
	FGlobalDem B;
	FString Error;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCubusLandscapeEvolutionGlobalSpacingTest,
	"Orakai.Cubus.LandscapeEvolution.Global4097Spacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionGlobalSpacingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FGlobalDem Dem;
	Dem.Resolution = 4097;
	Dem.WorldSizeMeters = 500000.0;
	Dem.CellSizeMeters = Dem.WorldSizeMeters / static_cast<double>(Dem.Resolution - 1);
	TestTrue(TEXT("500 km / 4096 intervals is approximately 122.0703125 m"),
		FMath::IsNearlyEqual(Dem.CellSizeMeters, 122.0703125, 0.000001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCubusLandscapeEvolutionHydrologyTest,
	"Orakai.Cubus.LandscapeEvolution.HydrologyMonotonicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionHydrologyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSettings Settings;
	Settings.Seed = 44771;
	Settings.Resolution = 65;
	Settings.WorldSizeMeters = 64000.0;
	Settings.PlateCount = 10;
	Settings.RiverSourceAreaKm2 = 4.0f;

	FGlobalDem Dem;
	FGenerationStats Stats;
	FString Error;
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
			TestTrue(TEXT("Receiver is hydrologically downhill"),
				Dem.HydrologyElevationM[Receiver] < Dem.HydrologyElevationM[Cell]);
			TestTrue(TEXT("Donor appears before receiver in flow order"), OrderPosition[Cell] < OrderPosition[Receiver]);
			TestTrue(TEXT("Receiver drainage area includes donor area"),
				Dem.DrainageAreaKm2[Receiver] + KINDA_SMALL_NUMBER >= Dem.DrainageAreaKm2[Cell]);
			TestEqual(TEXT("Donor and receiver share basin"), Dem.BasinId[Cell], Dem.BasinId[Receiver]);
			TestTrue(TEXT("Distance to outlet decreases downstream"),
				Dem.DistanceToOutletKm[Cell] > Dem.DistanceToOutletKm[Receiver]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCubusLandscapeEvolutionStreamPowerTest,
	"Orakai.Cubus.LandscapeEvolution.StreamPowerEvolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubusLandscapeEvolutionStreamPowerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSettings Settings;
	Settings.Seed = 73291;
	Settings.Resolution = 65;
	Settings.WorldSizeMeters = 64000.0;
	Settings.PlateCount = 10;
	Settings.RiverSourceAreaKm2 = 4.0f;
	Settings.EvolutionIterations = 4;
	Settings.EvolutionStepYears = 10000.0f;
	Settings.HydrologyRefreshInterval = 1;

	FGlobalDem A;
	FGlobalDem B;
	FGenerationStats StatsA;
	FGenerationStats StatsB;
	FString Error;
	if (!TestTrue(TEXT("Evolution skeleton A generates"), FGenerator::GenerateSkeleton(Settings, A, nullptr, &Error)) ||
		!TestTrue(TEXT("Evolution skeleton B generates"), FGenerator::GenerateSkeleton(Settings, B, nullptr, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Evolution A succeeds"), FGenerator::EvolveLandscape(Settings, A, &StatsA, &Error)) ||
		!TestTrue(TEXT("Evolution B succeeds"), FGenerator::EvolveLandscape(Settings, B, &StatsB, &Error)))
	{
		return false;
	}

	TestEqual(TEXT("Requested evolution iterations execute"), StatsA.EvolutionIterations, Settings.EvolutionIterations);
	TestTrue(TEXT("Stream incision diagnostic exists"), A.StreamIncisionM.Num() == A.NumCells());
	TestTrue(TEXT("At least some stream incision occurs"), StatsA.MaximumStreamIncisionM > 0.0f);
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
