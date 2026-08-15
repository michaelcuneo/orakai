#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"
#include "CubusCore/Generation/CubusDensityEditField.h"
#include "CubusCore/Generation/CubusLandmarkField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusBlockMesher.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"
#include "CubusCore/Storage/CubusDensityChunkStore.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelVolumeActor
{
TAutoConsoleVariable<float> CVarCubusLod0ChunkBoundsScale(
	TEXT("cubus.Terrain.Lod0ChunkBoundsScale"), 1.5f,
	TEXT("Bounds scale multiplier for LOD0 chunk procedural meshes. Increase if chunk terrain culls too early."), ECVF_Default);

int32 WholeChunkOffset(const int32 VoxelOffset)
{
	return (VoxelOffset / Cubus::ChunkSize) * Cubus::ChunkSize;
}

int32 AppendMaterialMeshes(UProceduralMeshComponent& TargetMesh, const UCubusMaterialRegistry* MaterialRegistry,
						   FCubusMaterialMeshMap& MaterialMeshes, const bool bGenerateCollision, int32& InOutMeshSectionIndex,
						   int32& InOutVertexCount, int32& InOutTriangleCount)
{
	TArray<int32> MaterialIds;
	MaterialMeshes.GetKeys(MaterialIds);
	MaterialIds.Sort();

	const int32 FirstSectionIndex = InOutMeshSectionIndex;

	for (const int32 MaterialId : MaterialIds)
	{
		FCubusMeshData* MeshData = MaterialMeshes.Find(MaterialId);

		if (MeshData == nullptr || !MeshData->IsValid())
		{
			continue;
		}

		TargetMesh.CreateMeshSection_LinearColor(InOutMeshSectionIndex, MeshData->Vertices, MeshData->Triangles, MeshData->Normals,
												 MeshData->UV0, MeshData->VertexColors, MeshData->Tangents, bGenerateCollision);

		UMaterialInterface* ResolvedMaterial = nullptr;

		if (IsValid(MaterialRegistry))
		{
			ResolvedMaterial = MaterialRegistry->ResolveRuntimeMaterial(MaterialId);
		}

		if (!IsValid(ResolvedMaterial))
		{
			ResolvedMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		TargetMesh.SetMaterial(InOutMeshSectionIndex, ResolvedMaterial);

		InOutVertexCount += MeshData->GetVertexCount();

		InOutTriangleCount += MeshData->GetTriangleCount();

		++InOutMeshSectionIndex;
	}

	return InOutMeshSectionIndex - FirstSectionIndex;
}
} // namespace CubusVoxelVolumeActor

ACubusVoxelVolumeActor::ACubusVoxelVolumeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));

	SetRootComponent(ProceduralMesh);

	ProceduralMesh->bUseAsyncCooking	 = true;
	ProceduralMesh->bVisibleInRayTracing = false;
	ProceduralMesh->SetComponentTickEnabled(false);

	ProceduralMesh->SetCastShadow(true);
	ProceduralMesh->SetMobility(EComponentMobility::Static);

	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StagingProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StagingProceduralMesh"));

	StagingProceduralMesh->SetupAttachment(ProceduralMesh);

	StagingProceduralMesh->SetRelativeTransform(FTransform::Identity);

	StagingProceduralMesh->bUseAsyncCooking		= true;
	StagingProceduralMesh->bVisibleInRayTracing = false;
	StagingProceduralMesh->SetComponentTickEnabled(false);

	StagingProceduralMesh->SetCastShadow(true);
	StagingProceduralMesh->SetMobility(EComponentMobility::Static);

	StagingProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StagingProceduralMesh->SetVisibility(false);
	StagingProceduralMesh->SetHiddenInGame(true);
	StagingProceduralMesh->SetRenderInMainPass(false);
	StagingProceduralMesh->SetRenderInDepthPass(false);

	ActiveProceduralMesh   = ProceduralMesh;
	InactiveProceduralMesh = StagingProceduralMesh;
}

void ACubusVoxelVolumeActor::GenerateTerrainData()
{
	EnsureChunkData();
	ChunkData->Clear();

	if (GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)
	{
		bChunkCacheDirty = false;
		return;
	}

	bChunkCacheDirty = true;

	if (bUseHeightTerrain)
	{
		GenerateHeightTerrain();
	}
	else
	{
		GenerateFlatTerrain();
	}
}

