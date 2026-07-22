#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusMaterialDefinition.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Meshing/CubusBlockMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"
#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"
#include "CubusCore/Data/CubusGeologyProfile.h"

#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelVolumeActor
{
    int32 ResolveMaterialId(
        const int32 MaterialId
    )
    {
        return FMath::Clamp(
            MaterialId,
            1,
            65535
        );
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

    ProceduralMesh->SetMobility(
        EComponentMobility::Static
    );

    ProceduralMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    Dimensions = FIntVector(
        Cubus::ChunkSize,
        Cubus::ChunkSize,
        Cubus::ChunkSize
    );
}

void ACubusVoxelVolumeActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    Dimensions = FIntVector(
        Cubus::ChunkSize,
        Cubus::ChunkSize,
        Cubus::ChunkSize
    );

    VoxelSize = FMath::Max(
        1.0f,
        VoxelSize
    );

    TestSolidMaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    SynchronizeChunkState();
    ResolveOwningBlockWorld();

    if (IsValid(OwningBlockWorld.Get()))
    {
        OwningBlockWorld->RegisterChunk(this);
    }

    if (bRebuildAutomatically)
    {
        GenerateTestShape();
    }
}

void ACubusVoxelVolumeActor::GenerateTestShapeData()
{
    EnsureChunkData();

    ChunkData->Clear();

    switch (TestShape)
    {
        case ECubusVolumeTestShape::FlatTerrain:
        {
            GenerateFlatTerrain();
            break;
        }

        case ECubusVolumeTestShape::HeightTerrain:
        {
            GenerateHeightTerrain();
            break;
        }

        case ECubusVolumeTestShape::SolidBlock:
        {
            GenerateSolidBlock();
            break;
        }

        case ECubusVolumeTestShape::Platform:
        {
            GeneratePlatform();
            break;
        }

        case ECubusVolumeTestShape::Steps:
        {
            GenerateSteps();
            break;
        }

        case ECubusVolumeTestShape::HollowRoom:
        {
            GenerateHollowRoom();
            break;
        }

        case ECubusVolumeTestShape::Pillars:
        {
            GeneratePillars();
            break;
        }

        case ECubusVolumeTestShape::MixedMaterials:
        {
            GenerateMixedMaterials();
            break;
        }

        default:
        {
            checkNoEntry();
            break;
        }
    }
}

void ACubusVoxelVolumeActor::GenerateTestShape()
{
    GenerateTestShapeData();
    RebuildVolume();
}

void ACubusVoxelVolumeActor::FillVolume()
{
    EnsureChunkData();

    FCubusBlockVoxel SolidVoxel;

    SolidVoxel.MaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    SolidVoxel.Flags = 0;

    ChunkData->Fill(SolidVoxel);
    RebuildVolume();
}

void ACubusVoxelVolumeActor::ClearVolume()
{
    EnsureChunkData();

    ChunkData->Clear();

    RebuildVolume();
}

void ACubusVoxelVolumeActor::RebuildVolume()
{
    ++RebuildCount;

    EnsureChunkData();
    ResolveOwningBlockWorld();

    if (!IsValid(ProceduralMesh))
    {
        return;
    }

    const double BuildStartTime =
        FPlatformTime::Seconds();

    ProceduralMesh->ClearAllMeshSections();
    ResetDiagnostics();

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

    TotalVoxelCount =
        ChunkData->GetVoxelCount();

    SolidVoxelCount =
        ChunkData->GetOccupiedVoxelCount();

    TArray<int32> MaterialIds;

    MaterialMeshes.GetKeys(MaterialIds);
    MaterialIds.Sort();

    int32 MeshSectionIndex = 0;

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

        ProceduralMesh->CreateMeshSection_LinearColor(
            MeshSectionIndex,
            MeshData->Vertices,
            MeshData->Triangles,
            MeshData->Normals,
            MeshData->UV0,
            MeshData->VertexColors,
            MeshData->Tangents,
            bGenerateCollision
        );

        UMaterialInterface* ResolvedMaterial =
            VoxelMaterial.Get();

        if (IsValid(MaterialRegistry.Get()))
        {
            const FCubusMaterialDefinition* Definition =
                MaterialRegistry->FindMaterialDefinition(
                    MaterialId
                );

            if (
                Definition != nullptr &&
                IsValid(Definition->Material.Get())
            )
            {
                ResolvedMaterial =
                    Definition->Material.Get();
            }
        }

        ProceduralMesh->SetMaterial(
            MeshSectionIndex,
            ResolvedMaterial
        );

        GeneratedVertexCount +=
            MeshData->GetVertexCount();

        GeneratedTriangleCount +=
            MeshData->GetTriangleCount();

        ++MeshSectionIndex;
    }

    GeneratedMaterialSectionCount =
        MeshSectionIndex;

    ProceduralMesh->SetCollisionEnabled(
        bGenerateCollision &&
        MeshSectionIndex > 0
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision
    );

    const double BuildEndTime =
        FPlatformTime::Seconds();

    LastBuildTimeMilliseconds =
        static_cast<float>(
            (
                BuildEndTime -
                BuildStartTime
            ) *
            1000.0
        );
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
        static_cast<double>(
            Cubus::ChunkSize
        ) *
        static_cast<double>(
            VoxelSize
        );

    SetActorLocation(
        FVector(
            static_cast<double>(
                ChunkCoordinate.X
            ) * ChunkWorldSize,
            static_cast<double>(
                ChunkCoordinate.Y
            ) * ChunkWorldSize,
            static_cast<double>(
                ChunkCoordinate.Z
            ) * ChunkWorldSize
        )
    );
}

