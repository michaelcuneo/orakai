#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusScaledDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "HAL/PlatformTime.h"

ACubusTerrainLodWorldActor::ACubusTerrainLodWorldActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ACubusTerrainLodWorldActor::BeginPlay()
{
    Super::BeginPlay();

    ResolveBlockWorld();
    TimeUntilStreamingUpdate = 0.0f;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus terrain LOD started: enabled=%s stride=%d inner=%d outer=%d vertical=%d concurrent=%d uploads=%d"
        ),
        bEnableTerrainLod ? TEXT("true") : TEXT("false"),
        Lod1CanonicalVoxelStride,
        Lod1InnerRadiusTiles,
        Lod1OuterRadiusTiles,
        Lod1VerticalRadiusTiles,
        MaxConcurrentLodBuilds,
        MaxLodUploadsPerTick
    );
}

void ACubusTerrainLodWorldActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ResolveBlockWorld();

    CollectCompletedBuilds();
    UploadCompletedBuilds();

    TimeUntilStreamingUpdate -= DeltaSeconds;

    if (TimeUntilStreamingUpdate <= 0.0f)
    {
        TimeUntilStreamingUpdate =
            FMath::Max(0.05f, LodStreamingUpdateInterval);

        UpdateStreaming();
    }

    StartPendingBuilds();

    LoadedLodTileCount = TileComponents.Num();
    BuildingLodTileCount = ActiveBuilds.Num();
    PendingLodTileCount = PendingTiles.Num();
}

void ACubusTerrainLodWorldActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    ClearAllTiles();
    Super::EndPlay(EndPlayReason);
}

void ACubusTerrainLodWorldActor::ResolveBlockWorld()
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

    for (TActorIterator<ACubusBlockWorldActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            BlockWorld = *It;
            break;
        }
    }
}

