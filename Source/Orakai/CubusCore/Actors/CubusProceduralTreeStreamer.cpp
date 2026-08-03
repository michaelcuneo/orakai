#include "CubusCore/Actors/CubusProceduralTreeStreamer.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusTreeSpecies.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Vegetation/CubusProceduralTreeGenerator.h"
#include "CubusCore/Vegetation/CubusVegetationChunkFilter.h"
#include "Camera/PlayerCameraManager.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    constexpr int32 BroadleafType = 3;
    constexpr int32 ConiferType = 6;

    uint32 CalculateTreeSignature(const FCubusBlockChunkData& ChunkData)
    {
        uint32 Hash = 0;
        int32 TreeCount = 0;

        for (const FCubusVegetationInstance& Instance : ChunkData.GetVegetationInstances())
        {
            if (Instance.TypeId != BroadleafType && Instance.TypeId != ConiferType)
            {
                continue;
            }

            Hash = HashCombineFast(Hash, GetTypeHash(Instance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(Instance.TypeId));
            Hash = HashCombineFast(Hash, GetTypeHash(Instance.Scale));
            ++TreeCount;
        }

        return HashCombineFast(Hash, GetTypeHash(TreeCount));
    }

    void AppendSection(
        const FCubusTreeMeshSection& Source,
        const FTransform& Transform,
        FCubusTreeMeshSection& Target
    )
    {
        const int32 VertexOffset = Target.Vertices.Num();

        for (int32 Index = 0; Index < Source.Vertices.Num(); ++Index)
        {
            Target.Vertices.Add(Transform.TransformPosition(Source.Vertices[Index]));
            Target.Normals.Add(Transform.TransformVectorNoScale(Source.Normals[Index]).GetSafeNormal());
            Target.UV0.Add(Source.UV0[Index]);
            Target.VertexColors.Add(Source.VertexColors[Index]);
        }

        for (const int32 TriangleIndex : Source.Triangles)
        {
            Target.Triangles.Add(VertexOffset + TriangleIndex);
        }
    }

    void CreateSection(
        UProceduralMeshComponent& Mesh,
        const int32 SectionIndex,
        const FCubusTreeMeshSection& Section,
        const bool bCollision
    )
    {
        if (!Section.IsValid())
        {
            return;
        }

        TArray<FProcMeshTangent> Tangents;
        Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), Section.Vertices.Num());

        Mesh.CreateMeshSection_LinearColor(
            SectionIndex,
            Section.Vertices,
            Section.Triangles,
            Section.Normals,
            Section.UV0,
            Section.VertexColors,
            Tangents,
            bCollision
        );
    }
}

ACubusProceduralTreeStreamer::ACubusProceduralTreeStreamer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ACubusProceduralTreeStreamer::BeginPlay()
{
    Super::BeginPlay();
    ResolveBlockWorld();
    RefreshVisibleChunks();
    TimeUntilRefresh = FMath::Max(0.1f, RefreshInterval);
}

void ACubusProceduralTreeStreamer::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TimeUntilRefresh -= DeltaSeconds;
    if (TimeUntilRefresh > 0.0f)
    {
        return;
    }

    TimeUntilRefresh = FMath::Max(0.1f, RefreshInterval);
    ResolveBlockWorld();
    RefreshVisibleChunks();
}

void ACubusProceduralTreeStreamer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearStreamedTrees();
    Super::EndPlay(EndPlayReason);
}

void ACubusProceduralTreeStreamer::RebuildStreamedTrees()
{
    ClearStreamedTrees();
    ResolveBlockWorld();
    RefreshVisibleChunks();
}

void ACubusProceduralTreeStreamer::ClearStreamedTrees()
{
    for (TPair<FIntVector, FCubusStreamedTreeChunk>& Pair : StreamedChunks)
    {
        if (IsValid(Pair.Value.Mesh))
        {
            Pair.Value.Mesh->DestroyComponent();
        }
    }

    StreamedChunks.Reset();
    StreamedChunkCount = 0;
    StreamedTreeCount = 0;
}

void ACubusProceduralTreeStreamer::ResolveBlockWorld()
{
    if (IsValid(BlockWorld))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        BlockWorld = *Iterator;
        break;
    }
}