void ACubusVoxelVolumeActor::RebuildVolume()
{
	++RebuildCount;
	EnsureChunkData();

	if (!IsValid(ActiveProceduralMesh))
	{
		ActiveProceduralMesh = ProceduralMesh;
	}

	if (!IsValid(ActiveProceduralMesh))
	{
		return;
	}

	bHasStagedVolume		 = false;
	bStagedBuildHadCollision = false;

	if (IsValid(InactiveProceduralMesh))
	{
		InactiveProceduralMesh->ClearAllMeshSections();
		InactiveProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	bLastBuildHadCollision = BuildVolumeInto(*ActiveProceduralMesh, true);
}

bool ACubusVoxelVolumeActor::BuildStagedVolume()
{
	EnsureChunkData();

	if (!IsValid(InactiveProceduralMesh))
	{
		return false;
	}

	bHasStagedVolume		 = false;
	bStagedBuildHadCollision = false;

	InactiveProceduralMesh->SetVisibility(false);
	InactiveProceduralMesh->SetHiddenInGame(true);

	InactiveProceduralMesh->SetRenderInMainPass(false);
	InactiveProceduralMesh->SetRenderInDepthPass(false);

	InactiveProceduralMesh->SetComponentTickEnabled(false);

	InactiveProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bStagedBuildHadCollision = BuildVolumeInto(*InactiveProceduralMesh, false);

	bHasStagedVolume = true;
	return true;
}

bool ACubusVoxelVolumeActor::BuildStagedVolumeFromDensityMesh(FCubusDensityMeshBuildResult& BuildResult)
{
	EnsureChunkData();

	if (!IsValid(InactiveProceduralMesh))
	{
		return false;
	}

	bHasStagedVolume		 = false;
	bStagedBuildHadCollision = false;

	InactiveProceduralMesh->SetVisibility(false);
	InactiveProceduralMesh->SetHiddenInGame(true);

	InactiveProceduralMesh->SetRenderInMainPass(false);
	InactiveProceduralMesh->SetRenderInDepthPass(false);

	InactiveProceduralMesh->SetComponentTickEnabled(false);

	InactiveProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/*
	 * The async path currently applies only to native Density mode.
	 *
	 * Blocks and Hybrid continue through the normal synchronous build path.
	 */
	if (GetEffectiveRenderMode() != ECubusVoxelRenderMode::Density)
	{
		return BuildStagedVolume();
	}

	bHasStagedVolume		 = false;
	bStagedBuildHadCollision = false;

	UProceduralMeshComponent& TargetMesh = *InactiveProceduralMesh;

	TargetMesh.BoundsScale = FMath::Max(1.0f, CubusVoxelVolumeActor::CVarCubusLod0ChunkBoundsScale.GetValueOnGameThread());

	TargetMesh.ClearAllMeshSections();

	TargetMesh.SetVisibility(false);
	TargetMesh.SetHiddenInGame(true);
	TargetMesh.SetRenderInMainPass(false);
	TargetMesh.SetRenderInDepthPass(false);

	TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (BuildResult.bHasGeneratedDensityBuffer && BuildResult.GeneratedDensityBuffer.IsBuilt())
	{
		CachedGeneratedDensityBuffer =
			MakeShared<FCubusDensitySamplingBuffer, ESPMode::ThreadSafe>(MoveTemp(BuildResult.GeneratedDensityBuffer));

		bHasCachedGeneratedDensityBuffer = true;

		BuildResult.bHasGeneratedDensityBuffer = false;
	}

	ResetDiagnostics();

	TotalVoxelCount = ChunkData->GetVoxelCount();

	SolidVoxelCount = ChunkData->GetOccupiedVoxelCount();

	LastBuiltRenderMode = ECubusVoxelRenderMode::Density;

	int32 MeshSectionIndex = 0;

	/*
	 * The expensive density geometry has already been produced on a worker.
	 *
	 * This function performs only the UObject/procedural-mesh upload and the
	 * block-edit overlay on the game thread.
	 */
	UploadDensityMesh(TargetMesh, BuildResult, bGenerateCollision, MeshSectionIndex);

	RebuildBlockEditOverlay(TargetMesh, bGenerateCollision, MeshSectionIndex);

	GeneratedMaterialSectionCount = MeshSectionIndex;

	bStagedBuildHadCollision = bGenerateCollision && (GeneratedDensitySectionCount > 0 || GeneratedBlockSectionCount > 0);

	/*
	 * Collision stays disabled until the atomic commit.
	 */
	TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TargetMesh.MarkRenderStateDirty();

	bHasStagedVolume = true;
	return true;
}

bool ACubusVoxelVolumeActor::BuildStagedVolumeFromBlockMesh(FCubusBlockMeshBuildResult& BuildResult)
{
	EnsureChunkData();
	if (!IsValid(InactiveProceduralMesh) || GetEffectiveRenderMode() != ECubusVoxelRenderMode::Blocks)
	{
		return false;
	}

	UProceduralMeshComponent& TargetMesh = *InactiveProceduralMesh;
	TargetMesh.BoundsScale				 = FMath::Max(1.0f, CubusVoxelVolumeActor::CVarCubusLod0ChunkBoundsScale.GetValueOnGameThread());
	TargetMesh.ClearAllMeshSections();
	TargetMesh.SetVisibility(false);
	TargetMesh.SetHiddenInGame(true);
	TargetMesh.SetRenderInMainPass(false);
	TargetMesh.SetRenderInDepthPass(false);
	TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ResetDiagnostics();
	TotalVoxelCount		= ChunkData->GetVoxelCount();
	SolidVoxelCount		= ChunkData->GetOccupiedVoxelCount();
	LastBuiltRenderMode = ECubusVoxelRenderMode::Blocks;
	GeneratedFaceCount	= BuildResult.GeneratedFaceCount;

	int32 MeshSectionIndex = 0;
	GeneratedBlockSectionCount =
		CubusVoxelVolumeActor::AppendMaterialMeshes(TargetMesh, MaterialRegistry.Get(), BuildResult.MaterialMeshes, bGenerateCollision,
													MeshSectionIndex, GeneratedVertexCount, GeneratedTriangleCount);
	GeneratedMaterialSectionCount = MeshSectionIndex;
	bStagedBuildHadCollision	  = bGenerateCollision && GeneratedBlockSectionCount > 0;
	TargetMesh.MarkRenderStateDirty();
	bHasStagedVolume = true;
	return true;
}

bool ACubusVoxelVolumeActor::BuildStagedVolumeFromHybridMesh(FCubusHybridMeshBuildResult& BuildResult)
{
	EnsureChunkData();
	if (!IsValid(InactiveProceduralMesh) || GetEffectiveRenderMode() != ECubusVoxelRenderMode::Hybrid)
	{
		return false;
	}

	UProceduralMeshComponent& TargetMesh = *InactiveProceduralMesh;
	TargetMesh.BoundsScale				 = FMath::Max(1.0f, CubusVoxelVolumeActor::CVarCubusLod0ChunkBoundsScale.GetValueOnGameThread());
	TargetMesh.ClearAllMeshSections();
	TargetMesh.SetVisibility(false);
	TargetMesh.SetHiddenInGame(true);
	TargetMesh.SetRenderInMainPass(false);
	TargetMesh.SetRenderInDepthPass(false);
	TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ResetDiagnostics();
	TotalVoxelCount		= ChunkData->GetVoxelCount();
	SolidVoxelCount		= ChunkData->GetOccupiedVoxelCount();
	LastBuiltRenderMode = ECubusVoxelRenderMode::Hybrid;
	GeneratedFaceCount	= BuildResult.Block.GeneratedFaceCount;

	int32 MeshSectionIndex = 0;
	GeneratedBlockSectionCount =
		CubusVoxelVolumeActor::AppendMaterialMeshes(TargetMesh, MaterialRegistry.Get(), BuildResult.Block.MaterialMeshes,
													bGenerateCollision, MeshSectionIndex, GeneratedVertexCount, GeneratedTriangleCount);
	UploadDensityMesh(TargetMesh, BuildResult.Density, false, MeshSectionIndex);
	GeneratedMaterialSectionCount = MeshSectionIndex;
	bStagedBuildHadCollision	  = bGenerateCollision && GeneratedBlockSectionCount > 0;
	TargetMesh.MarkRenderStateDirty();
	bHasStagedVolume = true;
	return true;
}

void ACubusVoxelVolumeActor::CommitStagedVolume()
{
	if (!bHasStagedVolume || !IsValid(ActiveProceduralMesh) || !IsValid(InactiveProceduralMesh))
	{
		return;
	}

	UProceduralMeshComponent* PreviousActive = ActiveProceduralMesh;

	UProceduralMeshComponent* NewActive = InactiveProceduralMesh;

	/*
	 * Publish the complete replacement first.
	 */
	NewActive->SetCollisionEnabled(bStagedBuildHadCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	NewActive->SetRenderInMainPass(true);
	NewActive->SetRenderInDepthPass(true);
	NewActive->SetHiddenInGame(false);
	NewActive->SetVisibility(true);

	/*
	 * Retire the previous revision.
	 */
	PreviousActive->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PreviousActive->SetVisibility(false);
	PreviousActive->SetHiddenInGame(true);
	PreviousActive->SetRenderInMainPass(false);
	PreviousActive->SetRenderInDepthPass(false);

	/*
	 * Swap front/back buffers.
	 */
	ActiveProceduralMesh = NewActive;

	InactiveProceduralMesh = PreviousActive;

	bLastBuildHadCollision = bStagedBuildHadCollision;

	bStagedBuildHadCollision = false;
	bHasStagedVolume		 = false;
}

void ACubusVoxelVolumeActor::DiscardStagedVolume()
{
	if (IsValid(InactiveProceduralMesh))
	{
		InactiveProceduralMesh->ClearAllMeshSections();

		InactiveProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		InactiveProceduralMesh->SetVisibility(false);
		InactiveProceduralMesh->SetHiddenInGame(true);
		InactiveProceduralMesh->SetRenderInMainPass(false);
		InactiveProceduralMesh->SetRenderInDepthPass(false);
	}

	bStagedBuildHadCollision = false;
	bHasStagedVolume		 = false;
}

bool ACubusVoxelVolumeActor::BuildVolumeInto(UProceduralMeshComponent& TargetMesh, const bool bPublishDiagnostics)
{
	const double BuildStartTime = FPlatformTime::Seconds();

	TargetMesh.BoundsScale = FMath::Max(1.0f, CubusVoxelVolumeActor::CVarCubusLod0ChunkBoundsScale.GetValueOnGameThread());

	TargetMesh.ClearAllMeshSections();

	TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ResetDiagnostics();

	TotalVoxelCount = ChunkData->GetVoxelCount();

	SolidVoxelCount = ChunkData->GetOccupiedVoxelCount();

	LastBuiltRenderMode = GetEffectiveRenderMode();

	int32 MeshSectionIndex = 0;
	bool  bBuiltCollision  = false;

	switch (LastBuiltRenderMode)
	{
	case ECubusVoxelRenderMode::Blocks:
	{
		RebuildBlockMesh(TargetMesh, bGenerateCollision, MeshSectionIndex);

		bBuiltCollision = bGenerateCollision && GeneratedBlockSectionCount > 0;

		break;
	}

	case ECubusVoxelRenderMode::Density:
	{
		RebuildDensityMesh(TargetMesh, bGenerateCollision, MeshSectionIndex);

		RebuildBlockEditOverlay(TargetMesh, bGenerateCollision, MeshSectionIndex);

		bBuiltCollision = bGenerateCollision && (GeneratedDensitySectionCount > 0 || GeneratedBlockSectionCount > 0);

		break;
	}

	case ECubusVoxelRenderMode::Hybrid:
	{
		RebuildBlockMesh(TargetMesh, bGenerateCollision, MeshSectionIndex);

		RebuildDensityMesh(TargetMesh, false, MeshSectionIndex);

		bBuiltCollision = bGenerateCollision && GeneratedBlockSectionCount > 0;

		break;
	}

	default:
	{
		checkNoEntry();
		break;
	}
	}

	GeneratedMaterialSectionCount = MeshSectionIndex;

	/*
	 * Visible builds may activate collision immediately.
	 * Staged builds remain collision-disabled until CommitStagedVolume().
	 */
	if (bPublishDiagnostics)
	{
		TargetMesh.SetCollisionEnabled(bBuiltCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

		TargetMesh.SetVisibility(true);
		TargetMesh.SetHiddenInGame(false);
		TargetMesh.SetRenderInMainPass(true);
		TargetMesh.SetRenderInDepthPass(true);
	}
	else
	{
		TargetMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);

		TargetMesh.SetVisibility(false);
		TargetMesh.SetHiddenInGame(true);
		TargetMesh.SetRenderInMainPass(false);
		TargetMesh.SetRenderInDepthPass(false);
	}

	LastBuildTimeMilliseconds = static_cast<float>((FPlatformTime::Seconds() - BuildStartTime) * 1000.0);

	TargetMesh.MarkRenderStateDirty();

	if (bPublishDiagnostics)
	{
		UE_LOG(LogTemp, Verbose,
			   TEXT("Cubus chunk (%d, %d, %d) built mode=%d "
					"densityStep=%.1fcm rootSections=%d blockSections=%d "
					"densitySections=%d vertices=%d triangles=%d "
					"densityTriangles=%d collision=%s time=%.2fms"),
			   ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, static_cast<int32>(LastBuiltRenderMode), GetDensitySampleSpacing(),
			   GeneratedMaterialSectionCount, GeneratedBlockSectionCount, GeneratedDensitySectionCount, GeneratedVertexCount,
			   GeneratedTriangleCount, GeneratedDensityTriangleCount, bBuiltCollision ? TEXT("true") : TEXT("false"),
			   LastBuildTimeMilliseconds);
	}

	return bBuiltCollision;
}

ECubusVoxelRenderMode ACubusVoxelVolumeActor::GetEffectiveRenderMode() const
{
	if (IsValid(OwningBlockWorld.Get()))
	{
		return OwningBlockWorld->GetVoxelRenderMode();
	}

	return StandaloneRenderMode;
}

void ACubusVoxelVolumeActor::RebuildBlockMesh(UProceduralMeshComponent& TargetMesh, const bool bGenerateBlockCollision,
											  int32& InOutMeshSectionIndex)
{
	FCubusMaterialMeshMap			   MaterialMeshes;
	const FCubusBlockChunkNeighborhood Neighborhood = BuildNeighborhood();

	FCubusBlockMesher::BuildChunk(Neighborhood, MaterialRegistry.Get(), VoxelSize, MaterialMeshes, GeneratedFaceCount);

	GeneratedBlockSectionCount =
		CubusVoxelVolumeActor::AppendMaterialMeshes(TargetMesh, MaterialRegistry.Get(), MaterialMeshes, bGenerateBlockCollision,
													InOutMeshSectionIndex, GeneratedVertexCount, GeneratedTriangleCount);
}

void ACubusVoxelVolumeActor::RebuildBlockEditOverlay(UProceduralMeshComponent& TargetMesh, const bool bGenerateBlockCollision,
													 int32& InOutMeshSectionIndex)
{
	ACubusBlockWorldActor* BlockWorld = OwningBlockWorld.Get();
	if (!IsValid(BlockWorld))
	{
		return;
	}

	TUniquePtr<FCubusBlockChunkData> Centre = MakeUnique<FCubusBlockChunkData>(ChunkCoordinate);

	if (!BlockWorld->BuildBlockEditOverlayChunk(ChunkCoordinate, *Centre))
	{
		return;
	}

	auto BuildNeighbour = [BlockWorld, this](const FIntVector& Offset) -> TUniquePtr<FCubusBlockChunkData>
	{
		const FIntVector				 Coordinate = ChunkCoordinate + Offset;
		TUniquePtr<FCubusBlockChunkData> Result		= MakeUnique<FCubusBlockChunkData>(Coordinate);

		if (!BlockWorld->BuildBlockEditOverlayChunk(Coordinate, *Result))
		{
			return nullptr;
		}
		return Result;
	};

	TUniquePtr<FCubusBlockChunkData> PositiveX = BuildNeighbour(FIntVector(1, 0, 0));
	TUniquePtr<FCubusBlockChunkData> NegativeX = BuildNeighbour(FIntVector(-1, 0, 0));
	TUniquePtr<FCubusBlockChunkData> PositiveY = BuildNeighbour(FIntVector(0, 1, 0));
	TUniquePtr<FCubusBlockChunkData> NegativeY = BuildNeighbour(FIntVector(0, -1, 0));
	TUniquePtr<FCubusBlockChunkData> PositiveZ = BuildNeighbour(FIntVector(0, 0, 1));
	TUniquePtr<FCubusBlockChunkData> NegativeZ = BuildNeighbour(FIntVector(0, 0, -1));

	FCubusBlockChunkNeighborhood Neighborhood;
	Neighborhood.Centre	   = Centre.Get();
	Neighborhood.PositiveX = PositiveX.Get();
	Neighborhood.NegativeX = NegativeX.Get();
	Neighborhood.PositiveY = PositiveY.Get();
	Neighborhood.NegativeY = NegativeY.Get();
	Neighborhood.PositiveZ = PositiveZ.Get();
	Neighborhood.NegativeZ = NegativeZ.Get();

	FCubusMaterialMeshMap MaterialMeshes;
	int32				  OverlayFaceCount = 0;
	FCubusBlockMesher::BuildChunk(Neighborhood, MaterialRegistry.Get(), VoxelSize, MaterialMeshes, OverlayFaceCount);

	GeneratedFaceCount += OverlayFaceCount;
	GeneratedBlockSectionCount +=
		CubusVoxelVolumeActor::AppendMaterialMeshes(TargetMesh, MaterialRegistry.Get(), MaterialMeshes, bGenerateBlockCollision,
													InOutMeshSectionIndex, GeneratedVertexCount, GeneratedTriangleCount);
}

FCubusTerrainDensitySettings ACubusVoxelVolumeActor::BuildDensitySettings() const
{
	FCubusTerrainDensitySettings DensitySettings;

	DensitySettings.bUseHeightTerrain = bUseHeightTerrain;

	DensitySettings.FlatSurfaceWorldZ = static_cast<float>(TerrainSurfaceWorldZ);

	DensitySettings.BaseHeight = static_cast<float>(TerrainBaseHeight);

	DensitySettings.VoxelSizeCm = VoxelSize;

	DensitySettings.ContinentAmplitude = TerrainContinentAmplitude;

	DensitySettings.ContinentFrequency = TerrainContinentFrequency;

	DensitySettings.HillAmplitude = TerrainHillAmplitude;

	DensitySettings.HillFrequency = TerrainHillFrequency;

	DensitySettings.DetailAmplitude = TerrainDetailAmplitude;

	DensitySettings.DetailFrequency = TerrainDetailFrequency;

	DensitySettings.RidgeAmplitude = TerrainRidgeAmplitude;

	DensitySettings.RidgeFrequency = TerrainRidgeFrequency;

	DensitySettings.ValleyDepth = TerrainValleyDepth;

	DensitySettings.ValleyFrequency = TerrainValleyFrequency;

	DensitySettings.ValleyWidth = TerrainValleyWidth;

	DensitySettings.ValleyFalloff = TerrainValleyFalloff;

	DensitySettings.ValleyWarpAmplitude = TerrainValleyWarpAmplitude;

	DensitySettings.ValleyWarpFrequency = TerrainValleyWarpFrequency;

	DensitySettings.RegionFrequency = TerrainRegionFrequency;

	DensitySettings.PlainsThreshold = TerrainPlainsThreshold;

	DensitySettings.PlainsBlend = TerrainPlainsBlend;

	DensitySettings.MountainThreshold = TerrainMountainThreshold;

	DensitySettings.MountainBlend = TerrainMountainBlend;

	DensitySettings.SurfaceMaterialId = TerrainSurfaceMaterialId;

	DensitySettings.SubsurfaceMaterialId = TerrainSubsurfaceMaterialId;

	DensitySettings.RockMaterialId = TerrainRockMaterialId;

	DensitySettings.SnowMaterialId		= TerrainSnowMaterialId;
	DensitySettings.BiomeSnowMaterialId = TerrainSnowMaterialId;

	DensitySettings.RockSlopeThreshold = TerrainRockSlopeThreshold;

	DensitySettings.SnowMinimumHeight	   = static_cast<float>(TerrainSnowMinimumHeight);
	DensitySettings.BiomeSnowMinimumHeight = static_cast<float>(TerrainSnowMinimumHeight);

	const FCubusGenerationSeeds& Seeds = ChunkData->GetGenerationSeeds();

	DensitySettings.TerrainOffsetX = CubusVoxelVolumeActor::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetX(Seeds.Terrain));

	DensitySettings.TerrainOffsetY = CubusVoxelVolumeActor::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetY(Seeds.Terrain));

	DensitySettings.RiverOffsetX = FCubusGenerationSeeds::DomainOffsetX(Seeds.Rivers);

	DensitySettings.RiverOffsetY = FCubusGenerationSeeds::DomainOffsetY(Seeds.Rivers);

	DensitySettings.CaveOffsetX = FCubusGenerationSeeds::DomainOffsetX(Seeds.Caves);

	DensitySettings.CaveOffsetY = FCubusGenerationSeeds::DomainOffsetY(Seeds.Caves);

	DensitySettings.CaveOffsetZ = FCubusGenerationSeeds::DomainOffsetZ(Seeds.Caves);

	DensitySettings.BiomeSettings			  = FCubusBiomeField::MakeSettings(GeologyProfile.Get(), Seeds.Biomes, Seeds.Rivers);
	DensitySettings.BiomeSettings.NivalWorldZ = FMath::Max(
		DensitySettings.BiomeSnowMinimumHeight, DensitySettings.BaseHeight + FMath::Max(64.0f, DensitySettings.RidgeAmplitude * 4.0f));

	DensitySettings.LandmarkSettings = FCubusLandmarkField::MakeSettings(GeologyProfile.Get(), Seeds.Terrain);

	if (IsValid(GeologyProfile.Get()))
	{
		DensitySettings.bGenerateRivers = GeologyProfile->bGenerateRivers;

		DensitySettings.RiverFrequency = GeologyProfile->RiverFrequency;

		DensitySettings.RiverChannelWidth = GeologyProfile->RiverChannelWidth;

		DensitySettings.RiverValleyWidth = GeologyProfile->RiverValleyWidth;

		DensitySettings.RiverValleyDepth = GeologyProfile->RiverValleyDepth;

		DensitySettings.RiverChannelDepth = static_cast<float>(GeologyProfile->RiverChannelDepth);

		DensitySettings.RiverWarpAmplitude = GeologyProfile->RiverWarpAmplitude;

		DensitySettings.RiverWarpFrequency = GeologyProfile->RiverWarpFrequency;

		DensitySettings.bGenerateCaves = GeologyProfile->bGenerateCaves;

		DensitySettings.CaveMinimumWorldZ = GeologyProfile->CaveMinimumWorldZ;

		DensitySettings.CaveMaximumWorldZ = GeologyProfile->CaveMaximumWorldZ;

		DensitySettings.CaveSurfaceClearance = GeologyProfile->CaveSurfaceClearance;

		DensitySettings.CavePrimaryFrequency = GeologyProfile->CavePrimaryFrequency;

		DensitySettings.CaveSecondaryFrequency = GeologyProfile->CaveSecondaryFrequency;

		DensitySettings.CaveThreshold = GeologyProfile->CaveThreshold;
	}

	return DensitySettings;
}

