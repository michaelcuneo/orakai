#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Generation/CubusDemIslandGenerator.h"
#include "CubusCore/Generation/CubusLandscapeEvolution.h"
#include "CubusLandscapeEvolutionController.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ECubusLandscapeDebugView : uint8
{
	Elevation,
	Province,
	Plate,
	Uplift,
	FlowDirection,
	DrainageArea,
	Watershed,
	RiverNetwork,
	StreamIncision,
	EvolutionDelta
};

UENUM(BlueprintType)
enum class ECubusLandscapePreviewMode : uint8
{
	NativeDetail UMETA(DisplayName = "Native Detail"),
	FullWorldOverview UMETA(DisplayName = "Full 500 km Island")
};

UENUM(BlueprintType)
enum class ECubusLandscapeEditorAction : uint8
{
	None UMETA(DisplayName = "-- Choose Action --"),
	GenerateRandomDemIsland UMETA(DisplayName = "Generate 500 km Real-DEM World"),
	GenerateWorldSkeleton UMETA(DisplayName = "Generate Legacy World Skeleton"),
	SolveGlobalHydrology UMETA(DisplayName = "Solve Hydrology"),
	EvolveLandscape UMETA(DisplayName = "Evolve Current Landscape"),
	GenerateAndSolve UMETA(DisplayName = "Generate Legacy + Solve"),
	GenerateSolveAndEvolve UMETA(DisplayName = "Generate Legacy + Solve + Evolve"),
	RebuildPreview UMETA(DisplayName = "Rebuild Preview")
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusLandscapeEvolutionController final : public AActor
{
	GENERATED_BODY()

public:
	ACubusLandscapeEvolutionController();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|DEM World|Actions", meta = (DisplayName = "Generate 500 km Real-DEM World"))
	void GenerateRandomDemIsland();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate Legacy World Skeleton"))
	void GenerateWorldSkeleton();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Solve Hydrology"))
	void SolveGlobalHydrology();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Evolve Current Landscape"))
	void EvolveLandscape();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate Legacy + Solve"))
	void GenerateAndSolve();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate Legacy + Solve + Evolve"))
	void GenerateSolveAndEvolve();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Rebuild Preview"))
	void RebuildPreview();

	UFUNCTION(BlueprintPure, Category = "Cubus|Landscape Evolution")
	float SampleGlobalHeightMeters(FVector2D WorldMeters) const;

	UFUNCTION(BlueprintPure, Category = "Cubus|Landscape Evolution")
	bool HasGeneratedDem() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Landscape Evolution")
	TObjectPtr<UProceduralMeshComponent> PreviewMesh;

	UPROPERTY(EditAnywhere, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "RUN EDITOR ACTION"))
	ECubusLandscapeEditorAction EditorAction = ECubusLandscapeEditorAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World", meta = (ClampMin = "513", ClampMax = "8193"))
	int32 GlobalResolution = 4097;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World", meta = (ClampMin = "10000.0", ClampMax = "1000000.0", Units = "m"))
	double WorldSizeMeters = 500000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Source")
	FString DemSourceDirectory = TEXT("Cubus/TerrainSources/DEM/Prepared");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Source", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float DemReliefScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Source", meta = (ClampMin = "0.0", ClampMax = "0.48"))
	float DemSecondaryBlend = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Shape", meta = (ClampMin = "0.0", ClampMax = "50000.0", Units = "m"))
	float DemWarpMeters = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Shape", meta = (ClampMin = "0.0", ClampMax = "1000.0", Units = "m"))
	float DemBaseLandElevationM = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Shape", meta = (ClampMin = "5000.0", ClampMax = "100000.0", Units = "m"))
	float DemCoastBandM = 45000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Provinces", meta = (ClampMin = "4", ClampMax = "64"))
	int32 DemProvinceCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Provinces", meta = (ClampMin = "10.0", ClampMax = "150.0", Units = "km"))
	float DemProvinceMinRadiusKm = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Provinces", meta = (ClampMin = "20.0", ClampMax = "250.0", Units = "km"))
	float DemProvinceMaxRadiusKm = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Provinces", meta = (ClampMin = "1.0", ClampMax = "80.0", Units = "km"))
	float DemProvinceBlendKm = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Shape")
	bool bEvolveDemIsland = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Legacy", meta = (ClampMin = "2", ClampMax = "30"))
	int32 PlateCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Ocean", meta = (Units = "m"))
	float OceanLevelM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM World|Ocean", meta = (ClampMax = "-1.0", Units = "m"))
	float OceanFloorM = -1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Legacy", meta = (ClampMin = "1000.0", Units = "m"))
	float CoastalMarginM = 45000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Hydrology", meta = (ClampMin = "0.01"))
	float RiverSourceAreaKm2 = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "64"))
	int32 EvolutionIterations = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1.0"))
	float EvolutionStepYears = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float StreamPowerK = 5.0e-7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StreamPowerM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0", Units = "m"))
	float MaximumIncisionPerIterationM = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float BaseUpliftRateMPerYear = 0.00015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float HillslopeDiffusivityM2PerYear = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "16"))
	int32 HydrologyRefreshInterval = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "17", ClampMax = "513"))
	int32 PreviewResolution = 513;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	ECubusLandscapePreviewMode PreviewMode = ECubusLandscapePreviewMode::FullWorldOverview;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bAutoFocusErosion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (Units = "m"))
	FVector2D PreviewCenterWorldMeters = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.0001"))
	float PreviewHorizontalScale = 0.001f;

	/** Display-only vertical exaggeration. A 500 km overview needs exaggeration or even kilometre-high mountains look almost planar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.01", ClampMax = "100.0", DisplayName = "Preview Vertical Exaggeration"))
	float PreviewVerticalScale = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	ECubusLandscapeDebugView DebugView = ECubusLandscapeDebugView::Elevation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bGenerateOnConstruction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bShowViewportDiagnostics = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") int32 GeneratedCellCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m")) float GlobalCellSizeM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m")) float PreviewDisplayedCellSizeM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "km")) float PreviewWindowSizeKm = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m")) FVector2D PreviewActualCenterWorldMeters = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") int32 GeneratedRiverCellCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") int32 GeneratedBasinCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MinimumElevationM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MaximumElevationM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MaximumStreamIncisionM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MeanStreamIncisionM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MaximumAbsoluteElevationChangeM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MeanAbsoluteElevationChangeM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MaximumTerrainLoweringM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float MaximumTerrainRaisingM = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float LandPercent = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "deg")) float MeanLandSlopeDegrees = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics") float SteepLandPercent = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m")) float MaximumNeighbourStepM = 0.0f;

private:
	CubusLandscapeEvolution::FSettings MakeSettings() const;
	CubusDemIsland::FSettings MakeDemIslandSettings() const;
	FLinearColor DebugColorForCell(int32 Cell) const;
	FString BuildViewportDiagnosticsText() const;
	void UpdateDiagnostics(const CubusLandscapeEvolution::FGenerationStats& Stats);

	CubusLandscapeEvolution::FGlobalDem GlobalDem;
};
