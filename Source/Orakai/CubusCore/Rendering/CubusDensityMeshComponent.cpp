#include "CubusCore/Rendering/CubusDensityMeshComponent.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

UCubusDensityMeshComponent::UCubusDensityMeshComponent(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    bUseAsyncCooking = true;
    SetCastShadow(true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UCubusDensityMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!bAutoRebuildOnBeginPlay)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(
                this,
                &UCubusDensityMeshComponent::RebuildDensityMeshDeferred
            )
        );
    }
}

bool UCubusDensityMeshComponent::RebuildDensityMesh()
{
    const double BuildStartTime =
        FPlatformTime::Seconds();

    ClearAllMeshSections();
    ResetDensityDiagnostics();

    const ACubusVoxelVolumeActor* OwnerChunk =
        Cast<ACubusVoxelVolumeActor>(GetOwner());

    if (
        !IsValid(OwnerChunk) ||
        OwnerChunk->GetChunkData() == nullptr
    )
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus density mesh component requires an owning Cubus voxel chunk with generated data.")
        );
        return false;
    }

    FCubusBlockVoxelSampler VoxelSampler =
        [this, OwnerChunk](
            const FIntVector& GlobalSampleCoordinate
        )
        {
            return SampleVoxelAtWorldCoordinate(
                *OwnerChunk,
                GlobalSampleCoordinate
            );
        };

    const FCubusBlockDensityField DensityField(
        MoveTemp(VoxelSampler),
        bTreatWaterAsEmpty,
        DensityMagnitude
    );

    FCubusDensitySamplingBuffer DensityBuffer;
    DensityBuffer.Build(
        OwnerChunk->GetChunkCoordinate(),
        DensityField
    );

    TMap<int32, FCubusMeshData> MaterialMeshes;
    int32 MesherTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        DensityBuffer,
        OwnerChunk->GetVoxelSize(),
        IsoLevel,
        MaterialMeshes,
        MesherTriangleCount
    );

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

        CreateMeshSection_LinearColor(
            MeshSectionIndex,
            MeshData->Vertices,
            MeshData->Triangles,
            MeshData->Normals,
            MeshData->UV0,
            MeshData->VertexColors,
            MeshData->Tangents,
            bGenerateDensityCollision
        );

        UMaterialInterface* ResolvedMaterial =
            DefaultMaterial.Get();

        if (IsValid(MaterialRegistry.Get()))
        {
            ResolvedMaterial =
                MaterialRegistry->ResolveRuntimeMaterial(
                    MaterialId
                );
        }

        SetMaterial(
            MeshSectionIndex,
            ResolvedMaterial
        );

        GeneratedDensityVertexCount +=
            MeshData->GetVertexCount();

        GeneratedDensityTriangleCount +=
            MeshData->GetTriangleCount();

        ++MeshSectionIndex;
    }

    GeneratedDensitySectionCount =
        MeshSectionIndex;

    ensureMsgf(
        GeneratedDensityTriangleCount ==
            MesherTriangleCount,
        TEXT("Cubus density triangle diagnostics did not match mesher output.")
    );

    SetCollisionEnabled(
        bGenerateDensityCollision &&
        MeshSectionIndex > 0
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision
    );

    LastDensityBuildTimeMilliseconds =
        static_cast<float>(
            (
                FPlatformTime::Seconds() -
                BuildStartTime
            ) *
            1000.0
        );

    return MeshSectionIndex > 0;
}

void UCubusDensityMeshComponent::ClearDensityMesh()
{
    ClearAllMeshSections();
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ResetDensityDiagnostics();
}

FCubusBlockVoxel
UCubusDensityMeshComponent::SampleVoxelAtWorldCoordinate(
    const ACubusVoxelVolumeActor& OwnerChunk,
    const FIntVector& WorldVoxelCoordinate
) const
{
    const FIntVector TargetChunkCoordinate =
        WorldVoxelToChunkCoordinate(
            WorldVoxelCoordinate
        );

    const ACubusVoxelVolumeActor* TargetChunk =
        nullptr;

    if (
        TargetChunkCoordinate ==
        OwnerChunk.GetChunkCoordinate()
    )
    {
        TargetChunk = &OwnerChunk;
    }
    else if (
        const ACubusBlockWorldActor* BlockWorld =
            OwnerChunk.GetOwningBlockWorld()
    )
    {
        TargetChunk =
            BlockWorld->FindChunk(
                TargetChunkCoordinate
            );
    }

    FCubusBlockVoxel EmptyVoxel;

    if (!IsValid(TargetChunk))
    {
        return EmptyVoxel;
    }

    const FCubusBlockChunkData* TargetData =
        TargetChunk->GetChunkData();

    if (TargetData == nullptr)
    {
        return EmptyVoxel;
    }

    const FIntVector LocalCoordinate =
        WorldVoxelCoordinate -
        TargetChunkCoordinate *
        Cubus::ChunkSize;

    const FCubusBlockVoxel* Voxel =
        TargetData->GetVoxel(LocalCoordinate);

    return Voxel != nullptr
        ? *Voxel
        : EmptyVoxel;
}

FIntVector
UCubusDensityMeshComponent::WorldVoxelToChunkCoordinate(
    const FIntVector& WorldVoxelCoordinate
)
{
    return FIntVector(
        FloorDivide(
            WorldVoxelCoordinate.X,
            Cubus::ChunkSize
        ),
        FloorDivide(
            WorldVoxelCoordinate.Y,
            Cubus::ChunkSize
        ),
        FloorDivide(
            WorldVoxelCoordinate.Z,
            Cubus::ChunkSize
        )
    );
}

int32 UCubusDensityMeshComponent::FloorDivide(
    const int32 Value,
    const int32 PositiveDivisor
)
{
    check(PositiveDivisor > 0);

    int32 Quotient =
        Value /
        PositiveDivisor;

    const int32 Remainder =
        Value %
        PositiveDivisor;

    if (Remainder < 0)
    {
        --Quotient;
    }

    return Quotient;
}

void UCubusDensityMeshComponent::RebuildDensityMeshDeferred()
{
    RebuildDensityMesh();
}

void UCubusDensityMeshComponent::ResetDensityDiagnostics()
{
    GeneratedDensityVertexCount = 0;
    GeneratedDensityTriangleCount = 0;
    GeneratedDensitySectionCount = 0;
    LastDensityBuildTimeMilliseconds = 0.0f;
}