FCubusDensityMeshBuildInput ACubusVoxelVolumeActor::CaptureDensityMeshBuildInput() const
{
	FCubusDensityMeshBuildInput Input;

	Input.DensitySettings = BuildDensitySettings();

	if (IsValid(OwningBlockWorld.Get()))
	{
		Input.DensityEdits = OwningBlockWorld->BuildDensityEditSnapshot(ChunkCoordinate);
	}

	Input.ChunkCoordinate = ChunkCoordinate;

	Input.VoxelSize = VoxelSize;

	Input.SubdivisionsPerVoxel = DensitySubdivisionsPerVoxel;

	if (IsValid(OwningBlockWorld.Get()))
	{
		Input.TransitionFaces = OwningBlockWorld->BuildDensityTransitionFaces(ChunkCoordinate, Input.SubdivisionsPerVoxel);
	}

	Input.IsoLevel = 0.0f;

	if (IsValid(OwningBlockWorld.Get()))
	{
		Input.WorldSeed = OwningBlockWorld->GetWorldSeed();
	}
	else
	{
		Input.WorldSeed = ChunkData->GetGenerationSeeds().World;
	}
	Input.GenerationVersion = FCubusGenerationSeeds::CurrentGenerationVersion;

	if (bHasCachedGeneratedDensityBuffer && CachedGeneratedDensityBuffer.IsValid() && CachedGeneratedDensityBuffer->IsBuilt())
	{
		/*
		 * Cheap thread-safe shared-pointer copy only.
		 *
		 * No FCubusDensitySamplingBuffer sample array is copied here.
		 */
		Input.GeneratedDensityBuffer = CachedGeneratedDensityBuffer;

		Input.bHasGeneratedDensityBuffer = true;
	}

	return Input;
}

