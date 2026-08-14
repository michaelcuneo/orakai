#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityEditField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusDensityLod.h"
#include "CubusCore/Meshing/CubusMeshData.h"
#include "CubusCore/Rendering/CubusVoxelRenderMode.h"

#include "CubusVoxelVolumeActor.generated.h"

class ACubusBlockWorldActor;
class UCubusMaterialRegistry;
class UCubusGeologyProfile;
class UProceduralMeshComponent;
struct FCubusBlockChunkNeighborhood;

struct FCubusDensityMeshBuildInput
{
	FCubusTerrainDensitySettings DensitySettings;
	FCubusDensityEditMap		 DensityEdits;

	/*
	 * Immutable generated subdivision-1 baseline shared with the worker.
	 *
	 * The worker creates its own mutable copy before applying density edits,
	 * so this baseline is never modified by an async build.
	 */
	TSharedPtr<const FCubusDensitySamplingBuffer, ESPMode::ThreadSafe> GeneratedDensityBuffer;

	FIntVector ChunkCoordinate = FIntVector::ZeroValue;

	float VoxelSize			   = 100.0f;
	int32 SubdivisionsPerVoxel = 1;
	float IsoLevel			   = 0.0f;

	bool bHasGeneratedDensityBuffer = false;
};

struct FCubusDensityMeshBuildResult
{
	TMap<int32, FCubusMeshData> MaterialMeshes;

	/*
	 * When a worker had to generate the canonical terrain baseline for the
	 * first time, return it so the chunk actor can retain it for subsequent
	 * edits.
	 */
	FCubusDensitySamplingBuffer GeneratedDensityBuffer;

	int32 GeneratedTriangleCount = 0;

	double BuildTimeMilliseconds = 0.0;

	bool bHasGeneratedDensityBuffer = false;

	void Reset()
	{
		MaterialMeshes.Reset();
		GeneratedDensityBuffer.Reset();

		GeneratedTriangleCount = 0;
		BuildTimeMilliseconds  = 0.0;

		bHasGeneratedDensityBuffer = false;
	}
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus", meta = (DisplayName = "Cubus Voxel Chunk"))
class ORAKAI_API ACubusVoxelVolumeActor : public AActor
{
	GENERATED_BODY()

public:
	ACubusVoxelVolumeActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GenerateTerrainData();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Rendering")
	void RebuildVolume();

	FCubusTerrainDensitySettings CaptureTerrainDensitySettings() const { return BuildDensitySettings(); }

	const UCubusGeologyProfile* GetGeologyProfile() const { return GeologyProfile.Get(); }

	FCubusDensityMeshBuildInput CaptureDensityMeshBuildInput() const;

	static FCubusDensityMeshBuildResult BuildDensityMeshData(const FCubusDensityMeshBuildInput& Input);

	bool BuildStagedVolumeFromDensityMesh(FCubusDensityMeshBuildResult& BuildResult);

	/*
	 * Builds a complete replacement mesh into the hidden staging component.
	 * The currently visible terrain is not modified.
	 */
	bool BuildStagedVolume();

	/*
	 * Publishes the staged mesh and retires the previously visible mesh.
	 * Stage 3H-B will call this for all chunks in an edit batch together.
	 */
	void CommitStagedVolume();

	/*
	 * Throws away an uncommitted staged rebuild.
	 */
	void DiscardStagedVolume();

	bool HasStagedVolume() const { return bHasStagedVolume; }

	UFUNCTION(BlueprintPure, Category = "Cubus|Rendering")
	ECubusVoxelRenderMode GetEffectiveRenderMode() const;

	const FIntVector& GetChunkCoordinate() const { return ChunkCoordinate; }

	float GetVoxelSize() const { return VoxelSize; }

	int32 GetDensitySubdivisionsPerVoxel() const { return DensitySubdivisionsPerVoxel; }

	float GetDensitySampleSpacing() const { return FCubusDensityLod::GetSampleSpacing(VoxelSize, DensitySubdivisionsPerVoxel); }

	const FCubusBlockChunkData* GetChunkData() const
	{
		const_cast<ACubusVoxelVolumeActor*>(this)->EnsureVegetationDataInitialized();

		return ChunkData.Get();
	}

	FCubusBlockChunkData* GetMutableChunkData()
	{
		EnsureVegetationDataInitialized();
		return ChunkData.Get();
	}