void ACubusTerrainLodWorldActor::UpdateStreaming()
{
    if (!bEnableTerrainLod || !IsValid(BlockWorld))
    {
        return;
    }

    const APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (
        !IsValid(PlayerController) ||
        !IsValid(PlayerController->PlayerCameraManager)
    )
    {
        return;
    }

    ACubusVoxelVolumeActor* SnapshotChunk = nullptr;

    for (const auto& Pair : BlockWorld->GetRegisteredChunks())
    {
        ACubusVoxelVolumeActor* Candidate = Pair.Value.Get();

        if (
            IsValid(Candidate) &&
            Candidate->GetChunkData() != nullptr
        )
        {
            SnapshotChunk = Candidate;
            break;
        }
    }

    if (!IsValid(SnapshotChunk))
    {
        return;
    }

    const float CanonicalVoxelSize =
        FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

    const int32 SafeStride =
        FMath::Clamp(Lod1CanonicalVoxelStride, 2, 64);

    const float CanonicalChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        CanonicalVoxelSize;

    const float TileWorldSize =
        CanonicalChunkWorldSize *
        static_cast<float>(SafeStride);

    const FVector WorldGridOrigin =
        ResolveWorldGridOrigin(CanonicalChunkWorldSize);

    const FVector CameraLocation =
        PlayerController->PlayerCameraManager->GetCameraLocation();

    const FVector CameraGridLocation =
        CameraLocation - WorldGridOrigin;

    const FIntVector CentreTile(
        FMath::FloorToInt(
            (CameraGridLocation.X + TileWorldSize * 0.5f) /
            TileWorldSize
        ),
        FMath::FloorToInt(
            (CameraGridLocation.Y + TileWorldSize * 0.5f) /
            TileWorldSize
        ),
        FMath::FloorToInt(
            (CameraGridLocation.Z + TileWorldSize * 0.5f) /
            TileWorldSize
        )
    );

    if (CentreTile == LastCentreTile && !RequiredTiles.IsEmpty())
    {
        return;
    }

    LastCentreTile = CentreTile;
    RequiredTiles.Reset();
    PendingTiles.Reset();

    const int32 SafeInnerRadius =
        FMath::Max(0, Lod1InnerRadiusTiles);

    const int32 SafeOuterRadius =
        FMath::Max(SafeInnerRadius + 1, Lod1OuterRadiusTiles);

    const int32 SafeVerticalRadius =
        FMath::Clamp(Lod1VerticalRadiusTiles, 0, 4);

    for (int32 Z = -SafeVerticalRadius; Z <= SafeVerticalRadius; ++Z)
    {
        for (int32 Y = -SafeOuterRadius; Y <= SafeOuterRadius; ++Y)
        {
            for (int32 X = -SafeOuterRadius; X <= SafeOuterRadius; ++X)
            {
                const int32 HorizontalDistance =
                    FMath::Max(FMath::Abs(X), FMath::Abs(Y));

                if (
                    HorizontalDistance <= SafeInnerRadius ||
                    HorizontalDistance > SafeOuterRadius
                )
                {
                    continue;
                }

                RequiredTiles.Add(
                    CentreTile + FIntVector(X, Y, Z)
                );
            }
        }
    }

    RemoveUnneededTiles();

    for (const FIntVector& TileCoordinate : RequiredTiles)
    {
        const bool bCompletedPendingUpload =
            CompletedBuilds.ContainsByPredicate(
                [&TileCoordinate](const FCubusTerrainLodTileBuildResult& Result)
                {
                    return Result.TileCoordinate == TileCoordinate;
                }
            );

        if (
            TileComponents.Contains(TileCoordinate) ||
            TilesBuilding.Contains(TileCoordinate) ||
            bCompletedPendingUpload
        )
        {
            continue;
        }

        PendingTiles.Add(TileCoordinate);
    }

    PendingTiles.Sort(
        [CentreTile](const FIntVector& A, const FIntVector& B)
        {
            const int32 DistanceA =
                FMath::Abs(A.X - CentreTile.X) +
                FMath::Abs(A.Y - CentreTile.Y) +
                FMath::Abs(A.Z - CentreTile.Z);

            const int32 DistanceB =
                FMath::Abs(B.X - CentreTile.X) +
                FMath::Abs(B.Y - CentreTile.Y) +
                FMath::Abs(B.Z - CentreTile.Z);

            // Pop() returns nearest-first.
            return DistanceA > DistanceB;
        }
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus terrain LOD window: centre=(%d,%d,%d) required=%d loaded=%d pending=%d building=%d tile=%.0fm"
        ),
        CentreTile.X,
        CentreTile.Y,
        CentreTile.Z,
        RequiredTiles.Num(),
        TileComponents.Num(),
        PendingTiles.Num(),
        ActiveBuilds.Num(),
        TileWorldSize / 100.0f
    );
}

void ACubusTerrainLodWorldActor::CollectCompletedBuilds()
{
    for (int32 BuildIndex = ActiveBuilds.Num() - 1; BuildIndex >= 0; --BuildIndex)
    {
        FCubusTerrainLodTileBuild& Build = ActiveBuilds[BuildIndex];

        if (!Build.Task.IsCompleted())
        {
            continue;
        }

        FCubusTerrainLodTileBuildResult Result =
            MoveTemp(Build.Task.GetResult());

        TilesBuilding.Remove(Build.TileCoordinate);

        if (RequiredTiles.Contains(Result.TileCoordinate))
        {
            CompletedBuilds.Add(MoveTemp(Result));
        }

        ActiveBuilds.RemoveAtSwap(
            BuildIndex,
            1,
            EAllowShrinking::No
        );
    }
}