bool ACubusVoxelVolumeActor::IsDensitySurfaceExpected() const
{
	if (GetEffectiveRenderMode() == ECubusVoxelRenderMode::Blocks || !ChunkData.IsValid())
	{
		return false;
	}

	const FCubusTerrainDensityField DensityField(BuildDensitySettings());
	const FIntVector				Base = ChunkCoordinate * Cubus::ChunkSize;
	for (const int32 LocalY : {0, Cubus::ChunkSize / 2, Cubus::ChunkSize - 1})
	{
		for (const int32 LocalX : {0, Cubus::ChunkSize / 2, Cubus::ChunkSize - 1})
		{
			const float SurfaceZ =
				DensityField.SampleSurfaceVoxelHeight(static_cast<float>(Base.X + LocalX), static_cast<float>(Base.Y + LocalY));
			if (SurfaceZ >= static_cast<float>(Base.Z - 1) && SurfaceZ <= static_cast<float>(Base.Z + Cubus::ChunkSize))
			{
				return true;
			}
		}
	}

	return false;
}

FCubusBlockMeshBuildInput ACubusVoxelVolumeActor::CaptureBlockMeshBuildInput() const
{
	FCubusBlockMeshBuildInput Input;
	Input.VoxelSize = VoxelSize;

	auto CaptureChunk = [](const FCubusBlockChunkData* Source) -> TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe>
	{
		if (Source == nullptr)
		{
			return nullptr;
		}

		TSharedPtr<FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe>();
		Snapshot->ChunkCoordinate = Source->GetChunkCoordinate();
		Snapshot->Voxels.Append(Source->GetVoxelView());
		return Snapshot;
	};

	Input.Neighborhood.Centre	 = CaptureChunk(ChunkData.Get());
	Input.Neighborhood.PositiveX = CaptureChunk(FindNeighbourChunkData(FIntVector(1, 0, 0)));
	Input.Neighborhood.NegativeX = CaptureChunk(FindNeighbourChunkData(FIntVector(-1, 0, 0)));
	Input.Neighborhood.PositiveY = CaptureChunk(FindNeighbourChunkData(FIntVector(0, 1, 0)));
	Input.Neighborhood.NegativeY = CaptureChunk(FindNeighbourChunkData(FIntVector(0, -1, 0)));
	Input.Neighborhood.PositiveZ = CaptureChunk(FindNeighbourChunkData(FIntVector(0, 0, 1)));
	Input.Neighborhood.NegativeZ = CaptureChunk(FindNeighbourChunkData(FIntVector(0, 0, -1)));

	if (IsValid(MaterialRegistry.Get()))
	{
		for (const FCubusMaterialDefinition& Definition : MaterialRegistry->Materials)
		{
			if (Definition.bRenderable && Definition.IsSolid())
			{
				Input.Materials.RenderableSolidMaterialIds.Add(Definition.MaterialId);
			}
		}
	}
	else
	{
		for (int32 MaterialId = 1; MaterialId <= MAX_uint16; ++MaterialId)
		{
			Input.Materials.RenderableSolidMaterialIds.Add(MaterialId);
		}
	}

	return Input;
}

