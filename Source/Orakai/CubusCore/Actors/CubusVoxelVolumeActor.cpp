#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"
#include "CubusCore/Generation/CubusDensityEditField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusBlockMesher.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelVolumeActor
{
    int32 WholeChunkOffset(const int32 VoxelOffset)
    {
        return
            (VoxelOffset / Cubus::ChunkSize) *
            Cubus::ChunkSize;
    }

    int32 AppendMaterialMeshes(
        UProceduralMeshComponent& TargetMesh,
        const UCubusMaterialRegistry* MaterialRegistry,
        FCubusMaterialMeshMap& MaterialMeshes,
        const bool bGenerateCollision,
        int32& InOutMeshSectionIndex,
        int32& InOutVertexCount,
        int32& InOutTriangleCount
    )
    {
        TArray<int32> MaterialIds;
        MaterialMeshes.GetKeys(MaterialIds);
        MaterialIds.Sort();

        const int32 FirstSectionIndex =
            InOutMeshSectionIndex;

        for (const int32 MaterialId : MaterialIds)
        {
            FCubusMeshData* MeshData =
                MaterialMeshes.Find(MaterialId);

            if (
                MeshData == nullptr ||
                !MeshData->IsValid()
            )
            {
                continue;
            }

            TargetMesh.CreateMeshSection_LinearColor(
                InOutMeshSectionIndex,
                MeshData->Vertices,
                MeshData->Triangles,
                MeshData->Normals,
                MeshData->UV0,
                MeshData->VertexColors,
                MeshData->Tangents,
                bGenerateCollision
            );

            UMaterialInterface* ResolvedMaterial = nullptr;

            if (IsValid(MaterialRegistry))
            {
                ResolvedMaterial =
                    MaterialRegistry->ResolveRuntimeMaterial(
                        MaterialId
                    );
            }

            if (!IsValid(ResolvedMaterial))
            {
                ResolvedMaterial =
                    UMaterial::GetDefaultMaterial(MD_Surface);
            }

            TargetMesh.SetMaterial(
                InOutMeshSectionIndex,
                ResolvedMaterial
            );

            InOutVertexCount +=
                MeshData->GetVertexCount();

            InOutTriangleCount +=
                MeshData->GetTriangleCount();

            ++InOutMeshSectionIndex;
        }

        return
            InOutMeshSectionIndex -
            FirstSectionIndex;
    }
}

ACubusVoxelVolumeActor::ACubusVoxelVolumeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh =
        CreateDefaultSubobject<UProceduralMeshComponent>(
            TEXT("ProceduralMesh")
        );

    SetRootComponent(ProceduralMesh);

    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCastShadow(true);
    ProceduralMesh->SetMobility(EComponentMobility::Static);
    ProceduralMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
}