void ACubusTerrainLodWorldActor::StartPendingBuilds()
{
    if (
        !bEnableTerrainLod ||
        !IsValid(BlockWorld) ||
        PendingTiles.IsEmpty()
    )
    {
        return;
    }

    ACubusVoxelVolumeActor* SnapshotChunk = nullptr;

    for (const auto& Pair : BlockWorld->GetRegisteredChunks())
    {
        ACubusVoxelVolumeActor* Candidate = Pair.Value.Get();

        if (
            IsValid(Candidate) &&
            Candidate->GetChunkData() != nullptr
        )
        {
            SnapshotChunk = Candidate;
            break;
        }
    }

    if (!IsValid(SnapshotChunk))
    {
        return;
    }

    const int32 SafeConcurrentBuilds =
        FMath::Clamp(MaxConcurrentLodBuilds, 1, 16);

    const int32 SafeStartsPerTick =
        FMath::Clamp(MaxLodBuildStartsPerTick, 1, 16);

    const FCubusTerrainDensitySettings DensitySettings =
        SnapshotChunk->CaptureTerrainDensitySettings();

    const float CanonicalVoxelSize =
        FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

    const int32 SafeStride =
        FMath::Clamp(Lod1CanonicalVoxelStride, 2, 64);

    int32 StartedThisTick = 0;

    while (
        ActiveBuilds.Num() < SafeConcurrentBuilds &&
        StartedThisTick < SafeStartsPerTick &&
        !PendingTiles.IsEmpty()
    )
    {
        const FIntVector TileCoordinate =
            PendingTiles.Pop(EAllowShrinking::No);

        if (
            !RequiredTiles.Contains(TileCoordinate) ||
            TileComponents.Contains(TileCoordinate) ||
            TilesBuilding.Contains(TileCoordinate)
        )
        {
            continue;
        }

        FCubusTerrainLodTileBuildInput Input;
        Input.DensitySettings = DensitySettings;
        Input.TileCoordinate = TileCoordinate;
        Input.CanonicalVoxelStride = SafeStride;
        Input.CanonicalVoxelSize = CanonicalVoxelSize;
        Input.IsoLevel = 0.0f;

        FCubusTerrainLodTileBuild Build;
        Build.TileCoordinate = TileCoordinate;

        Build.Task = UE::Tasks::Launch(
            TEXT("CubusTerrainLodTile"),
            [Input]()
            {
                return ACubusTerrainLodWorldActor::BuildTile(Input);
            }
        );

        TilesBuilding.Add(TileCoordinate);
        ActiveBuilds.Add(MoveTemp(Build));
        ++StartedThisTick;
    }
}

void ACubusTerrainLodWorldActor::UploadCompletedBuilds()
{
    if (!IsValid(BlockWorld) || CompletedBuilds.IsEmpty())
    {
        return;
    }

    ACubusVoxelVolumeActor* SnapshotChunk = nullptr;

    for (const auto& Pair : BlockWorld->GetRegisteredChunks())
    {
        ACubusVoxelVolumeActor* Candidate = Pair.Value.Get();

        if (IsValid(Candidate))
        {
            SnapshotChunk = Candidate;
            break;
        }
    }

    if (!IsValid(SnapshotChunk))
    {
        return;
    }

    const float CanonicalVoxelSize =
        FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

    const int32 SafeStride =
        FMath::Clamp(Lod1CanonicalVoxelStride, 2, 64);

    const float TileWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        CanonicalVoxelSize *
        static_cast<float>(SafeStride);

    UMaterialInterface* TerrainMaterial = ResolveTerrainMaterial();

    const int32 UploadLimit =
        FMath::Clamp(MaxLodUploadsPerTick, 1, 16);

    int32 UploadedThisTick = 0;

    while (
        UploadedThisTick < UploadLimit &&
        !CompletedBuilds.IsEmpty()
    )
    {
        FCubusTerrainLodTileBuildResult Result =
            MoveTemp(CompletedBuilds[0]);

        CompletedBuilds.RemoveAt(
            0,
            1,
            EAllowShrinking::No
        );

        if (!RequiredTiles.Contains(Result.TileCoordinate))
        {
            continue;
        }

        UProceduralMeshComponent* Component =
            CreateTileComponent(
                Result.TileCoordinate,
                TileWorldSize
            );

        if (!IsValid(Component))
        {
            continue;
        }

        if (FCubusMeshData* MeshData =
                Result.MaterialMeshes.Find(
                    FCubusDensityMesher::UnifiedDensityMaterialKey
                ))
        {
            if (MeshData->IsValid())
            {
                Component->CreateMeshSection_LinearColor(
                    0,
                    MeshData->Vertices,
                    MeshData->Triangles,
                    MeshData->Normals,
                    MeshData->UV0,
                    MeshData->VertexColors,
                    MeshData->Tangents,
                    false
                );

                if (IsValid(TerrainMaterial))
                {
                    Component->SetMaterial(0, TerrainMaterial);
                }
            }
        }

        Component->SetVisibility(true);
        Component->SetHiddenInGame(false);
        Component->SetRenderInMainPass(true);
        Component->SetRenderInDepthPass(true);
        Component->MarkRenderStateDirty();

        TileComponents.Add(
            Result.TileCoordinate,
            Component
        );

        ++UploadedThisTick;
    }
}