void ACubusVoxelVolumeActor::ResolveOwningBlockWorld()
{
    if (IsValid(OwningBlockWorld.Get()))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    for (
        TActorIterator<ACubusBlockWorldActor>
            Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        ACubusBlockWorldActor* FoundWorld =
            *Iterator;

        if (IsValid(FoundWorld))
        {
            OwningBlockWorld = FoundWorld;
            return;
        }
    }
}

void ACubusVoxelVolumeActor::ConfigureGeneratedChunk(
    const FIntVector& InChunkCoordinate,
    const float InVoxelSize,
    ACubusBlockWorldActor* InBlockWorld
)
{
    ChunkCoordinate =
        InChunkCoordinate;

    VoxelSize =
        FMath::Max(
            1.0f,
            InVoxelSize
        );

    OwningBlockWorld =
        InBlockWorld;

    SynchronizeChunkState();

    if (IsValid(OwningBlockWorld.Get()))
    {
        OwningBlockWorld->RegisterChunk(this);
    }
}

void ACubusVoxelVolumeActor::ConfigureRendering(
    UCubusMaterialRegistry* InMaterialRegistry,
    UMaterialInterface* InFallbackVoxelMaterial
)
{
    if (IsValid(InMaterialRegistry))
    {
        MaterialRegistry =
            InMaterialRegistry;
    }

    if (IsValid(InFallbackVoxelMaterial))
    {
        VoxelMaterial =
            InFallbackVoxelMaterial;
    }
}

void ACubusVoxelVolumeActor::ConfigureGeology(
    UCubusGeologyProfile* InGeologyProfile
)
{
    GeologyProfile =
        InGeologyProfile;
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
    TestShape =
        bInUseHeightTerrain
            ? ECubusVolumeTestShape::HeightTerrain
            : ECubusVolumeTestShape::FlatTerrain;

    TerrainSurfaceWorldZ =
        InTerrainSurfaceWorldZ;

    TerrainBaseHeight =
        InTerrainBaseHeight;

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

    bGenerateWater =
        bInGenerateWater;

    TerrainWaterLevel =
        InTerrainWaterLevel;

    TerrainWaterMaterialId =
        FMath::Max(
            1,
            InTerrainWaterMaterialId
        );
}

const FCubusBlockChunkData*
ACubusVoxelVolumeActor::FindNeighbourChunkData(
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

    Neighborhood.Centre =
        ChunkData.Get();

    Neighborhood.PositiveX =
        FindNeighbourChunkData(
            FIntVector(1, 0, 0)
        );

    Neighborhood.NegativeX =
        FindNeighbourChunkData(
            FIntVector(-1, 0, 0)
        );

    Neighborhood.PositiveY =
        FindNeighbourChunkData(
            FIntVector(0, 1, 0)
        );

    Neighborhood.NegativeY =
        FindNeighbourChunkData(
            FIntVector(0, -1, 0)
        );

    Neighborhood.PositiveZ =
        FindNeighbourChunkData(
            FIntVector(0, 0, 1)
        );

    Neighborhood.NegativeZ =
        FindNeighbourChunkData(
            FIntVector(0, 0, -1)
        );

    return Neighborhood;
}

