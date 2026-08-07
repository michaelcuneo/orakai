#include "CubusCore/Actors/CubusProceduralForestActor.h"

#include "CubusCore/Data/CubusTreeSpecies.h"
#include "CubusCore/Vegetation/CubusProceduralTreeGenerator.h"

#include "Kismet/KismetSystemLibrary.h"
#include "ProceduralMeshComponent.h"

namespace CubusProceduralForest
{
    void AppendSection(
        const FCubusTreeMeshSection& Source,
        const FTransform& Transform,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FLinearColor>& VertexColors
    )
    {
        const int32 BaseVertex = Vertices.Num();

        Vertices.Reserve(Vertices.Num() + Source.Vertices.Num());
        Normals.Reserve(Normals.Num() + Source.Normals.Num());
        UV0.Reserve(UV0.Num() + Source.UV0.Num());
        VertexColors.Reserve(VertexColors.Num() + Source.VertexColors.Num());
        Triangles.Reserve(Triangles.Num() + Source.Triangles.Num());

        for (const FVector& Vertex : Source.Vertices)
        {
            Vertices.Add(Transform.TransformPosition(Vertex));
        }

        for (const FVector& Normal : Source.Normals)
        {
            Normals.Add(Transform.TransformVectorNoScale(Normal).GetSafeNormal());
        }

        UV0.Append(Source.UV0);
        VertexColors.Append(Source.VertexColors);

        for (const int32 Index : Source.Triangles)
        {
            Triangles.Add(BaseVertex + Index);
        }
    }

    bool IsFarEnough(
        const FVector2D& Candidate,
        const TArray<FVector2D>& Existing,
        const float MinimumSpacingSquared
    )
    {
        for (const FVector2D& Point : Existing)
        {
            if (FVector2D::DistSquared(Candidate, Point) < MinimumSpacingSquared)
            {
                return false;
            }
        }

        return true;
    }
}

ACubusProceduralForestActor::ACubusProceduralForestActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ForestMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ForestMesh"));
    SetRootComponent(ForestMesh);

    ForestMesh->SetMobility(EComponentMobility::Static);
    ForestMesh->bUseAsyncCooking = true;
    ForestMesh->SetCollisionObjectType(ECC_WorldStatic);
    ForestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ForestMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ACubusProceduralForestActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (bRebuildOnConstruction)
    {
        RebuildForest();
    }
}

void ACubusProceduralForestActor::RandomizeForest()
{
    Seed = FMath::Rand();
    RebuildForest();
}

void ACubusProceduralForestActor::RebuildForest()
{
    GeneratedTreeCount = 0;
    BarkTriangleCount = 0;
    CanopyTriangleCount = 0;

    if (!IsValid(ForestMesh))
    {
        return;
    }

    ForestMesh->ClearAllMeshSections();

    if (!IsValid(Species))
    {
        return;
    }

    TArray<FVector> BarkVertices;
    TArray<int32> BarkTriangles;
    TArray<FVector> BarkNormals;
    TArray<FVector2D> BarkUV0;
    TArray<FLinearColor> BarkColors;

    TArray<FVector> CanopyVertices;
    TArray<int32> CanopyTriangles;
    TArray<FVector> CanopyNormals;
    TArray<FVector2D> CanopyUV0;
    TArray<FLinearColor> CanopyColors;

    FRandomStream Random(Seed);
    const int32 DesiredCount = FMath::Clamp(TreeCount, 1, 512);
    const float Radius = FMath::Max(100.0f, ForestRadius);
    const float MinimumSpacingSquared = FMath::Square(FMath::Max(0.0f, MinimumSpacing));
    const float MinScale = FMath::Max(0.05f, FMath::Min(ScaleRange.X, ScaleRange.Y));
    const float MaxScale = FMath::Max(MinScale, FMath::Max(ScaleRange.X, ScaleRange.Y));

    TArray<FVector2D> AcceptedPositions;
    AcceptedPositions.Reserve(DesiredCount);

    const int32 MaximumAttempts = DesiredCount * 32;
    for (int32 Attempt = 0;
        Attempt < MaximumAttempts && GeneratedTreeCount < DesiredCount;
        ++Attempt)
    {
        const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);
        const float Distance = FMath::Sqrt(Random.FRand()) * Radius;
        const FVector2D Candidate(
            FMath::Cos(Angle) * Distance,
            FMath::Sin(Angle) * Distance
        );

        if (!CubusProceduralForest::IsFarEnough(
            Candidate,
            AcceptedPositions,
            MinimumSpacingSquared
        ))
        {
            continue;
        }

        FVector LocalPosition(Candidate.X, Candidate.Y, 0.0f);

        if (bSnapTreesToGround && GetWorld())
        {
            const FVector WorldXY = GetActorTransform().TransformPosition(LocalPosition);
            const FVector TraceStart = WorldXY + FVector::UpVector * GroundTraceHeight;
            const FVector TraceEnd = WorldXY - FVector::UpVector * GroundTraceDepth;

            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CubusProceduralForestGround), false, this);
            if (GetWorld()->LineTraceSingleByChannel(
                Hit,
                TraceStart,
                TraceEnd,
                ECC_WorldStatic,
                Params
            ))
            {
                LocalPosition = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
            }
        }

        FCubusGeneratedTreeMesh GeneratedTree;
        const int32 TreeSeed = Random.RandRange(MIN_int32, MAX_int32);
        if (!FCubusProceduralTreeGenerator::BuildTree(
            *Species,
            TreeSeed,
            GeneratedTree
        ))
        {
            continue;
        }

        const float Scale = Random.FRandRange(MinScale, MaxScale);
        const float Yaw = Random.FRandRange(0.0f, 360.0f);
        const FTransform TreeTransform(
            FRotator(0.0f, Yaw, 0.0f),
            LocalPosition,
            FVector(Scale)
        );

        CubusProceduralForest::AppendSection(
            GeneratedTree.Bark,
            TreeTransform,
            BarkVertices,
            BarkTriangles,
            BarkNormals,
            BarkUV0,
            BarkColors
        );

        if (GeneratedTree.Canopy.IsValid())
        {
            CubusProceduralForest::AppendSection(
                GeneratedTree.Canopy,
                TreeTransform,
                CanopyVertices,
                CanopyTriangles,
                CanopyNormals,
                CanopyUV0,
                CanopyColors
            );
        }

        AcceptedPositions.Add(Candidate);
        ++GeneratedTreeCount;
    }

    TArray<FProcMeshTangent> EmptyTangents;

    if (BarkTriangles.Num() > 0)
    {
        ForestMesh->CreateMeshSection_LinearColor(
            0,
            BarkVertices,
            BarkTriangles,
            BarkNormals,
            BarkUV0,
            BarkColors,
            EmptyTangents,
            bGenerateCollision
        );
        ForestMesh->SetMaterial(0, Species->BarkMaterial);
    }

    if (CanopyTriangles.Num() > 0)
    {
        ForestMesh->CreateMeshSection_LinearColor(
            1,
            CanopyVertices,
            CanopyTriangles,
            CanopyNormals,
            CanopyUV0,
            CanopyColors,
            EmptyTangents,
            false
        );
        ForestMesh->SetMaterial(1, Species->CanopyMaterial);
    }

    ForestMesh->SetCollisionEnabled(
        bGenerateCollision
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision
    );

    BarkTriangleCount = BarkTriangles.Num() / 3;
    CanopyTriangleCount = CanopyTriangles.Num() / 3;
}