	UProceduralMeshComponent* GetTerrainMeshComponent() const { return ActiveProceduralMesh.Get(); }

	bool HasBuiltTerrainCollision() const { return bLastBuildHadCollision; }

	bool TryLoadCachedChunk();
	bool SaveCachedChunk() const;
	void RegenerateVegetationData();

	void SetGenerateVegetationData(const bool bEnabled) { bGenerateVegetationData = bEnabled; }

	void MarkChunkCacheDirty() { bChunkCacheDirty = true; }

	void SetOwningBlockWorld(ACubusBlockWorldActor* InBlockWorld) { OwningBlockWorld = InBlockWorld; }

	UFUNCTION(BlueprintPure, Category = "Cubus|Chunk")
	ACubusBlockWorldActor* GetOwningBlockWorld() const { return OwningBlockWorld; }

	void SetGenerateCollision(const bool bEnabled) { bGenerateCollision = bEnabled; }

	void ConfigureGenerationSeeds(const FCubusGenerationSeeds& InGenerationSeeds)
	{
		EnsureChunkData();

		ChunkData->SetGenerationSeeds(InGenerationSeeds);

		InvalidateGeneratedDensityCache();
	}

	void ConfigureGeneratedChunk(const FIntVector& InChunkCoordinate, float InVoxelSize, ACubusBlockWorldActor* InBlockWorld);

	/** Returns true when the chunk needs a density remesh. */
	bool ConfigureDensityResolution(int32 InSubdivisionsPerVoxel);

	void ConfigureRendering(UCubusMaterialRegistry* InMaterialRegistry);

	void ConfigureGeology(UCubusGeologyProfile* InGeologyProfile);

	void ConfigureTerrain(bool bInUseHeightTerrain, int32 InTerrainSurfaceWorldZ, int32 InTerrainBaseHeight,
						  float InTerrainContinentAmplitude, float InTerrainContinentFrequency, float InTerrainHillAmplitude,
						  float InTerrainHillFrequency, float InTerrainDetailAmplitude, float InTerrainDetailFrequency,
						  float InTerrainRidgeAmplitude, float InTerrainRidgeFrequency, float InTerrainValleyDepth,
						  float InTerrainValleyFrequency, float InTerrainValleyWidth, float InTerrainValleyFalloff,
						  float InTerrainValleyWarpAmplitude, float InTerrainValleyWarpFrequency, float InTerrainRegionFrequency,
						  float InTerrainPlainsThreshold, float InTerrainPlainsBlend, float InTerrainMountainThreshold,
						  float InTerrainMountainBlend, int32 InTerrainSurfaceMaterialId, int32 InTerrainSubsurfaceMaterialId,
						  int32 InTerrainRockMaterialId, int32 InTerrainSnowMaterialId, float InTerrainRockSlopeThreshold,
						  int32 InTerrainSnowMinimumHeight, bool bInGenerateWater, int32 InTerrainWaterLevel,
						  int32 InTerrainWaterMaterialId);

protected:
	UPROPERTY(Transient)
	bool bGenerateVegetationData = true;

	/**
	 * The single authoritative terrain render component for every mode.
	 *
	 * Block, density and hybrid sections are all submitted here so collision,
	 * hit resolution, streaming, ray tracing and teardown operate on the same
	 * component.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	/*
	 * Hidden back-buffer used for transactional terrain rebuilds.
	 *
	 * Mesh generation writes here while ProceduralMesh/ActiveProceduralMesh
	 * continues displaying the previous complete terrain revision.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
	TObjectPtr<UProceduralMeshComponent> StagingProceduralMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ActiveProceduralMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> InactiveProceduralMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cubus|Chunk")
	FIntVector ChunkCoordinate = FIntVector::ZeroValue;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cubus|Chunk")
	TObjectPtr<ACubusBlockWorldActor> OwningBlockWorld;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cubus|Chunk", meta = (Units = "cm"))
	float VoxelSize = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cubus|Density LOD", meta = (AllowPrivateAccess = "true"))
	int32 DensitySubdivisionsPerVoxel = 1;

	/*
	 * Generated canonical density is immutable until terrain configuration,
	 * generation seeds, geology, or chunk identity changes.
	 *
	 * Player density edits are never written into this buffer.
	 *
	 * Build inputs share this immutable baseline rather than copying all
	 * 42,875 samples on the game thread.
	 */
	TSharedPtr<const FCubusDensitySamplingBuffer, ESPMode::ThreadSafe> CachedGeneratedDensityBuffer;