void ACubusVoxelVolumeActor::GenerateTerrainData()
{
    EnsureChunkData();
    ChunkData->Clear();
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
    bLastBuildHadCollision = false;
    EnsureChunkData();

    if (!IsValid(ProceduralMesh))
    {
        return;
    }

    const double BuildStartTime =
        FPlatformTime::Seconds();

    ProceduralMesh->ClearAllMeshSections();
    ProceduralMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    ProceduralMesh->SetVisibility(true);
    ProceduralMesh->SetHiddenInGame(false);
    ProceduralMesh->SetRenderInMainPass(true);
    ProceduralMesh->SetRenderInDepthPass(true);

    ResetDiagnostics();

    TotalVoxelCount = ChunkData->GetVoxelCount();
    SolidVoxelCount = ChunkData->GetOccupiedVoxelCount();

    LastBuiltRenderMode = GetEffectiveRenderMode();

    int32 MeshSectionIndex = 0;
    bool bBuiltCollision = false;

    switch (LastBuiltRenderMode)
    {
        case ECubusVoxelRenderMode::Blocks:
        {
            RebuildBlockMesh(
                bGenerateCollision,
                MeshSectionIndex
            );

            bBuiltCollision =
                bGenerateCollision &&
                GeneratedBlockSectionCount > 0;
            break;
        }

        case ECubusVoxelRenderMode::Density:
        {
            RebuildDensityMesh(
                bGenerateCollision,
                MeshSectionIndex
            );

            bBuiltCollision =
                bGenerateCollision &&
                GeneratedDensitySectionCount > 0;
            break;
        }

        case ECubusVoxelRenderMode::Hybrid:
        {
            // Block collision remains authoritative until block/density edits
            // are represented by one composite scalar field.
            RebuildBlockMesh(
                bGenerateCollision,
                MeshSectionIndex
            );

            RebuildDensityMesh(
                false,
                MeshSectionIndex
            );

            bBuiltCollision =
                bGenerateCollision &&
                GeneratedBlockSectionCount > 0;
            break;
        }

        default:
            checkNoEntry();
            break;
    }

    GeneratedMaterialSectionCount = MeshSectionIndex;

    ProceduralMesh->SetCollisionEnabled(
        bBuiltCollision
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision
    );

    bLastBuildHadCollision = bBuiltCollision;

    LastBuildTimeMilliseconds =
        static_cast<float>(
            (
                FPlatformTime::Seconds() -
                BuildStartTime
            ) *
            1000.0
        );

    ProceduralMesh->MarkRenderStateDirty();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus chunk (%d, %d, %d) built mode=%d rootSections=%d blockSections=%d densitySections=%d vertices=%d triangles=%d densityTriangles=%d collision=%s time=%.2fms"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        static_cast<int32>(LastBuiltRenderMode),
        GeneratedMaterialSectionCount,
        GeneratedBlockSectionCount,
        GeneratedDensitySectionCount,
        GeneratedVertexCount,
        GeneratedTriangleCount,
        GeneratedDensityTriangleCount,
        bBuiltCollision ? TEXT("true") : TEXT("false"),
        LastBuildTimeMilliseconds
    );
}

ECubusVoxelRenderMode
ACubusVoxelVolumeActor::GetEffectiveRenderMode() const
{
    if (IsValid(OwningBlockWorld.Get()))
    {
        return OwningBlockWorld->GetVoxelRenderMode();
    }

    return StandaloneRenderMode;
}

void ACubusVoxelVolumeActor::RebuildBlockMesh(
    const bool bGenerateBlockCollision,
    int32& InOutMeshSectionIndex
)
{
    FCubusMaterialMeshMap MaterialMeshes;
    const FCubusBlockChunkNeighborhood Neighborhood =
        BuildNeighborhood();

    FCubusBlockMesher::BuildChunk(
        Neighborhood,
        MaterialRegistry.Get(),
        VoxelSize,
        MaterialMeshes,
        GeneratedFaceCount
    );

    GeneratedBlockSectionCount =
        CubusVoxelVolumeActor::AppendMaterialMeshes(
            *ProceduralMesh,
            MaterialRegistry.Get(),
            MaterialMeshes,
            bGenerateBlockCollision,
            InOutMeshSectionIndex,
            GeneratedVertexCount,
            GeneratedTriangleCount
        );
}

