#include "CubusCore/Rendering/CubusDensityMeshComponent.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
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
    SetRenderInMainPass(true);
    SetRenderInDepthPass(true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UCubusDensityMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    const ACubusVoxelVolumeActor* OwnerChunk =
        Cast<ACubusVoxelVolumeActor>(GetOwner());

    // Older setup instructions temporarily required a Blueprint-added density
    // component. Chunks now own a native density component, so prevent an old
    // manually-added component from rebuilding a second, stale surface over it.
    if (
        IsValid(OwnerChunk) &&
        OwnerChunk->GetDensityMeshComponent() != this
    )
    {
        ClearDensityMesh();
        SetVisibility(false);
        SetHiddenInGame(true);
        return;
    }

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

    if (!IsValid(OwnerChunk))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus density mesh component requires an owning Cubus voxel chunk.")
        );
        return false;
    }

    // Only the C++-owned density renderer may build automatically. This also
    // neutralises Blueprint components created while density was experimental.
    if (OwnerChunk->GetDensityMeshComponent() != this)
    {
        SetVisibility(false);
        SetHiddenInGame(true);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus ignored legacy duplicate density component %s on chunk %s."),
            *GetName(),
            *OwnerChunk->GetName()
        );

        return false;
    }

    // The original block mesh is the actor root in the current chunk class.
    // Density used to be its child, which meant hiding the block renderer also
    // hid the density renderer. Detach while preserving the chunk transform so
    // the two representations are genuinely independent.
    if (GetAttachParent() == OwnerChunk->GetRootComponent())
    {
        DetachFromComponent(
            FDetachmentTransformRules::KeepWorldTransform
        );
    }

    SetWorldTransform(OwnerChunk->GetActorTransform());
    SetVisibility(true);
    SetHiddenInGame(false);
    SetRenderInMainPass(true);
    SetRenderInDepthPass(true);

    // Density mode now always evaluates the continuous terrain scalar field.
    // The old +1/-1 block-occupancy adapter remains in the codebase for future
    // block/density transition work, but is no longer an automatic terrain
    // source and cannot silently turn Density mode back into rounded blocks.
    const FCubusTerrainDensityField DensityField(
        TerrainDensitySettings
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
            if (UMaterialInterface* RegistryMaterial =
                    MaterialRegistry->ResolveRuntimeMaterial(MaterialId))
            {
                ResolvedMaterial = RegistryMaterial;
            }
        }

        if (!IsValid(ResolvedMaterial))
        {
            ResolvedMaterial =
                UMaterial::GetDefaultMaterial(MD_Surface);
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

    MarkRenderStateDirty();

    if (MeshSectionIndex <= 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus native density produced no surface for chunk (%d, %d, %d)."),
            OwnerChunk->GetChunkCoordinate().X,
            OwnerChunk->GetChunkCoordinate().Y,
            OwnerChunk->GetChunkCoordinate().Z
        );

        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus native density built chunk (%d, %d, %d): %d sections, %d vertices, %d triangles in %.2f ms."),
        OwnerChunk->GetChunkCoordinate().X,
        OwnerChunk->GetChunkCoordinate().Y,
        OwnerChunk->GetChunkCoordinate().Z,
        GeneratedDensitySectionCount,
        GeneratedDensityVertexCount,
        GeneratedDensityTriangleCount,
        LastDensityBuildTimeMilliseconds
    );

    return true;
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
