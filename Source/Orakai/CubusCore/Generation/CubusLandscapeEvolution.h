#pragma once

#include "CoreMinimal.h"

namespace CubusLandscapeEvolution
{
enum class EBoundaryType : uint8
{
	Stable = 0,
	Convergent,
	Divergent,
	Transform
};

enum class EProvinceType : uint8
{
	StablePlain = 0,
	SedimentaryBasin,
	UpliftedPlateau,
	FoldMountainBelt,
	RiftValley,
	VolcanicProvince,
	CoastalShelf
};

struct FProvinceParameters
{
	float Erodibility = 1.0f;
	float HillslopeDiffusivity = 1.0f;
	float CriticalSlope = 0.65f;
	float GeologicalAge01 = 0.5f;
};

struct FPlate
{
	FVector2D Center01 = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	float BaseElevationM = 250.0f;
	float CrustStrength = 1.0f;
	float Age01 = 0.5f;
	EProvinceType InteriorProvince = EProvinceType::StablePlain;
	bool bVolcanic = false;
};

struct FPlateBoundary
{
	uint8 PlateA = 0;
	uint8 PlateB = 0;
	EBoundaryType Type = EBoundaryType::Stable;
	float RelativeNormalSpeed = 0.0f;
	float RelativeTangentialSpeed = 0.0f;
};

struct FSettings
{
	int32 Seed = 1337;
	int32 Resolution = 4097;
	double WorldSizeMeters = 500000.0;
	int32 PlateCount = 18;

	float OceanLevelM = 0.0f;
	float OceanFloorM = -650.0f;
	float CoastalMarginM = 45000.0f;

	// Plate ownership is categorical, but crustal base elevation is a continuous
	// field. This scale controls how broadly neighbouring plate base elevations
	// blend together; it must never become a hard Voronoi height step.
	float CrustBlendScaleM = 75000.0f;

	float BoundaryWidthM = 28000.0f;
	float BoundaryWarpAmplitudeM = 12000.0f;
	float ConvergentUpliftM = 3200.0f;
	float DivergentSubsidenceM = 850.0f;
	float TransformReliefM = 320.0f;

	float LongWaveMinM = 20000.0f;
	float LongWaveMaxM = 100000.0f;
	float LongWaveAmplitudeM = 60.0f;

	float RiverSourceAreaKm2 = 20.0f;
	float PriorityFloodEpsilonM = 0.001f;

	// First landscape-evolution stage. StreamPowerN is intentionally fixed at 1
	// in the solver so incision can use the unconditionally stable FastScape-style
	// implicit downstream update rather than a tiny explicit timestep.
	int32 EvolutionIterations = 12;
	float EvolutionStepYears = 25000.0f;

	// K is applied with drainage area expressed in square metres. Hydrology keeps
	// its public/debug accumulation in km^2, and the erosion solver converts it.
	float StreamPowerK = 5.0e-7f;
	float StreamPowerM = 0.5f;
	float BaseUpliftRateMPerYear = 0.00015f;
	float HillslopeDiffusivityM2PerYear = 0.03f;
	float MaxNonlinearDiffusionBoost = 4.0f;
	float ChannelDiffusionMultiplier = 0.15f;
	int32 HydrologyRefreshInterval = 2;
};

struct FGenerationStats
{
	double SkeletonSeconds = 0.0;
	double HydrologySeconds = 0.0;
	double EvolutionSeconds = 0.0;
	int32 CellCount = 0;
	int32 BasinCount = 0;
	int32 RiverCellCount = 0;
	int32 EvolutionIterations = 0;
	float MinimumElevationM = 0.0f;
	float MaximumElevationM = 0.0f;
	float MaximumDrainageAreaKm2 = 0.0f;
	float MaximumStreamIncisionM = 0.0f;
	float MeanStreamIncisionM = 0.0f;
	float MaximumAbsoluteElevationChangeM = 0.0f;
	float MeanAbsoluteElevationChangeM = 0.0f;
	float MaximumTerrainLoweringM = 0.0f;
	float MaximumTerrainRaisingM = 0.0f;
};

struct FGlobalDem
{
	int32 Resolution = 0;
	double WorldSizeMeters = 0.0;
	double CellSizeMeters = 0.0;
	float OceanLevelM = 0.0f;

	TArray<FPlate> Plates;
	TArray<FPlateBoundary> PlateBoundaries;

	TArray<float> ElevationM;
	TArray<float> HydrologyElevationM;
	TArray<float> UpliftM;
	TArray<float> DrainageAreaKm2;
	TArray<float> DistanceToOutletKm;
	TArray<float> StreamIncisionM;
	// Final elevation minus the elevation at the start of the most recent
	// EvolveLandscape call. Negative values are net lowering; positive values
	// are net raising from uplift/hillslope transport.
	TArray<float> EvolutionDeltaM;
	TArray<int32> Receiver;
	TArray<int32> BasinId;
	TArray<int32> FlowOrder;
	TArray<uint8> PlateId;
	TArray<uint8> ProvinceId;
	TArray<uint8> BoundaryType;
	TArray<uint8> RiverMask;

	void Reset();
	bool IsValid() const;
	bool HasHydrology() const;
	int32 NumCells() const;
	int32 Index(int32 X, int32 Y) const;
	FIntPoint Coordinates(int32 CellIndex) const;
	bool IsBoundaryCell(int32 X, int32 Y) const;
	float SampleHeightBilinearM(const FVector2D& WorldMeters) const;
};

class ORAKAI_API FGenerator
{
public:
	static bool GenerateSkeleton(const FSettings& Settings, FGlobalDem& OutDem, FGenerationStats* OutStats = nullptr, FString* OutError = nullptr);
	static bool SolveHydrology(const FSettings& Settings, FGlobalDem& InOutDem, FGenerationStats* OutStats = nullptr, FString* OutError = nullptr);
	static bool EvolveLandscape(const FSettings& Settings, FGlobalDem& InOutDem, FGenerationStats* OutStats = nullptr, FString* OutError = nullptr);
	static FProvinceParameters GetDefaultProvinceParameters(EProvinceType Province);
};
} // namespace CubusLandscapeEvolution