void ACubusVoxelVolumeActor::RebuildDensityMesh(
    const bool bGenerateDensityCollision,
    int32& InOutMeshSectionIndex
)
{
    FCubusTerrainDensitySettings DensitySettings;

    DensitySettings.bUseHeightTerrain =
        bUseHeightTerrain;
    DensitySettings.FlatSurfaceWorldZ =
        static_cast<float>(TerrainSurfaceWorldZ);
    DensitySettings.BaseHeight =
        static_cast<float>(TerrainBaseHeight);

    DensitySettings.ContinentAmplitude =
        TerrainContinentAmplitude;
    DensitySettings.ContinentFrequency =
        TerrainContinentFrequency;
    DensitySettings.HillAmplitude =
        TerrainHillAmplitude;
    DensitySettings.HillFrequency =
        TerrainHillFrequency;
    DensitySettings.DetailAmplitude =
        TerrainDetailAmplitude;
    DensitySettings.DetailFrequency =
        TerrainDetailFrequency;
    DensitySettings.RidgeAmplitude =
        TerrainRidgeAmplitude;
    DensitySettings.RidgeFrequency =
        TerrainRidgeFrequency;

    DensitySettings.ValleyDepth =
        TerrainValleyDepth;
    DensitySettings.ValleyFrequency =
        TerrainValleyFrequency;
    DensitySettings.ValleyWidth =
        TerrainValleyWidth;
    DensitySettings.ValleyFalloff =
        TerrainValleyFalloff;
    DensitySettings.ValleyWarpAmplitude =
        TerrainValleyWarpAmplitude;
    DensitySettings.ValleyWarpFrequency =
        TerrainValleyWarpFrequency;

    DensitySettings.RegionFrequency =
        TerrainRegionFrequency;
    DensitySettings.PlainsThreshold =
        TerrainPlainsThreshold;
    DensitySettings.PlainsBlend =
        TerrainPlainsBlend;
    DensitySettings.MountainThreshold =
        TerrainMountainThreshold;
    DensitySettings.MountainBlend =
        TerrainMountainBlend;

    DensitySettings.SurfaceMaterialId =
        TerrainSurfaceMaterialId;
    DensitySettings.SubsurfaceMaterialId =
        TerrainSubsurfaceMaterialId;
    DensitySettings.RockMaterialId =
        TerrainRockMaterialId;
    DensitySettings.SnowMaterialId =
        TerrainSnowMaterialId;
    DensitySettings.RockSlopeThreshold =
        TerrainRockSlopeThreshold;
    DensitySettings.SnowMinimumHeight =
        static_cast<float>(TerrainSnowMinimumHeight);

    const FCubusGenerationSeeds& Seeds =
        ChunkData->GetGenerationSeeds();

    DensitySettings.TerrainOffsetX =
        CubusVoxelVolumeActor::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetX(
                Seeds.Terrain
            )
        );

    DensitySettings.TerrainOffsetY =
        CubusVoxelVolumeActor::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetY(
                Seeds.Terrain
            )
        );

    DensitySettings.RiverOffsetX =
        FCubusGenerationSeeds::DomainOffsetX(
            Seeds.Rivers
        );

    DensitySettings.RiverOffsetY =
        FCubusGenerationSeeds::DomainOffsetY(
            Seeds.Rivers
        );

    DensitySettings.CaveOffsetX =
        FCubusGenerationSeeds::DomainOffsetX(
            Seeds.Caves
        );

    DensitySettings.CaveOffsetY =
        FCubusGenerationSeeds::DomainOffsetY(
            Seeds.Caves
        );

    DensitySettings.CaveOffsetZ =
        FCubusGenerationSeeds::DomainOffsetZ(
            Seeds.Caves
        );

    if (IsValid(GeologyProfile.Get()))
    {
        DensitySettings.bGenerateRivers =
            GeologyProfile->bGenerateRivers;
        DensitySettings.RiverFrequency =
            GeologyProfile->RiverFrequency;
        DensitySettings.RiverChannelWidth =
            GeologyProfile->RiverChannelWidth;
        DensitySettings.RiverValleyWidth =
            GeologyProfile->RiverValleyWidth;
        DensitySettings.RiverValleyDepth =
            GeologyProfile->RiverValleyDepth;
        DensitySettings.RiverChannelDepth =
            static_cast<float>(
                GeologyProfile->RiverChannelDepth
            );
        DensitySettings.RiverWarpAmplitude =
            GeologyProfile->RiverWarpAmplitude;
        DensitySettings.RiverWarpFrequency =
            GeologyProfile->RiverWarpFrequency;

        DensitySettings.bGenerateCaves =
            GeologyProfile->bGenerateCaves;
        DensitySettings.CaveMinimumWorldZ =
            GeologyProfile->CaveMinimumWorldZ;
        DensitySettings.CaveMaximumWorldZ =
            GeologyProfile->CaveMaximumWorldZ;
        DensitySettings.CaveSurfaceClearance =
            GeologyProfile->CaveSurfaceClearance;
        DensitySettings.CavePrimaryFrequency =
            GeologyProfile->CavePrimaryFrequency;
        DensitySettings.CaveSecondaryFrequency =
            GeologyProfile->CaveSecondaryFrequency;
        DensitySettings.CaveThreshold =
            GeologyProfile->CaveThreshold;
    }

    const FCubusTerrainDensityField DensityField(
        DensitySettings
    );

    FCubusDensityEditMap DensityEditSnapshot;

    if (IsValid(OwningBlockWorld.Get()))
    {
        DensityEditSnapshot =
            OwningBlockWorld->BuildDensityEditSnapshot(
                ChunkCoordinate
            );
    }

    const FCubusDensityEditField EditedDensityField(
        DensityField,
        DensityEditSnapshot
    );

    FCubusDensitySamplingBuffer DensityBuffer;
    DensityBuffer.Build(
        ChunkCoordinate,
        EditedDensityField
    );

    FCubusMaterialMeshMap MaterialMeshes;
    int32 MesherTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        DensityBuffer,
        VoxelSize,
        0.0f,
        MaterialMeshes,
        MesherTriangleCount
    );

    GeneratedDensityTriangleCount =
        MesherTriangleCount;

    GeneratedDensitySectionCount =
        CubusVoxelVolumeActor::AppendMaterialMeshes(
            *ProceduralMesh,
            MaterialRegistry.Get(),
            MaterialMeshes,
            bGenerateDensityCollision,
            InOutMeshSectionIndex,
            GeneratedVertexCount,
            GeneratedTriangleCount
        );

    if (GeneratedDensitySectionCount <= 0)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Cubus native density chunk (%d, %d, %d) contains no isosurface."),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z
        );
    }
}

