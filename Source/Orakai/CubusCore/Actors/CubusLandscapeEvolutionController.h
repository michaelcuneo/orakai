#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	DrainageArea,
	RiverNetwork,
	StreamIncision,
	EvolutionDelta
};

UENUM(BlueprintType)
enum class ECubusLandscapeEditorAction : uint8
{
	None UMETA(DisplayName = "-- Choose Action --"),
	GenerateWorldSkeleton UMETA(DisplayName = "Generate World Skeleton"),
	SolveGlobalHydrology UMETA(DisplayName = "Solve Global Hydrology"),
	EvolveLandscape UMETA(DisplayName = "Evolve Current Landscape"),
	GenerateAndSolve UMETA(DisplayName = "Generate + Solve"),
	GenerateSolveAndEvolve UMETA(DisplayName = "Generate + Solve + Evolve"),
	RebuildPreview UMETA(DisplayName = "Rebuild Preview")
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusLandscapeEvolutionController final : public AActor
{
	GENERATED_BODY()

public:
	ACubusLandscapeEvolutionController();
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate World Skeleton"))
	void GenerateWorldSkeleton();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Solve Global Hydrology"))
	void SolveGlobalHydrology();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Evolve Current Landscape"))
	void EvolveLandscape();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate + Solve"))
	void GenerateAndSolve();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution|Actions", meta = (DisplayName = "Generate + Solve + Evolve"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Generation")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Generation", meta = (ClampMin = "17", ClampMax = "4097"))
	int32 GlobalResolution = 513;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Generation", meta = (ClampMin = "10000.0", Units = "m"))
	double WorldSizeMeters = 500000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Generation", meta = (ClampMin = "2", ClampMax = "30"))
	int32 PlateCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Generation", meta = (ClampMin = "0.01"))
	float RiverSourceAreaKm2 = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "128"))
	int32 EvolutionIterations = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "100.0"))
	float EvolutionStepYears = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float StreamPowerK = 5.0e-7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StreamPowerM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float BaseUpliftRateMPerYear = 0.00015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "0.0"))
	float HillslopeDiffusivityM2PerYear = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Evolution", meta = (ClampMin = "1", ClampMax = "16"))
	int32 HydrologyRefreshInterval = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "17", ClampMax = "513"))
	int32 PreviewResolution = 513;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.0001"))
	float PreviewHorizontalScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.01", DisplayName = "Preview Vertical Exaggeration"))
	float PreviewVerticalScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	ECubusLandscapeDebugView DebugView = ECubusLandscapeDebugView::Elevation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview")
	bool bGenerateOnConstruction = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Landscape Evolution|Diagnostics")
	int32 GeneratedCellCount = 0;

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

private:
	CubusLandscapeEvolution::FSettings MakeSettings() const;
	FLinearColor DebugColorForCell(int32 Cell) const;
	void UpdateDiagnostics(const CubusLandscapeEvolution::FGenerationStats& Stats);

	CubusLandscapeEvolution::FGlobalDem GlobalDem;
};