void ACubusTerrainLodWorldActor::RemoveUnneededTiles()
{
    TArray<FIntVector> ExistingCoordinates;
    TileComponents.GetKeys(ExistingCoordinates);

    for (const FIntVector& TileCoordinate : ExistingCoordinates)
    {
        if (RequiredTiles.Contains(TileCoordinate))
        {
            continue;
        }

        if (TObjectPtr<UProceduralMeshComponent>* ComponentPtr =
                TileComponents.Find(TileCoordinate))
        {
            if (IsValid(ComponentPtr->Get()))
            {
                ComponentPtr->Get()->ClearAllMeshSections();
                ComponentPtr->Get()->DestroyComponent();
            }
        }

        TileComponents.Remove(TileCoordinate);
    }

    PendingTiles.RemoveAll(
        [this](const FIntVector& TileCoordinate)
        {
            return !RequiredTiles.Contains(TileCoordinate);
        }
    );

    CompletedBuilds.RemoveAll(
        [this](const FCubusTerrainLodTileBuildResult& Result)
        {
            return !RequiredTiles.Contains(Result.TileCoordinate);
        }
    );
}

void ACubusTerrainLodWorldActor::ClearAllTiles()
{
    for (const auto& Pair : TileComponents)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearAllMeshSections();
            Pair.Value->DestroyComponent();
        }
    }

    TileComponents.Reset();
    RequiredTiles.Reset();
    TilesBuilding.Reset();
    PendingTiles.Reset();
    ActiveBuilds.Reset();
    CompletedBuilds.Reset();

    LoadedLodTileCount = 0;
    BuildingLodTileCount = 0;
    PendingLodTileCount = 0;
}

UProceduralMeshComponent* ACubusTerrainLodWorldActor::CreateTileComponent(
    const FIntVector& TileCoordinate,
    const float TileWorldSize
)
{
    if (TObjectPtr<UProceduralMeshComponent>* Existing =
            TileComponents.Find(TileCoordinate))
    {
        return Existing->Get();
    }

    const FName ComponentName(
        *FString::Printf(
            TEXT("CubusTerrainLod1_%d_%d_%d"),
            TileCoordinate.X,
            TileCoordinate.Y,
            TileCoordinate.Z
        )
    );

    UProceduralMeshComponent* Component =
        NewObject<UProceduralMeshComponent>(
            this,
            ComponentName
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->RegisterComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->bUseAsyncCooking = true;

    const int32 SafeStride =
        FMath::Clamp(Lod1CanonicalVoxelStride, 2, 64);

    const float CanonicalChunkWorldSize =
        TileWorldSize /
        static_cast<float>(SafeStride);

    const FVector WorldGridOrigin =
        ResolveWorldGridOrigin(CanonicalChunkWorldSize);

    Component->SetWorldLocation(
        WorldGridOrigin +
        FVector(
            static_cast<double>(TileCoordinate.X) * TileWorldSize,
            static_cast<double>(TileCoordinate.Y) * TileWorldSize,
            static_cast<double>(TileCoordinate.Z) * TileWorldSize
        )
    );

    return Component;
}

UMaterialInterface* ACubusTerrainLodWorldActor::ResolveTerrainMaterial() const
{
    if (!IsValid(BlockWorld))
    {
        return nullptr;
    }

    for (const auto& Pair : BlockWorld->GetRegisteredChunks())
    {
        const ACubusVoxelVolumeActor* Chunk = Pair.Value.Get();

        if (!IsValid(Chunk))
        {
            continue;
        }

        UProceduralMeshComponent* TerrainMesh =
            Chunk->GetTerrainMeshComponent();

        if (!IsValid(TerrainMesh))
        {
            continue;
        }

        const int32 MaterialCount = TerrainMesh->GetNumMaterials();

        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            if (UMaterialInterface* Material = TerrainMesh->GetMaterial(MaterialIndex))
            {
                return Material;
            }
        }
    }

    return nullptr;
}