void ACubusVoxelVolumeActor::EnsureChunkData()
{
    if (!ChunkData.IsValid())
    {
        ChunkData =
            MakeUnique<FCubusBlockChunkData>(
                ChunkCoordinate
            );
    }
    else
    {
        ChunkData->SetChunkCoordinate(
            ChunkCoordinate
        );
    }
}

void ACubusVoxelVolumeActor::SynchronizeChunkState()
{
    EnsureChunkData();
    ChunkData->SetChunkCoordinate(
        ChunkCoordinate
    );

    const double ChunkWorldSize =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(VoxelSize);

    SetActorLocation(
        FVector(
            static_cast<double>(ChunkCoordinate.X) *
                ChunkWorldSize,
            static_cast<double>(ChunkCoordinate.Y) *
                ChunkWorldSize,
            static_cast<double>(ChunkCoordinate.Z) *
                ChunkWorldSize
        )
    );
}

void ACubusVoxelVolumeActor::ConfigureGeneratedChunk(
    const FIntVector& InChunkCoordinate,
    const float InVoxelSize,
    ACubusBlockWorldActor* InBlockWorld
)
{
    ChunkCoordinate = InChunkCoordinate;
    VoxelSize = FMath::Max(1.0f, InVoxelSize);
    OwningBlockWorld = InBlockWorld;

    SynchronizeChunkState();

    if (IsValid(OwningBlockWorld.Get()))
    {
        OwningBlockWorld->RegisterChunk(this);
    }
}

void ACubusVoxelVolumeActor::ConfigureRendering(
    UCubusMaterialRegistry* InMaterialRegistry
)
{
    MaterialRegistry = InMaterialRegistry;
}

void ACubusVoxelVolumeActor::ConfigureGeology(
    UCubusGeologyProfile* InGeologyProfile
)
{
    GeologyProfile = InGeologyProfile;
}

