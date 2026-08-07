#include "CubusCore/Actors/CubusProceduralTreeActor.h"

#include "CubusCore/Data/CubusTreeSpecies.h"
#include "CubusCore/Vegetation/CubusProceduralTreeGenerator.h"
#include "ProceduralMeshComponent.h"

ACubusProceduralTreeActor::ACubusProceduralTreeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    TreeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TreeMesh"));
    SetRootComponent(TreeMesh);

    TreeMesh->SetMobility(EComponentMobility::Static);
    TreeMesh->SetCanEverAffectNavigation(true);
    TreeMesh->bUseAsyncCooking = true;
    TreeMesh->SetCastShadow(true);
}

void ACubusProceduralTreeActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (bRebuildOnConstruction)
    {
        RebuildTree();
    }
}

void ACubusProceduralTreeActor::RebuildTree()
{
    BarkTriangleCount = 0;
    CanopyTriangleCount = 0;

    if (!IsValid(TreeMesh))
    {
        return;
    }

    TreeMesh->ClearAllMeshSections();

    if (!IsValid(Species))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus procedural tree has no species asset."));
        return;
    }

    FCubusGeneratedTreeMesh Generated;
    if (!FCubusProceduralTreeGenerator::BuildTree(*Species, Seed, Generated))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus failed to generate tree species %s with seed %d."),
            *Species->GetName(),
            Seed
        );
        return;
    }

    TArray<FProcMeshTangent> EmptyTangents;

    TreeMesh->CreateMeshSection_LinearColor(
        0,
        Generated.Bark.Vertices,
        Generated.Bark.Triangles,
        Generated.Bark.Normals,
        Generated.Bark.UV0,
        Generated.Bark.VertexColors,
        EmptyTangents,
        bGenerateCollision
    );

    if (Generated.Canopy.IsValid())
    {
        TreeMesh->CreateMeshSection_LinearColor(
            1,
            Generated.Canopy.Vertices,
            Generated.Canopy.Triangles,
            Generated.Canopy.Normals,
            Generated.Canopy.UV0,
            Generated.Canopy.VertexColors,
            EmptyTangents,
            false
        );
    }

    if (IsValid(Species->BarkMaterial))
    {
        TreeMesh->SetMaterial(0, Species->BarkMaterial);
    }
    if (IsValid(Species->CanopyMaterial))
    {
        TreeMesh->SetMaterial(1, Species->CanopyMaterial);
    }

    BarkTriangleCount = Generated.Bark.Triangles.Num() / 3;
    CanopyTriangleCount = Generated.Canopy.Triangles.Num() / 3;
}

void ACubusProceduralTreeActor::RandomizeSeed()
{
    Seed = FMath::Rand();
    RebuildTree();
}