FCubusDensityMeshBuildResult ACubusVoxelVolumeActor::BuildDensityMeshData(const FCubusDensityMeshBuildInput& Input)
{
	const double BuildStartTime = FPlatformTime::Seconds();

	FCubusDensityMeshBuildResult Result;

	const FCubusTerrainDensityField DensityField(Input.DensitySettings);

	const int32					   Subdivisions = FCubusDensityLod::NormalizeSubdivisions(Input.SubdivisionsPerVoxel);
	const FCubusDensityEditField   EditedDensityField(DensityField, Input.DensityEdits);
	FCubusDensityChunkStoreContext MeshCacheContext;
	MeshCacheContext.WorldSeed			  = Input.WorldSeed;
	MeshCacheContext.GenerationVersion	  = Input.GenerationVersion;
	MeshCacheContext.VoxelSize			  = Input.VoxelSize;
	MeshCacheContext.SubdivisionsPerVoxel = Subdivisions;
	const uint32 TransitionSignature	  = Input.TransitionFaces.GetSignature(Subdivisions);
	const bool	 bCanUseMeshCache		  = Input.bUseDiskDensityCache && Input.DensityEdits.IsEmpty();

	if (bCanUseMeshCache && FCubusDensityChunkStore::LoadMesh(Input.ChunkCoordinate, MeshCacheContext, TransitionSignature,
															  Result.MaterialMeshes, Result.GeneratedTriangleCount))
	{
		Result.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;
		return Result;
	}

	if (Subdivisions <= 1)
	{
		FCubusDensitySamplingBuffer DensityBuffer;

		if (Input.bHasGeneratedDensityBuffer && Input.GeneratedDensityBuffer.IsValid() && Input.GeneratedDensityBuffer->IsBuilt())
		{
			/*
			 * One intentional copy remains here.
			 *
			 * This worker-local buffer is the only copy that receives player edits.
			 * The shared generated baseline stays permanently immutable.
			 */
			DensityBuffer = *Input.GeneratedDensityBuffer;
		}
		else
		{
			FCubusDensityChunkStoreContext CacheContext;
			CacheContext.WorldSeed			  = Input.WorldSeed;
			CacheContext.GenerationVersion	  = Input.GenerationVersion;
			CacheContext.VoxelSize			  = Input.VoxelSize;
			CacheContext.SubdivisionsPerVoxel = Subdivisions;

			const bool bLoadedDiskBaseline =
				Input.bUseDiskDensityCache && FCubusDensityChunkStore::LoadBuffer(Input.ChunkCoordinate, CacheContext, DensityBuffer);

			if (!bLoadedDiskBaseline)
			{
				/* Generate the expensive edit-free terrain baseline exactly once. */
				DensityBuffer.Build(Input.ChunkCoordinate, DensityField);

				/* Streaming workers write unique chunk files, keeping disk I/O off the game thread. */
				if (Input.bUseDiskDensityCache)
				{
					FCubusDensityChunkStore::SaveBuffer(DensityBuffer, CacheContext);
				}
			}

			/*
			 * Whether generated or loaded, return the immutable baseline to the actor.
			 * Later density edits copy and mutate this baseline only in worker-local memory.
			 */
			Result.GeneratedDensityBuffer	  = DensityBuffer;
			Result.bHasGeneratedDensityBuffer = true;
		}

		/*
		 * Mutate only this worker-local copy.
		 *
		 * The actor's generated baseline always remains edit-free.
		 */
		DensityBuffer.ApplyEdits(Input.DensityEdits);

		FCubusDensityMesher::BuildChunk(DensityBuffer, Input.VoxelSize, Input.IsoLevel, Result.MaterialMeshes,
										Result.GeneratedTriangleCount, nullptr, &EditedDensityField, Input.TransitionFaces);
	}
	else
	{
		FCubusDensityMesher::BuildAdaptiveChunk(EditedDensityField, Input.ChunkCoordinate, Input.VoxelSize, Subdivisions, Input.IsoLevel,
												Result.MaterialMeshes, Result.GeneratedTriangleCount, Input.TransitionFaces);
	}

	Result.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;
	if (bCanUseMeshCache)
	{
		FCubusDensityChunkStore::SaveMesh(Input.ChunkCoordinate, MeshCacheContext, TransitionSignature, Result.MaterialMeshes,
										  Result.GeneratedTriangleCount);
	}

	return Result;
}

