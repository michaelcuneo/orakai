#include "CubusCore/Rendering/CubusTerrainRayTracingProxyActor.h"

#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

ACubusTerrainRayTracingProxyActor::ACubusTerrainRayTracingProxyActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProxyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
        TEXT("CubusTerrainRayTracingProxy")
    );

    SetRootComponent(ProxyMesh);

    ProxyMesh->SetMobility(EComponentMobility::Static);
    ProxyMesh->bUseAsyncCooking = false;
    ProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProxyMesh->SetGenerateOverlapEvents(false);
    ProxyMesh->SetCastShadow(true);
    ProxyMesh->SetRenderInMainPass(false);
    ProxyMesh->SetRenderInDepthPass(false);
    ProxyMesh->SetVisibleInRayTracing(false);
}

bool ACubusTerrainRayTracingProxyActor::BuildFromSource(
    ACubusPCGVoxelVolumeActor* InSourceChunk,
    const int32 InSourceRevision
)
{
    if (
        !IsValid(InSourceChunk) ||
        !IsValid(ProxyMesh) ||
        SourceRevision != INDEX_NONE
    )
    {
        return false;
    }

    const UProceduralMeshComponent* SourceMesh =
        Cast<UProceduralMeshComponent>(InSourceChunk->GetRootComponent());

    if (!IsValid(SourceMesh))
    {
        return false;
    }

    const int32 SectionCount = SourceMesh->GetNumSections();

    if (SectionCount <= 0)
    {
        return false;
    }

    SetActorTransform(InSourceChunk->GetActorTransform());

    int32 CopiedSectionCount = 0;

    for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        const FProcMeshSection* SourceSection =
            SourceMesh->GetProcMeshSection(SectionIndex);

        if (
            SourceSection == nullptr ||
            SourceSection->ProcVertexBuffer.Num() == 0 ||
            SourceSection->ProcIndexBuffer.Num() == 0
        )
        {
            continue;
        }

        TArray<FVector> Vertices;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;

        const int32 VertexCount = SourceSection->ProcVertexBuffer.Num();
        Vertices.Reserve(VertexCount);
        Normals.Reserve(VertexCount);
        UV0.Reserve(VertexCount);
        VertexColors.Reserve(VertexCount);
        Tangents.Reserve(VertexCount);

        for (const FProcMeshVertex& SourceVertex : SourceSection->ProcVertexBuffer)
        {
            Vertices.Add(SourceVertex.Position);
            Normals.Add(SourceVertex.Normal);
            UV0.Add(SourceVertex.UV0);
            VertexColors.Add(FLinearColor(SourceVertex.Color));
            Tangents.Add(SourceVertex.Tangent);
        }

        ProxyMesh->CreateMeshSection_LinearColor(
            SectionIndex,
            Vertices,
            SourceSection->ProcIndexBuffer,
            Normals,
            UV0,
            VertexColors,
            Tangents,
            false
        );

        ProxyMesh->SetMaterial(
            SectionIndex,
            SourceMesh->GetMaterial(SectionIndex)
        );

        ++CopiedSectionCount;
    }

    if (CopiedSectionCount <= 0)
    {
        ProxyMesh->ClearAllMeshSections();
        return false;
    }

    SourceChunk = InSourceChunk;
    SourceRevision = InSourceRevision;

    ProxyMesh->SetVisibleInRayTracing(true);
    ProxyMesh->MarkRenderStateDirty();

    return true;
}

void ACubusTerrainRayTracingProxyActor::BeginRetire(
    const float DelaySeconds
)
{
    if (bRetiring)
    {
        return;
    }

    bRetiring = true;

    if (IsValid(ProxyMesh))
    {
        ProxyMesh->SetVisibleInRayTracing(false);
        ProxyMesh->MarkRenderStateDirty();
    }

    SetLifeSpan(FMath::Max(0.25f, DelaySeconds));
}

void ACubusTerrainRayTracingProxyActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (IsValid(ProxyMesh))
    {
        ProxyMesh->SetVisibleInRayTracing(false);
        ProxyMesh->MarkRenderStateDirty();
    }

    Super::EndPlay(EndPlayReason);
}