bool ACubusProceduralTreeStreamer::IsChunkVisible(
    const ACubusVoxelVolumeActor& Chunk,
    const FVector& CameraLocation,
    const bool bHasCamera
) const
{
    if (!bCullByCameraChunkRadius || !bHasCamera)
    {
        return true;
    }

    return FCubusVegetationChunkFilter::IsWithinCameraRadius(
        &Chunk,
        CameraLocation,
        true,
        CameraChunkHorizontalRadius,
        CameraChunkVerticalRadius
    );
}

void ACubusProceduralTreeStreamer::RefreshVisibleChunks()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !IsValid(BlockWorld))
    {
        return;
    }

    const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    const bool bHasCamera = IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager);
    const FVector CameraLocation = bHasCamera
        ? PlayerController->PlayerCameraManager->GetCameraLocation()
        : FVector::ZeroVector;

    TSet<FIntVector> VisibleChunkCoordinates;
    int32 NewTreeCount = 0;

    for (TActorIterator<ACubusVoxelVolumeActor> Iterator(World); Iterator; ++Iterator)
    {
        ACubusVoxelVolumeActor* Chunk = *Iterator;
        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld ||
            !IsChunkVisible(*Chunk, CameraLocation, bHasCamera)
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();
        if (ChunkData == nullptr)
        {
            continue;
        }

        const FIntVector ChunkCoordinate = Chunk->GetChunkCoordinate();
        VisibleChunkCoordinates.Add(ChunkCoordinate);
        const uint32 Signature = CalculateTreeSignature(*ChunkData);

        const FCubusStreamedTreeChunk* Existing = StreamedChunks.Find(ChunkCoordinate);
        if (Existing == nullptr || Existing->Signature != Signature || !IsValid(Existing->Mesh))
        {
            BuildChunkTrees(*Chunk, Signature);
        }

        for (const FCubusVegetationInstance& Instance : ChunkData->GetVegetationInstances())
        {
            if (Instance.TypeId == BroadleafType || Instance.TypeId == ConiferType)
            {
                ++NewTreeCount;
            }
        }
    }

    TArray<FIntVector> ChunksToRemove;
    for (const TPair<FIntVector, FCubusStreamedTreeChunk>& Pair : StreamedChunks)
    {
        if (!VisibleChunkCoordinates.Contains(Pair.Key))
        {
            ChunksToRemove.Add(Pair.Key);
        }
    }

    for (const FIntVector& ChunkCoordinate : ChunksToRemove)
    {
        if (FCubusStreamedTreeChunk* Entry = StreamedChunks.Find(ChunkCoordinate))
        {
            if (IsValid(Entry->Mesh))
            {
                Entry->Mesh->DestroyComponent();
            }
        }
        StreamedChunks.Remove(ChunkCoordinate);
    }

    StreamedChunkCount = StreamedChunks.Num();
    StreamedTreeCount = NewTreeCount;
}

