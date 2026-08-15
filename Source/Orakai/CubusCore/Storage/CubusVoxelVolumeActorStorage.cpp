#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"
#include "CubusCore/Generation/CubusTerrainClutterGenerator.h"
#include "CubusCore/Storage/CubusChunkStore.h"
#include "CubusCore/Storage/CubusDensityChunkStore.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

#include "ProceduralMeshComponent.h"

namespace CubusVoxelVolumeActorStorage
{
FCubusChunkStoreContext MakeContext(const FCubusBlockChunkData& ChunkData)
{
	FCubusChunkStoreContext Context;
	Context.WorldSeed		  = ChunkData.GetGenerationSeeds().World;
	Context.GenerationVersion = FCubusGenerationSeeds::CurrentGenerationVersion;
	return Context;
}

FCubusDensityChunkStoreContext MakeDensityContext(const ACubusVoxelVolumeActor& Actor, const FCubusBlockChunkData& ChunkData)
{
	FCubusDensityChunkStoreContext Context;
	const ACubusBlockWorldActor* BlockWorld = Actor.GetOwningBlockWorld();
	Context.WorldSeed = IsValid(BlockWorld) ? BlockWorld->GetWorldSeed() : ChunkData.GetGenerationSeeds().World;
	Context.GenerationVersion = FCubusGenerationSeeds::CurrentGenerationVersion;
	Context.VoxelSize = Actor.GetVoxelSize();
	Context.SubdivisionsPerVoxel = FCubusDensityLod::NormalizeSubdivisions(Actor.GetDensitySubdivisionsPerVoxel());
	return Context;
}
} // namespace CubusVoxelVolumeActorStorage

bool ACubusVoxelVolumeActor::TryLoadCachedChunk()
{
	EnsureChunkData();

	if (GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)
	{
		FCubusDensitySamplingBuffer LoadedBuffer;
		const FCubusDensityChunkStoreContext Context = CubusVoxelVolumeActorStorage::MakeDensityContext(*this, *ChunkData);

		if (!FCubusDensityChunkStore::LoadBuffer(ChunkCoordinate, Context, LoadedBuffer))
		{
			return false;
		}

		CachedGeneratedDensityBuffer =
			MakeShared<FCubusDensitySamplingBuffer, ESPMode::ThreadSafe>(MoveTemp(LoadedBuffer));
		bHasCachedGeneratedDensityBuffer = true;
		bChunkCacheDirty = false;
		return true;
	}

	if (!ChunkData.IsValid())
	{
		return false;
	}

	const FCubusChunkStoreContext Context = CubusVoxelVolumeActorStorage::MakeContext(*ChunkData);

	if (!FCubusChunkStore::HasChunk(ChunkCoordinate, Context))
	{
		return false;
	}

	const bool bLoaded = FCubusChunkStore::LoadChunk(*ChunkData, Context);

	if (bLoaded)
	{
		bChunkCacheDirty = false;

		UE_LOG(LogTemp, Verbose, TEXT("Cubus chunk cache hit (%d, %d, %d), seed %lld, generation %u"), ChunkCoordinate.X, ChunkCoordinate.Y,
			   ChunkCoordinate.Z, static_cast<long long>(Context.WorldSeed), Context.GenerationVersion);
	}

	return bLoaded;
}

bool ACubusVoxelVolumeActor::SaveCachedChunk() const
{
	if (!ChunkData.IsValid())
	{
		return false;
	}

	if (GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)
	{
		if (!bHasCachedGeneratedDensityBuffer || !CachedGeneratedDensityBuffer.IsValid() || !CachedGeneratedDensityBuffer->IsBuilt())
		{
			return true;
		}

		const FCubusDensityChunkStoreContext DensityContext = CubusVoxelVolumeActorStorage::MakeDensityContext(*this, *ChunkData);
		return FCubusDensityChunkStore::SaveBuffer(*CachedGeneratedDensityBuffer, DensityContext);
	}

	if (!bChunkCacheDirty)
	{
		return true;
	}

	const FCubusChunkStoreContext Context = CubusVoxelVolumeActorStorage::MakeContext(*ChunkData);

	const bool bSaved = FCubusChunkStore::SaveChunk(*ChunkData, Context);

	if (bSaved)
	{
		const_cast<ACubusVoxelVolumeActor*>(this)->bChunkCacheDirty = false;

		UE_LOG(LogTemp, Verbose, TEXT("Cubus chunk cache saved (%d, %d, %d), seed %lld, generation %u"), ChunkCoordinate.X,
			   ChunkCoordinate.Y, ChunkCoordinate.Z, static_cast<long long>(Context.WorldSeed), Context.GenerationVersion);
	}

	return bSaved;
}

void ACubusVoxelVolumeActor::RegenerateVegetationData()
{
	EnsureChunkData();

	if (!ChunkData.IsValid())
	{
		return;
	}

	const ACubusBlockWorldActor* BlockWorld = GetOwningBlockWorld();
	if (!bGenerateVegetationData || (IsValid(BlockWorld) && !BlockWorld->IsWorldVegetationEnabled()))
	{
		ChunkData->ClearVegetationInstances();
		return;
	}

	if (GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)
	{
		const FCubusTerrainDensitySettings DensitySettings = BuildDensitySettings();

		const FCubusTerrainDensityField DensityField(DensitySettings);

		FCubusBlockVegetationGenerator::Generate(*ChunkData, GeologyProfile, &DensityField, bGenerateWater, TerrainWaterLevel);

		FCubusTerrainClutterGenerator::Append(*ChunkData, MaterialRegistry, VoxelSize, &DensityField, bGenerateWater, TerrainWaterLevel);

		return;
	}

	FCubusBlockVegetationGenerator::Generate(*ChunkData, GeologyProfile);

	FCubusTerrainClutterGenerator::Append(*ChunkData, MaterialRegistry, VoxelSize);
}

void ACubusVoxelVolumeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(GetRootComponent()))
	{
		Mesh->SetVisibleInRayTracing(false);
		Mesh->MarkRenderStateDirty();
	}

	if (bChunkCacheDirty)
	{
		SaveCachedChunk();
	}

	Super::EndPlay(EndPlayReason);
}