void ACubusVoxelVolumeActor::ConfigureTerrain(
    const bool bInUseHeightTerrain,
    const int32 InTerrainSurfaceWorldZ,
    const int32 InTerrainBaseHeight,
    const float InTerrainContinentAmplitude,
    const float InTerrainContinentFrequency,
    const float InTerrainHillAmplitude,
    const float InTerrainHillFrequency,
    const float InTerrainDetailAmplitude,
    const float InTerrainDetailFrequency,
    const float InTerrainRidgeAmplitude,
    const float InTerrainRidgeFrequency,
    const float InTerrainValleyDepth,
    const float InTerrainValleyFrequency,
    const float InTerrainValleyWidth,
    const float InTerrainValleyFalloff,
    const float InTerrainValleyWarpAmplitude,
    const float InTerrainValleyWarpFrequency,
    const float InTerrainRegionFrequency,
    const float InTerrainPlainsThreshold,
    const float InTerrainPlainsBlend,
    const float InTerrainMountainThreshold,
    const float InTerrainMountainBlend,
    const int32 InTerrainSurfaceMaterialId,
    const int32 InTerrainSubsurfaceMaterialId,
    const int32 InTerrainRockMaterialId,
    const int32 InTerrainSnowMaterialId,
    const float InTerrainRockSlopeThreshold,
    const int32 InTerrainSnowMinimumHeight,
    const bool bInGenerateWater,
    const int32 InTerrainWaterLevel,
    const int32 InTerrainWaterMaterialId
)
{
    bUseHeightTerrain = bInUseHeightTerrain;
    TerrainSurfaceWorldZ = InTerrainSurfaceWorldZ;
    TerrainBaseHeight = InTerrainBaseHeight;

    TerrainContinentAmplitude =
        FMath::Max(
            0.0f,
            InTerrainContinentAmplitude
        );

    TerrainContinentFrequency =
        FMath::Max(
            0.000001f,
            InTerrainContinentFrequency
        );

    TerrainHillAmplitude =
        FMath::Max(
            0.0f,
            InTerrainHillAmplitude
        );

    TerrainHillFrequency =
        FMath::Max(
            0.000001f,
            InTerrainHillFrequency
        );

    TerrainDetailAmplitude =
        FMath::Max(
            0.0f,
            InTerrainDetailAmplitude
        );

    TerrainDetailFrequency =
        FMath::Max(
            0.000001f,
            InTerrainDetailFrequency
        );

    TerrainRidgeAmplitude =
        FMath::Max(
            0.0f,
            InTerrainRidgeAmplitude
        );

    TerrainRidgeFrequency =
        FMath::Max(
            0.000001f,
            InTerrainRidgeFrequency
        );

    TerrainValleyDepth =
        FMath::Max(
            0.0f,
            InTerrainValleyDepth
        );

    TerrainValleyFrequency =
        FMath::Max(
            0.000001f,
            InTerrainValleyFrequency
        );

    TerrainValleyWidth =
        FMath::Clamp(
            InTerrainValleyWidth,
            0.0f,
            1.0f
        );

    TerrainValleyFalloff =
        FMath::Clamp(
            InTerrainValleyFalloff,
            0.001f,
            1.0f
        );

    TerrainValleyWarpAmplitude =
        FMath::Max(
            0.0f,
            InTerrainValleyWarpAmplitude
        );

    TerrainValleyWarpFrequency =
        FMath::Max(
            0.000001f,
            InTerrainValleyWarpFrequency
        );

    TerrainRegionFrequency =
        FMath::Max(
            0.000001f,
            InTerrainRegionFrequency
        );

    TerrainPlainsThreshold =
        FMath::Clamp(
            InTerrainPlainsThreshold,
            -1.0f,
            1.0f
        );

    TerrainPlainsBlend =
        FMath::Clamp(
            InTerrainPlainsBlend,
            0.001f,
            1.0f
        );

    TerrainMountainThreshold =
        FMath::Clamp(
            InTerrainMountainThreshold,
            TerrainPlainsThreshold,
            1.0f
        );

    TerrainMountainBlend =
        FMath::Clamp(
            InTerrainMountainBlend,
            0.001f,
            1.0f
        );

    TerrainSurfaceMaterialId =
        FMath::Max(
            1,
            InTerrainSurfaceMaterialId
        );

    TerrainSubsurfaceMaterialId =
        FMath::Max(
            1,
            InTerrainSubsurfaceMaterialId
        );

    TerrainRockMaterialId =
        FMath::Max(
            1,
            InTerrainRockMaterialId
        );

    TerrainSnowMaterialId =
        FMath::Max(
            1,
            InTerrainSnowMaterialId
        );

    TerrainRockSlopeThreshold =
        FMath::Max(
            0.0f,
            InTerrainRockSlopeThreshold
        );

    TerrainSnowMinimumHeight =
        InTerrainSnowMinimumHeight;

    bGenerateWater = bInGenerateWater;
    TerrainWaterLevel = InTerrainWaterLevel;

    TerrainWaterMaterialId =
        FMath::Max(
            1,
            InTerrainWaterMaterialId
        );
}

