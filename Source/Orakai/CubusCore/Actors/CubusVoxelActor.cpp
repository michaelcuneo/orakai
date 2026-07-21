#include "CubusCore/Actors/CubusVoxelActor.h"

#include "CubusCore/Meshing/CubusBlockMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

ACubusVoxelActor::ACubusVoxelActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh =
        CreateDefaultSubobject<UProceduralMeshComponent>(
            TEXT("ProceduralMesh")
        );

    SetRootComponent(ProceduralMesh);

    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCastShadow(true);
    ProceduralMesh->SetMobility(EComponentMobility::Movable);

    Voxel.Type = ECubusVoxelType::Solid;
    Voxel.MaterialId = 0;
}

void ACubusVoxelActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    if (bRebuildAutomatically)
    {
        RebuildVoxel();
    }
}

void ACubusVoxelActor::RebuildVoxel()
{
    if (!IsValid(ProceduralMesh))
    {
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    GeneratedVertexCount = 0;
    GeneratedTriangleCount = 0;

    FCubusMeshData MeshData;

    FCubusBlockMesher::BuildSingleVoxel(
        Voxel,
        VoxelSize,
        MeshData
    );

    if (!MeshData.IsValid())
    {
        ProceduralMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );

        return;
    }

    constexpr int32 MeshSectionIndex = 0;

    ProceduralMesh->CreateMeshSection_LinearColor(
        MeshSectionIndex,
        MeshData.Vertices,
        MeshData.Triangles,
        MeshData.Normals,
        MeshData.UV0,
        MeshData.VertexColors,
        MeshData.Tangents,
        bGenerateCollision
    );

    if (IsValid(VoxelMaterial))
    {
        ProceduralMesh->SetMaterial(
            MeshSectionIndex,
            VoxelMaterial
        );
    }

    ProceduralMesh->SetCollisionEnabled(
        bGenerateCollision
        ? ECollisionEnabled::QueryAndPhysics
        : ECollisionEnabled::NoCollision
    );

    GeneratedVertexCount = MeshData.GetVertexCount();
    GeneratedTriangleCount = MeshData.GetTriangleCount();
}

void ACubusVoxelActor::ClearVoxel()
{
    if (!IsValid(ProceduralMesh))
    {
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    ProceduralMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    GeneratedVertexCount = 0;
    GeneratedTriangleCount = 0;
}