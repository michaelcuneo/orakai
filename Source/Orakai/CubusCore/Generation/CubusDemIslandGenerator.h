#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusLandscapeEvolution.h"

namespace CubusDemIsland
{
/** Prepared real-world DEM patch written by Tools/DemLibrary/prepare_linz_coastal.py. */
struct FPatch
{
	FString SourcePath;
	int32 Width = 0;
	int32 Height = 0;
	float CellSizeM = 1.0f;
	float MeanElevationM = 0.0f;
	float MinimumElevationM = 0.0f;
	float MaximumElevationM = 0.0f;
	TArray<float> ElevationM;

	bool IsValid() const;
	float SampleBilinear(float U, float V) const;
};

struct FSettings
{
	int32 Seed = 1337;
	int32 Resolution = 501;
	double WorldSizeMeters = 500.0;
	float OceanLevelM = 0.0f;
	float OceanFloorM = -40.0f;

	/** Width of the forced island-to-ocean transition near the generated coast. */
	float CoastBandM = 55.0f;

	/** Raises the remixed real DEM around sea level before the coastal envelope is applied. */
	float BaseLandElevationM = 24.0f;

	/** Scales the source patch relief after its absolute NZ elevation datum is removed. */
	float ReliefScale = 0.65f;

	/** Amount of a second real DEM patch mixed into the primary source. */
	float SecondaryBlend = 0.24f;

	/** Low-frequency coordinate displacement applied before sampling source patches. */
	float WarpMeters = 28.0f;

	/** Relative to Project/Content unless an absolute path is supplied. */
	FString SourceDirectory = TEXT("Cubus/TerrainSources/DEM/Prepared");
};

class ORAKAI_API FGenerator
{
public:
	static FString ResolveSourceDirectory(const FSettings& Settings);
	static bool DiscoverPatches(const FSettings& Settings, TArray<FString>& OutPatchPaths, FString* OutError = nullptr);
	static bool LoadPatch(const FString& Path, FPatch& OutPatch, FString* OutError = nullptr);
	static bool Generate(const FSettings& Settings, CubusLandscapeEvolution::FGlobalDem& OutDem,
		CubusLandscapeEvolution::FGenerationStats* OutStats = nullptr, FString* OutError = nullptr);
};
} // namespace CubusDemIsland