	bool bHasCachedGeneratedDensityBuffer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Rendering",
			  meta = (ToolTip = "Used only when this chunk has no owning Cubus Block World."))
	ECubusVoxelRenderMode StandaloneRenderMode = ECubusVoxelRenderMode::Blocks;

	bool bUseHeightTerrain = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
	int32 TerrainSurfaceWorldZ = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain", meta = (ClampMin = "1"))
	int32 TerrainSurfaceMaterialId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain", meta = (ClampMin = "1"))
	int32 TerrainSubsurfaceMaterialId = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Collision")
	bool bGenerateCollision = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cubus|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCubusMaterialRegistry> MaterialRegistry = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCubusGeologyProfile> GeologyProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
	int32 TerrainBaseHeight = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainContinentAmplitude = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainContinentFrequency = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainHillAmplitude = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainHillFrequency = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainDetailAmplitude = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainDetailFrequency = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainRidgeAmplitude = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainRidgeFrequency = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainValleyDepth = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainValleyFrequency = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TerrainValleyWidth = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float TerrainValleyFalloff = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainValleyWarpAmplitude = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
	float TerrainValleyWarpFrequency = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.000001"))
	float TerrainRegionFrequency = 0.0025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TerrainPlainsThreshold = -0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float TerrainPlainsBlend = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TerrainMountainThreshold = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float TerrainMountainBlend = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
	int32 TerrainRockMaterialId = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
	int32 TerrainSnowMaterialId = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "0.0"))
	float TerrainRockSlopeThreshold = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials")
	int32 TerrainSnowMinimumHeight = 34;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
	bool bGenerateWater = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
	int32 TerrainWaterLevel = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water", meta = (ClampMin = "1"))
	int32 TerrainWaterMaterialId = 5;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 TotalVoxelCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 SolidVoxelCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedFaceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedVertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedTriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedMaterialSectionCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedBlockSectionCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedDensitySectionCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 GeneratedDensityTriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	ECubusVoxelRenderMode LastBuiltRenderMode = ECubusVoxelRenderMode::Blocks;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics", meta = (Units = "ms"))
	float LastBuildTimeMilliseconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
	int32 RebuildCount = 0;

private:
	TUniquePtr<FCubusBlockChunkData> ChunkData;

	bool bChunkCacheDirty		= false;
	bool bLastBuildHadCollision = false;

	bool bHasStagedVolume		  = false;
	bool bStagedBuildHadCollision = false;

	void EnsureChunkData();

	void EnsureVegetationDataInitialized()
	{
		EnsureChunkData();

		if (ChunkData.IsValid() && ChunkData->GetVegetationRevision() == 0)
		{
			RegenerateVegetationData();
		}
	}

	void SynchronizeChunkState();

	bool BuildVolumeInto(UProceduralMeshComponent& TargetMesh, bool bPublishDiagnostics);

	void RebuildBlockMesh(UProceduralMeshComponent& TargetMesh, bool bGenerateBlockCollision, int32& InOutMeshSectionIndex);

	FCubusTerrainDensitySettings BuildDensitySettings() const;

	void UploadDensityMesh(UProceduralMeshComponent& TargetMesh, FCubusDensityMeshBuildResult& BuildResult, bool bGenerateDensityCollision,
						   int32& InOutMeshSectionIndex);

	void RebuildDensityMesh(UProceduralMeshComponent& TargetMesh, bool bGenerateDensityCollision, int32& InOutMeshSectionIndex);

	void RebuildBlockEditOverlay(UProceduralMeshComponent& TargetMesh, bool bGenerateBlockCollision, int32& InOutMeshSectionIndex);

	void InvalidateGeneratedDensityCache()
	{
		CachedGeneratedDensityBuffer.Reset();
		bHasCachedGeneratedDensityBuffer = false;
	}

	const FCubusBlockChunkData* FindNeighbourChunkData(const FIntVector& CoordinateOffset) const;

	FCubusBlockChunkNeighborhood BuildNeighborhood() const;

	void RebuildAffectedChunks();
	void GenerateHeightTerrain();
	void GenerateFlatTerrain();
	void ResetDiagnostics();
};