FVector ACubusTerrainLodWorldActor::ResolveWorldGridOrigin(
    const float CanonicalChunkWorldSize
) const
{
    if (!IsValid(BlockWorld))
    {
        return FVector::ZeroVector;
    }

    for (const auto& Pair : BlockWorld->GetRegisteredChunks())
    {
        const ACubusVoxelVolumeActor* Chunk = Pair.Value.Get();

        if (!IsValid(Chunk))
        {
            continue;
        }

        const FIntVector ChunkCoordinate =
            Chunk->GetChunkCoordinate();

        return Chunk->GetActorLocation() -
            FVector(
                static_cast<double>(ChunkCoordinate.X) *
                    CanonicalChunkWorldSize,
                static_cast<double>(ChunkCoordinate.Y) *
                    CanonicalChunkWorldSize,
                static_cast<double>(ChunkCoordinate.Z) *
                    CanonicalChunkWorldSize
            );
    }

    return FVector::ZeroVector;
}

FCubusTerrainLodTileBuildResult
ACubusTerrainLodWorldActor::BuildTile(
    const FCubusTerrainLodTileBuildInput& Input
)
{
    const double BuildStartTime = FPlatformTime::Seconds();

    FCubusTerrainLodTileBuildResult Result;
    Result.TileCoordinate = Input.TileCoordinate;

    const int32 SafeStride =
        FMath::Clamp(Input.CanonicalVoxelStride, 2, 64);

    const FCubusTerrainDensityField SourceField(
        Input.DensitySettings
    );

    const FVector LogicalOrigin(
        static_cast<double>(Input.TileCoordinate.X * Cubus::ChunkSize),
        static_cast<double>(Input.TileCoordinate.Y * Cubus::ChunkSize),
        static_cast<double>(Input.TileCoordinate.Z * Cubus::ChunkSize)
    );

    /*
     * Cubus chunk coordinates identify chunk centres, while canonical density
     * sample coordinate zero lies on the negative face of chunk zero.
     *
     * Scaling the 32-cell logical tile by N therefore needs an additional
     * half-extent correction of 16 * (N - 1) canonical samples. Without this,
     * a stride-4 LOD1 tile would be shifted by 48 metres relative to LOD0.
     */
    const double AlignmentOffset =
        static_cast<double>(Cubus::ChunkSize) *
        0.5 *
        static_cast<double>(SafeStride - 1);

    const FVector SourceOrigin =
        LogicalOrigin *
            static_cast<double>(SafeStride) -
        FVector(
            AlignmentOffset,
            AlignmentOffset,
            AlignmentOffset
        );

    const FCubusScaledDensityField ScaledField(
        SourceField,
        LogicalOrigin,
        SourceOrigin,
        static_cast<float>(SafeStride)
    );

    FCubusDensitySamplingBuffer DensityBuffer;
    DensityBuffer.Build(
        Input.TileCoordinate,
        ScaledField
    );

    FCubusDensityMesher::BuildChunk(
        DensityBuffer,
        Input.CanonicalVoxelSize *
            static_cast<float>(SafeStride),
        Input.IsoLevel,
        Result.MaterialMeshes,
        Result.GeneratedTriangleCount
    );

    Result.BuildTimeMilliseconds =
        (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;

    return Result;
}
