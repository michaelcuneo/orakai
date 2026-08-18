#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tasks/Task.h"

#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusMeshData.h"
#include "CubusCore/Meshing/CubusDensityLod.h"

#include "CubusTerrainLodWorldActor.generated.h"

class ACubusBlockWorldActor;
class UProceduralMeshComponent;
class USceneComponent;
class UMaterialInterface;

struct FCubusTerrainLodTileBuildInput
{
	FCubusTerrainDensitySettings DensitySettings;
	FIntVector TileCoordinate = FIntVector::ZeroValue;
	int32 CanonicalVoxelStride = 2;
	int32 MeshingSubdivisions = 2;
	FCubusDensityTransitionFaces TransitionFaces;
	float CanonicalVoxelSize = 100.0f;
	float IsoLevel = 0.0f;
};

struct FCubusTerrainLodTileBuildResult
{
	FIntVector TileCoordinate = FIntVector::ZeroValue;
	TMap<int32, FCubusMeshData> MaterialMeshes;
	int32 GeneratedTriangleCount = 0;
	double BuildTimeMilliseconds = 0.0;
	int32 AppliedRefinement = 2;
	uint32 TransitionSignature = 0;
};

struct FCubusTerrainLodTileBuild
{
	FIntVector TileCoordinate = FIntVector::ZeroValue;
	UE::Tasks::TTask<FCubusTerrainLodTileBuildResult> Task;
};

struct FCubusTerrainLodTierRuntime
{
	int32 LodLevel = 1;
	int32 CanonicalVoxelStride = 2;
	int32 MeshingSubdivisions = 2;
	int32 VerticalRadiusTiles = 0;

	FCubusDensityTileBounds2D InnerBounds;
	FCubusDensityTileBounds2D OuterBounds;

	TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> TileComponents;
	TSet<FIntVector> RequiredTiles;
	TSet<FIntVector> ResolvedTiles;
	TSet<FIntVector> TilesBuilding;
	TArray<FIntVector> PendingTiles;
	TArray<FCubusTerrainLodTileBuild> ActiveBuilds;
	TArray<FCubusTerrainLodTileBuildResult> CompletedBuilds;
	TMap<FIntVector, uint32> ResolvedTransitionSignatures;
};

/**
 * World-level owner for coarse DEM-derived density tiles outside gameplay LOD0.
 * All tiers sample the same authoritative density field as the gameplay chunks.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus", meta = (DisplayName = "Cubus Terrain LOD World"))
class ORAKAI_API ACubusTerrainLodWorldActor : public AActor
{
	GENERATED_BODY()

public:
	ACubusTerrainLodWorldActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool IsInitialVisualCoverageReady() const;
	float GetInitialVisualCoverageProgress() const;

	/**
	 * Spawn only waits for the useful nearby coarse context. LOD1 and LOD2 are
	 * enough to surround the compact voxel spawn window; LOD3-LOD6 are allowed
	 * to continue building asynchronously after the character enters the world.
	 */
	bool IsPreSpawnVisualCoverageReady() const
	{
		return IsTierWindowResident(Lod1Runtime) && IsTierWindowResident(Lod2Runtime);
	}

	float GetPreSpawnVisualCoverageProgress() const
	{
		const FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime};
		int32 RequiredCount = 0;
		int32 ReadyCount = 0;
		for (const FCubusTerrainLodTierRuntime* Tier : Tiers)
		{
			RequiredCount += Tier->RequiredTiles.Num();
			for (const FIntVector& Coordinate : Tier->RequiredTiles)
			{
				ReadyCount += Tier->ResolvedTiles.Contains(Coordinate) ? 1 : 0;
			}
		}
		return RequiredCount > 0
			? static_cast<float>(ReadyCount) / static_cast<float>(RequiredCount)
			: 0.0f;
	}

	/**
	 * LOD tiles may be built ahead of the player, but the spawn screen only
	 * needs a limited visible horizon. This toggles already-created components;
	 * GameMode reapplies it while staging so newly uploaded far tiles are hidden
	 * on the next frame. Pass 6 after possession to expose the full clipmap.
	 */
	void SetMaximumVisibleLod(const int32 InMaximumVisibleLod)
	{
		MaximumVisibleLod = FMath::Clamp(InMaximumVisibleLod, 0, 6);
		FCubusTerrainLodTierRuntime* Tiers[] = {
			&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,
			&Lod4Runtime, &Lod5Runtime, &Lod6Runtime
		};
		for (FCubusTerrainLodTierRuntime* Tier : Tiers)
		{
			const bool bVisible = Tier->LodLevel <= MaximumVisibleLod;
			for (TPair<FIntVector, TObjectPtr<UProceduralMeshComponent>>& Pair : Tier->TileComponents)
			{
				if (IsValid(Pair.Value))
				{
					Pair.Value->SetVisibility(bVisible);
					Pair.Value->SetHiddenInGame(!bVisible);
					Pair.Value->SetRenderInMainPass(bVisible);
					Pair.Value->SetRenderInDepthPass(bVisible);
				}
			}
		}
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD")
	bool bEnableTerrainLod = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "2", ClampMax = "2"))
	int32 Lod1CanonicalVoxelStride = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Lod1OverlapTiles = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "1", ClampMax = "32"))
	int32 Lod1OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod1VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "4", ClampMax = "4"))
	int32 Lod2CanonicalVoxelStride = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Lod2OverlapTiles = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "1", ClampMax = "32"))
	int32 Lod2OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod2VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "8", ClampMax = "8"))
	int32 Lod3CanonicalVoxelStride = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Lod3OverlapTiles = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "1", ClampMax = "32"))
	int32 Lod3OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod3VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD4", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Lod4OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD4", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod4VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD5", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Lod5OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD5", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod5VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD6", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Lod6OuterRadiusTiles = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD6", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Lod6VerticalRadiusTiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxConcurrentLodBuilds = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxLodBuildStartsPerTick = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxLodUploadsPerTick = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "0.05", Units = "s"))
	float LodStreamingUpdateInterval = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
	int32 LoadedLodTileCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
	int32 BuildingLodTileCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
	int32 PendingLodTileCount = 0;

