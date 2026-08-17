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

	const float CellAreaKm2 = static_cast<float>((Dem.CellSizeMeters * Dem.CellSizeMeters) / 1000000.0);
	for (int32 Cell = 0; Cell < Dem.NumCells(); Cell += 31)
	{
		TestTrue(TEXT("Accumulation contains at least the local cell area"), Dem.DrainageAreaKm2[Cell] + KINDA_SMALL_NUMBER >= CellAreaKm2);
		const int32 Receiver = Dem.Receiver[Cell];
		if (Receiver != INDEX_NONE)
		{
			TestTrue(TEXT("Receiver is hydrologically downhill"),
				Dem.HydrologyElevationM[Receiver] < Dem.HydrologyElevationM[Cell]);
		}
	}
	return true;
}

#endif
