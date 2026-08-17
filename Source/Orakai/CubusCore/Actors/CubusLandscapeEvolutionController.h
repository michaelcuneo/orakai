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
	FullWorldOverview UMETA(DisplayName = "Full 500 m Island")
};

UENUM(BlueprintType)
enum class ECubusLandscapeEditorAction : uint8
{
	None UMETA(DisplayName = "-- Choose Action --"),
	GenerateRandomDemIsland UMETA(DisplayName = "Generate Random Real-DEM Island"),
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

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|DEM Island|Actions", meta = (DisplayName = "Generate Random Real-DEM Island"))
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

	UPROPERTY(EditAnywhere, Category = "Cubus|Landscape Evolution|Actions",
		meta = (DisplayName = "RUN EDITOR ACTION", ToolTip = "Choose an action to execute immediately on this placed controller."))
	ECubusLandscapeEditorAction EditorAction = ECubusLandscapeEditorAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island")
	int32 Seed = 1337;

	/** One metre authoritative surface over the 500 x 500 m island. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island", meta = (ClampMin = "129", ClampMax = "2049"))
	int32 GlobalResolution = 501;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island", meta = (ClampMin = "100.0", ClampMax = "5000.0", Units = "m"))
	double WorldSizeMeters = 500.0;

	/** Prepared .cdem files live below Project/Content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Source")
	FString DemSourceDirectory = TEXT("Cubus/TerrainSources/DEM/Prepared");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Source", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DemReliefScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Source", meta = (ClampMin = "0.0", ClampMax = "0.48"))
	float DemSecondaryBlend = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Source", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "m"))
	float DemWarpMeters = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Shape", meta = (ClampMin = "5.0", ClampMax = "100.0", Units = "m"))
	float DemBaseLandElevationM = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Shape", meta = (ClampMin = "8.0", ClampMax = "150.0", Units = "m"))
	float DemCoastBandM = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Shape")
	bool bEvolveDemIsland = true;

	/** Legacy procedural skeleton plate count, retained while the old generator remains available for comparison. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Legacy", meta = (ClampMin = "2", ClampMax = "30"))
	int32 PlateCount = 18;

	/** Water surface datum. Terrain below this elevation is ocean. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Ocean", meta = (Units = "m"))
	float OceanLevelM = 0.0f;

	/** Seafloor around the 500 m island. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|DEM Island|Ocean", meta = (ClampMax = "-1.0", Units = "m"))
	float OceanFloorM = -40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Legacy", meta = (ClampMin = "1.0", Units = "m"))
	float CoastalMarginM = 55.0f;

	/** 0.004 km^2 = 4,000 m^2 catchment: appropriate for small island streams. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Hydrology", meta = (ClampMin = "0.0001"))
	float RiverSourceAreaKm2 = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "64"))
	int32 EvolutionIterations = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1.0"))
	float EvolutionStepYears = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float StreamPowerK = 5.0e-7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StreamPowerM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0", Units = "m"))
	float MaximumIncisionPerIterationM = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float BaseUpliftRateMPerYear = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float HillslopeDiffusivityM2PerYear = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "16"))
	int32 HydrologyRefreshInterval = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "17", ClampMax = "513"))
	int32 PreviewResolution = 501;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	ECubusLandscapePreviewMode PreviewMode = ECubusLandscapePreviewMode::FullWorldOverview;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bAutoFocusErosion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (Units = "m"))
	FVector2D PreviewCenterWorldMeters = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.0001"))
	float PreviewHorizontalScale = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.01", DisplayName = "Preview Vertical Exaggeration"))
	float PreviewVerticalScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	ECubusLandscapeDebugView DebugView = ECubusLandscapeDebugView::Elevation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bGenerateOnConstruction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bShowViewportDiagnostics = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	int32 GeneratedCellCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m"))
	float GlobalCellSizeM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m"))
	float PreviewDisplayedCellSizeM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "km"))
	float PreviewWindowSizeKm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m"))
	FVector2D PreviewActualCenterWorldMeters = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	int32 GeneratedRiverCellCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	int32 GeneratedBasinCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MinimumElevationM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MaximumElevationM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MaximumStreamIncisionM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MeanStreamIncisionM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MaximumAbsoluteElevationChangeM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MeanAbsoluteElevationChangeM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MaximumTerrainLoweringM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float MaximumTerrainRaisingM = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float LandPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "deg"))
	float MeanLandSlopeDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	float SteepLandPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics", meta = (Units = "m"))
	float MaximumNeighbourStepM = 0.0f;

private:
	CubusLandscapeEvolution::FSettings MakeSettings() const;
	CubusDemIsland::FSettings MakeDemIslandSettings() const;
	FLinearColor DebugColorForCell(int32 Cell) const;
	FString BuildViewportDiagnosticsText() const;
	void UpdateDiagnostics(const CubusLandscapeEvolution::FGenerationStats& Stats);

	CubusLandscapeEvolution::FGlobalDem GlobalDem;
};
