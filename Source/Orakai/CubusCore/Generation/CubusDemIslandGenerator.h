#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusLandscapeEvolution.h"

namespace CubusDemIsland
{
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

struct FIndexedPatch
{
	FString RelativePath;
	FString SourceKey;
	FString TerrainClass;
	TArray<FString> Tags;
	float MinimumElevationM = 0.0f;
	float MaximumElevationM = 0.0f;
	float MeanElevationM = 0.0f;
	float MedianElevationM = 0.0f;
	float ElevationP05M = 0.0f;
	float ElevationP95M = 0.0f;
	float ReliefP90M = 0.0f;
	float MeanSlopeDeg = 0.0f;
	float DominantStructureAngleDeg = 0.0f;
	float DirectionalAnisotropy = 0.0f;
	float MacroSuitability = 0.0f;
	float RegionalSuitability = 0.0f;
	float LocalSuitability = 0.0f;
};

struct FSettings
{
	int32 Seed = 1337;
	int32 Resolution = 4097;
	double WorldSizeMeters = 500000.0;
	float OceanLevelM = 0.0f;
	float OceanFloorM = -1000.0f;
	float CoastBandM = 45000.0f;
	float BaseLandElevationM = 180.0f;
	float ReliefScale = 1.0f;
	float SecondaryBlend = 0.20f;
	float WarpMeters = 12000.0f;
	int32 ProvinceCount = 18;
	float ProvinceMinRadiusKm = 35.0f;
	float ProvinceMaxRadiusKm = 110.0f;
	float ProvinceBlendKm = 18.0f;
	FString SourceDirectory = TEXT("Cubus/TerrainSources/DEM/Prepared");
	FString IndexFileName = TEXT("library_index.json");
};

class ORAKAI_API FGenerator
{
public:
	static FString ResolveSourceDirectory(const FSettings& Settings);
	static bool DiscoverPatches(const FSettings& Settings, TArray<FString>& OutPatchPaths, FString* OutError = nullptr);
	static bool LoadPatch(const FString& Path, FPatch& OutPatch, FString* OutError = nullptr);
	static bool LoadIndex(const FSettings& Settings, TArray<FIndexedPatch>& OutEntries, FString* OutError = nullptr);
	static bool Generate(const FSettings& Settings, CubusLandscapeEvolution::FGlobalDem& OutDem,
		CubusLandscapeEvolution::FGenerationStats* OutStats = nullptr, FString* OutError = nullptr);
};
} // namespace CubusDemIsland