const FCubusBlockChunkData* ACubusVoxelVolumeActor::FindNeighbourChunkData(
    const FIntVector& CoordinateOffset
) const
{
    if (!IsValid(OwningBlockWorld.Get()))
    {
        return nullptr;
    }

    ACubusVoxelVolumeActor* NeighbourActor =
        OwningBlockWorld->FindChunk(
            ChunkCoordinate +
            CoordinateOffset
        );

    if (!IsValid(NeighbourActor))
    {
        return nullptr;
    }

    return NeighbourActor->GetChunkData();
}

FCubusBlockChunkNeighborhood
ACubusVoxelVolumeActor::BuildNeighborhood() const
{
    FCubusBlockChunkNeighborhood Neighborhood;
    Neighborhood.Centre = ChunkData.Get();
    Neighborhood.PositiveX =
        FindNeighbourChunkData(FIntVector(1, 0, 0));
    Neighborhood.NegativeX =
        FindNeighbourChunkData(FIntVector(-1, 0, 0));
    Neighborhood.PositiveY =
        FindNeighbourChunkData(FIntVector(0, 1, 0));
    Neighborhood.NegativeY =
        FindNeighbourChunkData(FIntVector(0, -1, 0));
    Neighborhood.PositiveZ =
        FindNeighbourChunkData(FIntVector(0, 0, 1));
    Neighborhood.NegativeZ =
        FindNeighbourChunkData(FIntVector(0, 0, -1));
    return Neighborhood;
}

void ACubusVoxelVolumeActor::RebuildAffectedChunks()
{
    if (IsValid(OwningBlockWorld.Get()))
    {
        OwningBlockWorld->RebuildChunkAndNeighbours(
            ChunkCoordinate
        );
        return;
    }

    RebuildVolume();
}

void ACubusVoxelVolumeActor::GenerateFlatTerrain()
{
    FCubusBlockTerrainGenerator::GenerateFlatTerrain(
        *ChunkData,
        TerrainSurfaceWorldZ,
        TerrainSurfaceMaterialId,
        TerrainSubsurfaceMaterialId
    );
}

void ACubusVoxelVolumeActor::GenerateHeightTerrain()
{
    FCubusBlockTerrainGenerator::GenerateHeightTerrain(
        *ChunkData,
        TerrainBaseHeight,
        TerrainContinentAmplitude,
        TerrainContinentFrequency,
        TerrainHillAmplitude,
        TerrainHillFrequency,
        TerrainDetailAmplitude,
        TerrainDetailFrequency,
        TerrainRidgeAmplitude,
        TerrainRidgeFrequency,
        TerrainValleyDepth,
        TerrainValleyFrequency,
        TerrainValleyWidth,
        TerrainValleyFalloff,
        TerrainValleyWarpAmplitude,
        TerrainValleyWarpFrequency,
        TerrainRegionFrequency,
        TerrainPlainsThreshold,
        TerrainPlainsBlend,
        TerrainMountainThreshold,
        TerrainMountainBlend,
        TerrainSurfaceMaterialId,
        TerrainSubsurfaceMaterialId,
        TerrainRockMaterialId,
        TerrainSnowMaterialId,
        TerrainRockSlopeThreshold,
        TerrainSnowMinimumHeight,
        bGenerateWater,
        TerrainWaterLevel,
        TerrainWaterMaterialId,
        GeologyProfile.Get()
    );
}

void ACubusVoxelVolumeActor::ResetDiagnostics()
{
    TotalVoxelCount = 0;
    SolidVoxelCount = 0;
    GeneratedFaceCount = 0;
    GeneratedVertexCount = 0;
    GeneratedTriangleCount = 0;
    GeneratedMaterialSectionCount = 0;
    GeneratedBlockSectionCount = 0;
    GeneratedDensitySectionCount = 0;
    GeneratedDensityTriangleCount = 0;
    LastBuildTimeMilliseconds = 0.0f;
}