private:
	void ResolveBlockWorld();
	void UpdateStreaming();
	void UpdateTierStreaming(FCubusTerrainLodTierRuntime& Tier, const FCubusTerrainDensityField& DensityField,
		float CanonicalChunkWorldSize, const FCubusDensityTileBounds2D& InnerBounds,
		const FCubusDensityTileBounds2D& OuterBounds, int32 CanonicalVoxelStride, int32 VerticalRadiusTiles);
	void CollectCompletedBuilds();
	void CollectCompletedBuildsForTier(FCubusTerrainLodTierRuntime& Tier);
	void StartPendingBuilds();
	void UploadCompletedBuilds();
	bool UploadOneCompletedBuild(FCubusTerrainLodTierRuntime& Tier, float CanonicalVoxelSize, UMaterialInterface* TerrainMaterial,
		double& WorkerMilliseconds);
	void RemoveUnneededTiles(FCubusTerrainLodTierRuntime& Tier);
	bool IsTierWindowResident(const FCubusTerrainLodTierRuntime& Tier) const;
	void RetireStableTierWindows();
	void ClearAllTiles();
	void ClearTier(FCubusTerrainLodTierRuntime& Tier);

	static int32 ResolveTierSubdivisions(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);
	static FCubusDensityTransitionFaces BuildTierTransitionFaces(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);

	UProceduralMeshComponent* CreateTileComponent(FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate, float TileWorldSize);
	UMaterialInterface* ResolveTerrainMaterial() const;
	FVector ResolveWorldGridOrigin(float CanonicalChunkWorldSize) const;
	static FCubusTerrainLodTileBuildResult BuildTile(const FCubusTerrainLodTileBuildInput& Input);

	UPROPERTY(Transient)
	TObjectPtr<ACubusBlockWorldActor> BlockWorld;

	FCubusTerrainLodTierRuntime Lod1Runtime;
	FCubusTerrainLodTierRuntime Lod2Runtime;
	FCubusTerrainLodTierRuntime Lod3Runtime;
	FCubusTerrainLodTierRuntime Lod4Runtime;
	FCubusTerrainLodTierRuntime Lod5Runtime;
	FCubusTerrainLodTierRuntime Lod6Runtime;

	int32 MaximumVisibleLod = 6;
	float TimeUntilStreamingUpdate = 0.0f;
	int32 NextLodBuildTierIndex = 0;
	int32 NextLodUploadTierIndex = 0;
};