void ACubusVoxelVolumeActor::RebuildAffectedChunks()
{
    ResolveOwningBlockWorld();

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

void ACubusVoxelVolumeActor::GenerateSolidBlock()
{
    FCubusBlockVoxel SolidVoxel;

    SolidVoxel.MaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    SolidVoxel.Flags = 0;

    ChunkData->Fill(SolidVoxel);
}

void ACubusVoxelVolumeActor::GeneratePlatform()
{
    const int32 SolidMaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    const int32 PlatformHeight =
        FMath::Max(
            1,
            Cubus::ChunkSize / 4
        );

    for (
        int32 Z = 0;
        Z < PlatformHeight;
        ++Z
    )
    {
        for (
            int32 Y = 0;
            Y < Cubus::ChunkSize;
            ++Y
        )
        {
            for (
                int32 X = 0;
                X < Cubus::ChunkSize;
                ++X
            )
            {
                ChunkData->SetMaterialId(
                    X,
                    Y,
                    Z,
                    SolidMaterialId
                );
            }
        }
    }
}

void ACubusVoxelVolumeActor::GenerateSteps()
{
    const int32 SolidMaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    for (
        int32 X = 0;
        X < Cubus::ChunkSize;
        ++X
    )
    {
        const float NormalizedX =
            Cubus::ChunkSize > 1
                ? static_cast<float>(X) /
                    static_cast<float>(
                        Cubus::ChunkSize - 1
                    )
                : 0.0f;

        const int32 ColumnHeight =
            FMath::Clamp(
                FMath::FloorToInt(
                    NormalizedX *
                    static_cast<float>(
                        Cubus::ChunkSize - 1
                    )
                ) + 1,
                1,
                Cubus::ChunkSize
            );

        for (
            int32 Y = 0;
            Y < Cubus::ChunkSize;
            ++Y
        )
        {
            for (
                int32 Z = 0;
                Z < ColumnHeight;
                ++Z
            )
            {
                ChunkData->SetMaterialId(
                    X,
                    Y,
                    Z,
                    SolidMaterialId
                );
            }
        }
    }
}

void ACubusVoxelVolumeActor::GenerateHollowRoom()
{
    const int32 SolidMaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    for (
        int32 Z = 0;
        Z < Cubus::ChunkSize;
        ++Z
    )
    {
        for (
            int32 Y = 0;
            Y < Cubus::ChunkSize;
            ++Y
        )
        {
            for (
                int32 X = 0;
                X < Cubus::ChunkSize;
                ++X
            )
            {
                const bool bBoundary =
                    X == 0 ||
                    Y == 0 ||
                    Z == 0 ||
                    X == Cubus::ChunkSize - 1 ||
                    Y == Cubus::ChunkSize - 1 ||
                    Z == Cubus::ChunkSize - 1;

                if (bBoundary)
                {
                    ChunkData->SetMaterialId(
                        X,
                        Y,
                        Z,
                        SolidMaterialId
                    );
                }
            }
        }
    }

    const int32 DoorCentreX =
        Cubus::ChunkSize / 2;

    const int32 DoorHalfWidth =
        FMath::Max(
            0,
            Cubus::ChunkSize / 8
        );

    const int32 DoorHeight =
        FMath::Max(
            1,
            Cubus::ChunkSize / 2
        );

    for (
        int32 X =
            DoorCentreX -
            DoorHalfWidth;
        X <=
            DoorCentreX +
            DoorHalfWidth;
        ++X
    )
    {
        for (
            int32 Z = 0;
            Z < DoorHeight;
            ++Z
        )
        {
            ChunkData->SetMaterialId(
                X,
                0,
                Z,
                0
            );
        }
    }
}

void ACubusVoxelVolumeActor::GeneratePillars()
{
    const int32 SolidMaterialId =
        CubusVoxelVolumeActor::ResolveMaterialId(
            TestSolidMaterialId
        );

    const int32 MaximumCoordinate =
        Cubus::ChunkSize - 1;

    const FIntPoint PillarPositions[] =
    {
        FIntPoint(0, 0),
        FIntPoint(MaximumCoordinate, 0),
        FIntPoint(0, MaximumCoordinate),
        FIntPoint(
            MaximumCoordinate,
            MaximumCoordinate
        )
    };

    for (
        const FIntPoint& PillarPosition :
        PillarPositions
    )
    {
        for (
            int32 Z = 0;
            Z < Cubus::ChunkSize;
            ++Z
        )
        {
            ChunkData->SetMaterialId(
                PillarPosition.X,
                PillarPosition.Y,
                Z,
                SolidMaterialId
            );
        }
    }

    for (
        int32 Y = 0;
        Y < Cubus::ChunkSize;
        ++Y
    )
    {
        for (
            int32 X = 0;
            X < Cubus::ChunkSize;
            ++X
        )
        {
            ChunkData->SetMaterialId(
                X,
                Y,
                0,
                SolidMaterialId
            );
        }
    }
}

void ACubusVoxelVolumeActor::GenerateMixedMaterials()
{
    constexpr int32 StoneMaterialId = 1;
    constexpr int32 DirtMaterialId = 2;
    constexpr int32 SandMaterialId = 3;

    const int32 FirstLayerHeight =
        FMath::Max(
            1,
            Cubus::ChunkSize / 3
        );

    const int32 SecondLayerHeight =
        FMath::Max(
            FirstLayerHeight + 1,
            (
                Cubus::ChunkSize *
                2
            ) / 3
        );

    for (
        int32 Z = 0;
        Z < Cubus::ChunkSize;
        ++Z
    )
    {
        int32 MaterialId =
            SandMaterialId;

        if (Z < FirstLayerHeight)
        {
            MaterialId =
                StoneMaterialId;
        }
        else if (Z < SecondLayerHeight)
        {
            MaterialId =
                DirtMaterialId;
        }

        for (
            int32 Y = 0;
            Y < Cubus::ChunkSize;
            ++Y
        )
        {
            for (
                int32 X = 0;
                X < Cubus::ChunkSize;
                ++X
            )
            {
                ChunkData->SetMaterialId(
                    X,
                    Y,
                    Z,
                    MaterialId
                );
            }
        }
    }
}

void ACubusVoxelVolumeActor::ResetDiagnostics()
{
    TotalVoxelCount = 0;
    SolidVoxelCount = 0;
    GeneratedFaceCount = 0;
    GeneratedVertexCount = 0;
    GeneratedTriangleCount = 0;
    GeneratedMaterialSectionCount = 0;
    LastBuildTimeMilliseconds = 0.0f;
}