FCubusBlockMeshBuildResult ACubusVoxelVolumeActor::BuildBlockMeshData(const FCubusBlockMeshBuildInput& Input)
{
	const double			   BuildStartTime = FPlatformTime::Seconds();
	FCubusBlockMeshBuildResult Result;
	FCubusBlockMesher::BuildChunk(Input.Neighborhood, Input.Materials, Input.VoxelSize, Result.MaterialMeshes, Result.GeneratedFaceCount);
	Result.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;
	return Result;
}

FCubusHybridMeshBuildResult ACubusVoxelVolumeActor::BuildHybridMeshData(const FCubusHybridMeshBuildInput& Input)
{
	FCubusHybridMeshBuildResult Result;
	Result.Block   = BuildBlockMeshData(Input.Block);
	Result.Density = BuildDensityMeshData(Input.Density);
	return Result;
}

void ACubusVoxelVolumeActor::UploadDensityMesh(UProceduralMeshComponent& TargetMesh, FCubusDensityMeshBuildResult& BuildResult,
											   const bool bGenerateDensityCollision, int32& InOutMeshSectionIndex)
{
	GeneratedDensityTriangleCount = BuildResult.GeneratedTriangleCount;

	GeneratedDensitySectionCount = CubusVoxelVolumeActor::AppendMaterialMeshes(
		TargetMesh, MaterialRegistry.Get(), BuildResult.MaterialMeshes, bGenerateDensityCollision, InOutMeshSectionIndex,
		GeneratedVertexCount, GeneratedTriangleCount);

	if (GeneratedDensitySectionCount <= 0)
	{
		UE_LOG(LogTemp, Verbose,
			   TEXT("Cubus native density chunk (%d, %d, %d) "
					"contains no isosurface."),
			   ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
	}
}

void ACubusVoxelVolumeActor::RebuildDensityMesh(UProceduralMeshComponent& TargetMesh, const bool bGenerateDensityCollision,
												int32& InOutMeshSectionIndex)
{
	const FCubusDensityMeshBuildInput BuildInput = CaptureDensityMeshBuildInput();

	FCubusDensityMeshBuildResult BuildResult = BuildDensityMeshData(BuildInput);

	if (BuildResult.bHasGeneratedDensityBuffer && BuildResult.GeneratedDensityBuffer.IsBuilt())
	{
		CachedGeneratedDensityBuffer =
			MakeShared<FCubusDensitySamplingBuffer, ESPMode::ThreadSafe>(MoveTemp(BuildResult.GeneratedDensityBuffer));

		bHasCachedGeneratedDensityBuffer = true;

		BuildResult.bHasGeneratedDensityBuffer = false;
	}

	UploadDensityMesh(TargetMesh, BuildResult, bGenerateDensityCollision, InOutMeshSectionIndex);
}

void ACubusVoxelVolumeActor::EnsureChunkData()
{
	if (!ChunkData.IsValid())
	{
		ChunkData = MakeUnique<FCubusBlockChunkData>(ChunkCoordinate);
	}
	else
	{
		ChunkData->SetChunkCoordinate(ChunkCoordinate);
	}
}

void ACubusVoxelVolumeActor::SynchronizeChunkState()
{
	EnsureChunkData();
	ChunkData->SetChunkCoordinate(ChunkCoordinate);

	const double ChunkWorldSize = static_cast<double>(Cubus::ChunkSize) * static_cast<double>(VoxelSize);

	SetActorLocation(FVector(static_cast<double>(ChunkCoordinate.X) * ChunkWorldSize,
							 static_cast<double>(ChunkCoordinate.Y) * ChunkWorldSize,
							 static_cast<double>(ChunkCoordinate.Z) * ChunkWorldSize));
}

void ACubusVoxelVolumeActor::ConfigureGeneratedChunk(const FIntVector& InChunkCoordinate, const float InVoxelSize,
													 ACubusBlockWorldActor* InBlockWorld)
{
	InvalidateGeneratedDensityCache();

	ChunkCoordinate	 = InChunkCoordinate;
	VoxelSize		 = FMath::Max(1.0f, InVoxelSize);
	OwningBlockWorld = InBlockWorld;

	SynchronizeChunkState();
}

bool ACubusVoxelVolumeActor::ConfigureDensityResolution(const int32 InSubdivisionsPerVoxel)
{
	const int32 ResolvedSubdivisions = FCubusDensityLod::NormalizeSubdivisions(InSubdivisionsPerVoxel);

	if (DensitySubdivisionsPerVoxel == ResolvedSubdivisions)
	{
		return false;
	}

	DensitySubdivisionsPerVoxel = ResolvedSubdivisions;
	return true;
}

void ACubusVoxelVolumeActor::ConfigureRendering(UCubusMaterialRegistry* InMaterialRegistry)
{
	MaterialRegistry = InMaterialRegistry;
}

void ACubusVoxelVolumeActor::ConfigureGeology(UCubusGeologyProfile* InGeologyProfile)
{
	if (GeologyProfile == InGeologyProfile)
	{
		return;
	}

	GeologyProfile = InGeologyProfile;

	InvalidateGeneratedDensityCache();
}

void ACubusVoxelVolumeActor::ConfigureTerrain(
	const bool bInUseHeightTerrain, const int32 InTerrainSurfaceWorldZ, const int32 InTerrainBaseHeight,
	const float InTerrainContinentAmplitude, const float InTerrainContinentFrequency, const float InTerrainHillAmplitude,
	const float InTerrainHillFrequency, const float InTerrainDetailAmplitude, const float InTerrainDetailFrequency,
	const float InTerrainRidgeAmplitude, const float InTerrainRidgeFrequency, const float InTerrainValleyDepth,
	const float InTerrainValleyFrequency, const float InTerrainValleyWidth, const float InTerrainValleyFalloff,
	const float InTerrainValleyWarpAmplitude, const float InTerrainValleyWarpFrequency, const float InTerrainRegionFrequency,
	const float InTerrainPlainsThreshold, const float InTerrainPlainsBlend, const float InTerrainMountainThreshold,
	const float InTerrainMountainBlend, const int32 InTerrainSurfaceMaterialId, const int32 InTerrainSubsurfaceMaterialId,
	const int32 InTerrainRockMaterialId, const int32 InTerrainSnowMaterialId, const float InTerrainRockSlopeThreshold,
	const int32 InTerrainSnowMinimumHeight, const bool bInGenerateWater, const int32 InTerrainWaterLevel,
	const int32 InTerrainWaterMaterialId)
{
	InvalidateGeneratedDensityCache();

	bUseHeightTerrain	 = bInUseHeightTerrain;
	TerrainSurfaceWorldZ = InTerrainSurfaceWorldZ;
	TerrainBaseHeight	 = InTerrainBaseHeight;

	TerrainContinentAmplitude = FMath::Max(0.0f, InTerrainContinentAmplitude);

	TerrainContinentFrequency = FMath::Max(0.000001f, InTerrainContinentFrequency);

	TerrainHillAmplitude = FMath::Max(0.0f, InTerrainHillAmplitude);

	TerrainHillFrequency = FMath::Max(0.000001f, InTerrainHillFrequency);

	TerrainDetailAmplitude = FMath::Max(0.0f, InTerrainDetailAmplitude);

	TerrainDetailFrequency = FMath::Max(0.000001f, InTerrainDetailFrequency);

	TerrainRidgeAmplitude = FMath::Max(0.0f, InTerrainRidgeAmplitude);

	TerrainRidgeFrequency = FMath::Max(0.000001f, InTerrainRidgeFrequency);

	TerrainValleyDepth = FMath::Max(0.0f, InTerrainValleyDepth);

	TerrainValleyFrequency = FMath::Max(0.000001f, InTerrainValleyFrequency);

	TerrainValleyWidth = FMath::Clamp(InTerrainValleyWidth, 0.0f, 1.0f);

	TerrainValleyFalloff = FMath::Clamp(InTerrainValleyFalloff, 0.001f, 1.0f);

	TerrainValleyWarpAmplitude = FMath::Max(0.0f, InTerrainValleyWarpAmplitude);

	TerrainValleyWarpFrequency = FMath::Max(0.000001f, InTerrainValleyWarpFrequency);

	TerrainRegionFrequency = FMath::Max(0.000001f, InTerrainRegionFrequency);

	TerrainPlainsThreshold = FMath::Clamp(InTerrainPlainsThreshold, -1.0f, 1.0f);

	TerrainPlainsBlend = FMath::Clamp(InTerrainPlainsBlend, 0.001f, 1.0f);

	TerrainMountainThreshold = FMath::Clamp(InTerrainMountainThreshold, TerrainPlainsThreshold, 1.0f);

	TerrainMountainBlend = FMath::Clamp(InTerrainMountainBlend, 0.001f, 1.0f);

	TerrainSurfaceMaterialId = FMath::Max(1, InTerrainSurfaceMaterialId);

	TerrainSubsurfaceMaterialId = FMath::Max(1, InTerrainSubsurfaceMaterialId);

	TerrainRockMaterialId = FMath::Max(1, InTerrainRockMaterialId);

	TerrainSnowMaterialId = FMath::Max(1, InTerrainSnowMaterialId);

	TerrainRockSlopeThreshold = FMath::Max(0.0f, InTerrainRockSlopeThreshold);

	TerrainSnowMinimumHeight = InTerrainSnowMinimumHeight;

	bGenerateWater	  = bInGenerateWater;
	TerrainWaterLevel = InTerrainWaterLevel;

	TerrainWaterMaterialId = FMath::Max(1, InTerrainWaterMaterialId);
}

const FCubusBlockChunkData* ACubusVoxelVolumeActor::FindNeighbourChunkData(const FIntVector& CoordinateOffset) const
{
	if (!IsValid(OwningBlockWorld.Get()))
	{
		return nullptr;
	}

	ACubusVoxelVolumeActor* NeighbourActor = OwningBlockWorld->FindChunk(ChunkCoordinate + CoordinateOffset);

	if (!IsValid(NeighbourActor))
	{
		return nullptr;
	}

	return NeighbourActor->GetChunkData();
}

FCubusBlockChunkNeighborhood ACubusVoxelVolumeActor::BuildNeighborhood() const
{
	FCubusBlockChunkNeighborhood Neighborhood;
	Neighborhood.Centre	   = ChunkData.Get();
	Neighborhood.PositiveX = FindNeighbourChunkData(FIntVector(1, 0, 0));
	Neighborhood.NegativeX = FindNeighbourChunkData(FIntVector(-1, 0, 0));
	Neighborhood.PositiveY = FindNeighbourChunkData(FIntVector(0, 1, 0));
	Neighborhood.NegativeY = FindNeighbourChunkData(FIntVector(0, -1, 0));
	Neighborhood.PositiveZ = FindNeighbourChunkData(FIntVector(0, 0, 1));
	Neighborhood.NegativeZ = FindNeighbourChunkData(FIntVector(0, 0, -1));
	return Neighborhood;
}

void ACubusVoxelVolumeActor::RebuildAffectedChunks()
{
	if (IsValid(OwningBlockWorld.Get()))
	{
		OwningBlockWorld->RebuildChunkAndNeighbours(ChunkCoordinate);
		return;
	}

	RebuildVolume();
}

void ACubusVoxelVolumeActor::GenerateFlatTerrain()
{
	FCubusBlockTerrainGenerator::GenerateFlatTerrain(*ChunkData, TerrainSurfaceWorldZ, TerrainSurfaceMaterialId,
													 TerrainSubsurfaceMaterialId);
}

void ACubusVoxelVolumeActor::GenerateHeightTerrain()
{
	FCubusBlockTerrainGenerator::GenerateHeightTerrain(
		*ChunkData, TerrainBaseHeight, TerrainContinentAmplitude, TerrainContinentFrequency, TerrainHillAmplitude, TerrainHillFrequency,
		TerrainDetailAmplitude, TerrainDetailFrequency, TerrainRidgeAmplitude, TerrainRidgeFrequency, TerrainValleyDepth,
		TerrainValleyFrequency, TerrainValleyWidth, TerrainValleyFalloff, TerrainValleyWarpAmplitude, TerrainValleyWarpFrequency,
		TerrainRegionFrequency, TerrainPlainsThreshold, TerrainPlainsBlend, TerrainMountainThreshold, TerrainMountainBlend,
		TerrainSurfaceMaterialId, TerrainSubsurfaceMaterialId, TerrainRockMaterialId, TerrainSnowMaterialId, TerrainRockSlopeThreshold,
		TerrainSnowMinimumHeight, bGenerateWater, TerrainWaterLevel, TerrainWaterMaterialId, GeologyProfile.Get());
}

void ACubusVoxelVolumeActor::ResetDiagnostics()
{
	TotalVoxelCount				  = 0;
	SolidVoxelCount				  = 0;
	GeneratedFaceCount			  = 0;
	GeneratedVertexCount		  = 0;
	GeneratedTriangleCount		  = 0;
	GeneratedMaterialSectionCount = 0;
	GeneratedBlockSectionCount	  = 0;
	GeneratedDensitySectionCount  = 0;
	GeneratedDensityTriangleCount = 0;
	LastBuildTimeMilliseconds	  = 0.0f;
}