void ACubusProceduralTreeStreamer::BuildChunkTrees(
    ACubusVoxelVolumeActor& Chunk,
    const uint32 Signature
)
{
    const FCubusBlockChunkData* ChunkData = Chunk.GetChunkData();
    if (ChunkData == nullptr)
    {
        return;
    }

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    FCubusStreamedTreeChunk& Entry = StreamedChunks.FindOrAdd(ChunkCoordinate);

    if (!IsValid(Entry.Mesh))
    {
        Entry.Mesh = NewObject<UProceduralMeshComponent>(
            this,
            *FString::Printf(
                TEXT("ProceduralTrees_%d_%d_%d"),
                ChunkCoordinate.X,
                ChunkCoordinate.Y,
                ChunkCoordinate.Z
            )
        );
        Entry.Mesh->SetupAttachment(Root);
        Entry.Mesh->RegisterComponent();
        Entry.Mesh->SetMobility(EComponentMobility::Movable);
        Entry.Mesh->SetCollisionEnabled(
            bGenerateCollision
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
        Entry.Mesh->SetCastShadow(bCastShadows);
    }

    Entry.Mesh->ClearAllMeshSections();
    Entry.Signature = Signature;

    FCubusTreeMeshSection BroadleafBark;
    FCubusTreeMeshSection BroadleafCanopy;
    FCubusTreeMeshSection ConiferBark;
    FCubusTreeMeshSection ConiferCanopy;
    const float SafeVoxelSize = FMath::Max(1.0f, Chunk.GetVoxelSize());
    const double ChunkHalfWorldExtent =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(SafeVoxelSize) * 0.5;
    const FTransform ActorInverse = GetActorTransform().Inverse();
    const int32 SafeVariantCount = FMath::Clamp(VariantsPerSpecies, 1, 32);

    for (const FCubusVegetationInstance& Instance : ChunkData->GetVegetationInstances())
    {
        const UCubusTreeSpecies* Species = nullptr;
        FCubusTreeMeshSection* BarkTarget = nullptr;
        FCubusTreeMeshSection* CanopyTarget = nullptr;

        if (Instance.TypeId == BroadleafType)
        {
            Species = BroadleafSpecies;
            BarkTarget = &BroadleafBark;
            CanopyTarget = &BroadleafCanopy;
        }
        else if (Instance.TypeId == ConiferType)
        {
            Species = ConiferSpecies;
            BarkTarget = &ConiferBark;
            CanopyTarget = &ConiferCanopy;
        }

        if (!IsValid(Species) || BarkTarget == nullptr || CanopyTarget == nullptr)
        {
            continue;
        }

        const uint32 InstanceHash = HashCombineFast(
            GetTypeHash(Instance.WorldVoxel),
            HashCombineFast(GetTypeHash(Instance.TypeId), GetTypeHash(GenerationSeed))
        );
        const int32 VariantIndex = static_cast<int32>(InstanceHash % SafeVariantCount);
        const int32 TreeSeed = GenerationSeed + VariantIndex * 104729 + Instance.TypeId * 4099;

        FCubusGeneratedTreeMesh Generated;
        if (!FCubusProceduralTreeGenerator::BuildTree(*Species, TreeSeed, Generated))
        {
            continue;
        }

        const FVector WorldLocation(
            (static_cast<double>(Instance.WorldVoxel.X) + 0.5) * SafeVoxelSize - ChunkHalfWorldExtent,
            (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) * SafeVoxelSize - ChunkHalfWorldExtent,
            static_cast<double>(Instance.WorldVoxel.Z) * SafeVoxelSize - ChunkHalfWorldExtent
        );
        const float Scale = FMath::Max(0.01f, Instance.Scale * GlobalTreeScale);
        const FTransform WorldTreeTransform(
            FRotator(0.0f, Instance.RotationYaw, 0.0f),
            WorldLocation,
            FVector(Scale)
        );
        const FTransform LocalTreeTransform = WorldTreeTransform * ActorInverse;

        AppendSection(Generated.Bark, LocalTreeTransform, *BarkTarget);
        AppendSection(Generated.Canopy, LocalTreeTransform, *CanopyTarget);
    }

    CreateSection(*Entry.Mesh, 0, BroadleafBark, bGenerateCollision);
    CreateSection(*Entry.Mesh, 1, BroadleafCanopy, false);
    CreateSection(*Entry.Mesh, 2, ConiferBark, bGenerateCollision);
    CreateSection(*Entry.Mesh, 3, ConiferCanopy, false);

    if (IsValid(BroadleafSpecies))
    {
        if (IsValid(BroadleafSpecies->BarkMaterial))
        {
            Entry.Mesh->SetMaterial(0, BroadleafSpecies->BarkMaterial);
        }
        if (IsValid(BroadleafSpecies->CanopyMaterial))
        {
            Entry.Mesh->SetMaterial(1, BroadleafSpecies->CanopyMaterial);
        }
    }

    if (IsValid(ConiferSpecies))
    {
        if (IsValid(ConiferSpecies->BarkMaterial))
        {
            Entry.Mesh->SetMaterial(2, ConiferSpecies->BarkMaterial);
        }
        if (IsValid(ConiferSpecies->CanopyMaterial))
        {
            Entry.Mesh->SetMaterial(3, ConiferSpecies->CanopyMaterial);
        }
    }
}
