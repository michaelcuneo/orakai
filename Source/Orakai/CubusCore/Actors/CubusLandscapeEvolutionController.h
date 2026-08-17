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
	StreamIncision
};

/**
 * Isolated world-scale terrain-generation testbed controller intended for Lvl_Streaming.
 * It owns the coarse global DEM and debug preview only; it does not modify the legacy biome generator.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusLandscapeEvolutionController final : public AActor
{
	GENERATED_BODY()

public:
	ACubusLandscapeEvolutionController();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void GenerateWorldSkeleton();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void SolveGlobalHydrology();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void EvolveLandscape();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void GenerateAndSolve();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void GenerateSolveAndEvolve();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Landscape Evolution")
	void RebuildPreview();

	UFUNCTION(BlueprintPure, Category = "Cubus|Landscape Evolution")
	float SampleGlobalHeightMeters(FVector2D WorldMeters) const;

	UFUNCTION(BlueprintPure, Category = "Cubus|Landscape Evolution")
	bool HasGeneratedDem() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Landscape Evolution")
	TObjectPtr<UProceduralMeshComponent> PreviewMesh;

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
	int32 PreviewResolution = 129;

	/** Uniform scale applied to the physical DEM after converting metres to Unreal centimetres. 0.01 displays a 500 km world as 5 km wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Landscape Evolution|Preview", meta = (ClampMin = "0.0001"))
	float PreviewHorizontalScale = 0.01f;

	/** Height exaggeration relative to the uniform preview scale. 1.0 preserves the DEM's real aspect ratio. */
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

private:
	CubusLandscapeEvolution::FSettings MakeSettings() const;
	FLinearColor DebugColorForCell(int32 Cell) const;
	void UpdateDiagnostics(const CubusLandscapeEvolution::FGenerationStats& Stats);

	CubusLandscapeEvolution::FGlobalDem GlobalDem;
};
