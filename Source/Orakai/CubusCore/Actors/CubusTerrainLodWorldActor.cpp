#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusScaledDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "HAL/PlatformTime.h"

ACubusTerrainLodWorldActor::ACubusTerrainLodWorldActor()
{
	PrimaryActorTick.bCanEverTick		   = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Lod1Runtime.LodLevel = 1;
	Lod2Runtime.LodLevel = 2;
	Lod3Runtime.LodLevel = 3;
}

void ACubusTerrainLodWorldActor::BeginPlay()
{
	Super::BeginPlay();

	ResolveBlockWorld();
	TimeUntilStreamingUpdate = 0.0f;

	UE_LOG(LogTemp, Display,
		   TEXT("Cubus terrain LOD started: enabled=%s lod1=(stride=%d overlap=%d outer=%d vertical=%d) lod2=(stride=%d overlap=%d "
				"outer=%d vertical=%d) lod3=(stride=%d overlap=%d outer=%d vertical=%d) concurrent=%d uploads=%d"),
		   bEnableTerrainLod ? TEXT("true") : TEXT("false"), Lod1CanonicalVoxelStride, Lod1OverlapTiles, Lod1OuterRadiusTiles,
		   Lod1VerticalRadiusTiles, Lod2CanonicalVoxelStride, Lod2OverlapTiles, Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles,
		   Lod3CanonicalVoxelStride, Lod3OverlapTiles, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles, MaxConcurrentLodBuilds,
		   MaxLodUploadsPerTick);
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
		TimeUntilStreamingUpdate = FMath::Max(0.05f, LodStreamingUpdateInterval);

		UpdateStreaming();
	}

	StartPendingBuilds();

	LoadedLodTileCount = Lod1Runtime.TileComponents.Num() + Lod2Runtime.TileComponents.Num() + Lod3Runtime.TileComponents.Num();

	BuildingLodTileCount = Lod1Runtime.ActiveBuilds.Num() + Lod2Runtime.ActiveBuilds.Num() + Lod3Runtime.ActiveBuilds.Num();

	PendingLodTileCount = Lod1Runtime.PendingTiles.Num() + Lod2Runtime.PendingTiles.Num() + Lod3Runtime.PendingTiles.Num();
}

void ACubusTerrainLodWorldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	ACubusVoxelVolumeActor* SnapshotChunk = nullptr;

	for (const auto& Pair : BlockWorld->GetRegisteredChunks())
	{
		ACubusVoxelVolumeActor* Candidate = Pair.Value.Get();

		if (IsValid(Candidate) && Candidate->GetChunkData() != nullptr)
		{
			SnapshotChunk = Candidate;
			break;
		}
	}

	if (!IsValid(SnapshotChunk))
	{
		return;
	}

	const float CanonicalVoxelSize = FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

	const float CanonicalChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;

	const FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);

	/*
	 * World ownership follows the controlled pawn, not the camera.
	 *
	 * A third-person boom camera can orbit, lag and cross a coarse tile
	 * boundary without the player moving through that boundary. Using it as
	 * the streaming origin makes terrain change underneath the character.
	 */
	const FVector StreamingGridLocation = PlayerPawn->GetActorLocation() - WorldGridOrigin;

	const int32 Lod0HorizontalRadiusChunks = FMath::Max(0, BlockWorld->GetClientHorizontalViewDistance());

	const double Lod0HalfExtentCanonicalChunks = static_cast<double>(Lod0HorizontalRadiusChunks) + 0.5;

	const int32 SafeLod1Stride = FMath::Clamp(Lod1CanonicalVoxelStride, 2, 64);

	UpdateTierStreaming(Lod1Runtime, StreamingGridLocation, CanonicalChunkWorldSize, Lod0HalfExtentCanonicalChunks, SafeLod1Stride,
						Lod1OverlapTiles, Lod1OuterRadiusTiles, Lod1VerticalRadiusTiles);

	const int32 SafeLod1OuterRadius = FMath::Max(Lod1Runtime.InnerRadiusTiles + 1, Lod1Runtime.OuterRadiusTiles);

	const double Lod1HalfExtentCanonicalChunks = (static_cast<double>(SafeLod1OuterRadius) + 0.5) * static_cast<double>(SafeLod1Stride);

	const int32 SafeLod2Stride = FMath::Clamp(Lod2CanonicalVoxelStride, SafeLod1Stride + 1, 255);

	UpdateTierStreaming(Lod2Runtime, StreamingGridLocation, CanonicalChunkWorldSize, Lod1HalfExtentCanonicalChunks, SafeLod2Stride,
						Lod2OverlapTiles, Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles);

	const int32 SafeLod2OuterRadius = FMath::Max(Lod2Runtime.InnerRadiusTiles + 1, Lod2Runtime.OuterRadiusTiles);

	const double Lod2HalfExtentCanonicalChunks = (static_cast<double>(SafeLod2OuterRadius) + 0.5) * static_cast<double>(SafeLod2Stride);

	const int32 SafeLod3Stride = FMath::Clamp(Lod3CanonicalVoxelStride, SafeLod2Stride + 1, 256);

	UpdateTierStreaming(Lod3Runtime, StreamingGridLocation, CanonicalChunkWorldSize, Lod2HalfExtentCanonicalChunks, SafeLod3Stride,
						Lod3OverlapTiles, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles);
}

void ACubusTerrainLodWorldActor::UpdateTierStreaming(FCubusTerrainLodTierRuntime& Tier, const FVector& StreamingGridLocation,
													 const float  CanonicalChunkWorldSize,
													 const double PreviousTierHalfExtentCanonicalChunks, const int32 CanonicalVoxelStride,
													 const int32 OverlapTiles, const int32 OuterRadiusTiles,
													 const int32 VerticalRadiusTiles)
{
	const int32 SafeStride = FMath::Clamp(CanonicalVoxelStride, 2, 256);

	const int32 SafeOverlapTiles = FMath::Clamp(OverlapTiles, 0, 2);

	const int32 SafeVerticalRadius = FMath::Clamp(VerticalRadiusTiles, 0, 4);

	const double PreviousRadiusInCurrentTiles = PreviousTierHalfExtentCanonicalChunks / static_cast<double>(SafeStride);

	const int32 FirstNonOverlappingTileDistance = FMath::CeilToInt(PreviousRadiusInCurrentTiles + 0.5);

	const int32 SafeInnerRadius = FMath::Max(0, FirstNonOverlappingTileDistance - 1 - SafeOverlapTiles);

	const int32 SafeOuterRadius = FMath::Max(SafeInnerRadius + 1, OuterRadiusTiles);

	const bool bConfigurationChanged = Tier.CanonicalVoxelStride != SafeStride || Tier.InnerRadiusTiles != SafeInnerRadius ||
									   Tier.OuterRadiusTiles != SafeOuterRadius || Tier.VerticalRadiusTiles != SafeVerticalRadius ||
									   Tier.OverlapTiles != SafeOverlapTiles;

	Tier.CanonicalVoxelStride = SafeStride;
	Tier.InnerRadiusTiles	  = SafeInnerRadius;
	Tier.OuterRadiusTiles	  = SafeOuterRadius;
	Tier.VerticalRadiusTiles  = SafeVerticalRadius;
	Tier.OverlapTiles		  = SafeOverlapTiles;

	const float TileWorldSize = CanonicalChunkWorldSize * static_cast<float>(SafeStride);

	const FIntVector CentreTile(FMath::FloorToInt((StreamingGridLocation.X + TileWorldSize * 0.5f) / TileWorldSize),
								FMath::FloorToInt((StreamingGridLocation.Y + TileWorldSize * 0.5f) / TileWorldSize),
								FMath::FloorToInt((StreamingGridLocation.Z + TileWorldSize * 0.5f) / TileWorldSize));

	if (CentreTile == Tier.LastCentreTile && !Tier.RequiredTiles.IsEmpty() && !bConfigurationChanged)
	{
		return;
	}

	Tier.LastCentreTile = CentreTile;
	Tier.RequiredTiles.Reset();
	Tier.PendingTiles.Reset();

	for (int32 Z = -SafeVerticalRadius; Z <= SafeVerticalRadius; ++Z)
	{
		for (int32 Y = -SafeOuterRadius; Y <= SafeOuterRadius; ++Y)
		{
			for (int32 X = -SafeOuterRadius; X <= SafeOuterRadius; ++X)
			{
				const int32 HorizontalDistance = FMath::Max(FMath::Abs(X), FMath::Abs(Y));

				if (HorizontalDistance <= SafeInnerRadius || HorizontalDistance > SafeOuterRadius)
				{
					continue;
				}

				Tier.RequiredTiles.Add(CentreTile + FIntVector(X, Y, Z));
			}
		}
	}

	/*
	 * Make-before-break streaming.
	 *
	 * Do not destroy the previous edge immediately when the window moves.
	 * Keep those tiles visible until every tile in the new window is resident,
	 * then retire the stale edge as one completed handoff.
	 */
	for (const FIntVector& TileCoordinate : Tier.RequiredTiles)
	{
		const bool bCompletedPendingUpload = Tier.CompletedBuilds.ContainsByPredicate(
			[&TileCoordinate](const FCubusTerrainLodTileBuildResult& Result) { return Result.TileCoordinate == TileCoordinate; });

		if (Tier.TileComponents.Contains(TileCoordinate) || Tier.TilesBuilding.Contains(TileCoordinate) || bCompletedPendingUpload)
		{
			continue;
		}

		Tier.PendingTiles.Add(TileCoordinate);
	}

	Tier.PendingTiles.Sort(
		[CentreTile](const FIntVector& A, const FIntVector& B)
		{
			const int32 DistanceA = FMath::Abs(A.X - CentreTile.X) + FMath::Abs(A.Y - CentreTile.Y) + FMath::Abs(A.Z - CentreTile.Z);

			const int32 DistanceB = FMath::Abs(B.X - CentreTile.X) + FMath::Abs(B.Y - CentreTile.Y) + FMath::Abs(B.Z - CentreTile.Z);

			return DistanceA > DistanceB;
		});

	bool bWindowResident = !Tier.RequiredTiles.IsEmpty();

	for (const FIntVector& TileCoordinate : Tier.RequiredTiles)
	{
		if (!Tier.TileComponents.Contains(TileCoordinate))
		{
			bWindowResident = false;
			break;
		}
	}

	if (bWindowResident)
	{
		RemoveUnneededTiles(Tier);
	}

	UE_LOG(LogTemp, Display,
		   TEXT("Cubus terrain LOD%d window: centre=(%d,%d,%d) stride=%d inner=%d overlap=%d outer=%d required=%d loaded=%d pending=%d "
				"building=%d tile=%.0fm"),
		   Tier.LodLevel, CentreTile.X, CentreTile.Y, CentreTile.Z, SafeStride, SafeInnerRadius, SafeOverlapTiles, SafeOuterRadius,
		   Tier.RequiredTiles.Num(), Tier.TileComponents.Num(), Tier.PendingTiles.Num(), Tier.ActiveBuilds.Num(), TileWorldSize / 100.0f);
}

void ACubusTerrainLodWorldActor::CollectCompletedBuilds()
{
	CollectCompletedBuildsForTier(Lod1Runtime);
	CollectCompletedBuildsForTier(Lod2Runtime);
	CollectCompletedBuildsForTier(Lod3Runtime);
}

void ACubusTerrainLodWorldActor::CollectCompletedBuildsForTier(FCubusTerrainLodTierRuntime& Tier)
{
	for (int32 BuildIndex = Tier.ActiveBuilds.Num() - 1; BuildIndex >= 0; --BuildIndex)
	{
		FCubusTerrainLodTileBuild& Build = Tier.ActiveBuilds[BuildIndex];

		if (!Build.Task.IsCompleted())
		{
			continue;
		}

		FCubusTerrainLodTileBuildResult Result = MoveTemp(Build.Task.GetResult());

		Tier.TilesBuilding.Remove(Build.TileCoordinate);

		if (Tier.RequiredTiles.Contains(Result.TileCoordinate))
		{
			Tier.CompletedBuilds.Add(MoveTemp(Result));
		}

		Tier.ActiveBuilds.RemoveAtSwap(BuildIndex, 1, EAllowShrinking::No);
	}
}

void ACubusTerrainLodWorldActor::StartPendingBuilds()
{
	if (!bEnableTerrainLod || !IsValid(BlockWorld))
	{
		return;
	}

	if (Lod1Runtime.PendingTiles.IsEmpty() && Lod2Runtime.PendingTiles.IsEmpty() && Lod3Runtime.PendingTiles.IsEmpty())
	{
		return;
	}

	ACubusVoxelVolumeActor* SnapshotChunk = nullptr;

	for (const auto& Pair : BlockWorld->GetRegisteredChunks())
	{
		ACubusVoxelVolumeActor* Candidate = Pair.Value.Get();

		if (IsValid(Candidate) && Candidate->GetChunkData() != nullptr)
		{
			SnapshotChunk = Candidate;
			break;
		}
	}

	if (!IsValid(SnapshotChunk))
	{
		return;
	}

	const int32 SafeConcurrentBuilds = FMath::Clamp(MaxConcurrentLodBuilds, 1, 16);

	const int32 SafeStartsPerTick = FMath::Clamp(MaxLodBuildStartsPerTick, 1, 16);

	const FCubusTerrainDensitySettings DensitySettings = SnapshotChunk->CaptureTerrainDensitySettings();

	const float CanonicalVoxelSize = FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

	int32 StartedThisTick = 0;

	while (StartedThisTick < SafeStartsPerTick)
	{
		const int32 ActiveBuildCount = Lod1Runtime.ActiveBuilds.Num() + Lod2Runtime.ActiveBuilds.Num() + Lod3Runtime.ActiveBuilds.Num();

		if (ActiveBuildCount >= SafeConcurrentBuilds)
		{
			break;
		}

		FCubusTerrainLodTierRuntime* Tier = nullptr;

		if (!Lod1Runtime.PendingTiles.IsEmpty())
		{
			Tier = &Lod1Runtime;
		}
		else if (!Lod2Runtime.PendingTiles.IsEmpty())
		{
			Tier = &Lod2Runtime;
		}
		else if (!Lod3Runtime.PendingTiles.IsEmpty())
		{
			Tier = &Lod3Runtime;
		}
		else
		{
			break;
		}

		const FIntVector TileCoordinate = Tier->PendingTiles.Pop(EAllowShrinking::No);

		if (!Tier->RequiredTiles.Contains(TileCoordinate) || Tier->TileComponents.Contains(TileCoordinate) ||
			Tier->TilesBuilding.Contains(TileCoordinate))
		{
			continue;
		}

		FCubusTerrainLodTileBuildInput Input;
		Input.DensitySettings	   = DensitySettings;
		Input.TileCoordinate	   = TileCoordinate;
		Input.CanonicalVoxelStride = Tier->CanonicalVoxelStride;
		Input.CanonicalVoxelSize   = CanonicalVoxelSize;
		Input.IsoLevel			   = 0.0f;

		FCubusTerrainLodTileBuild Build;
		Build.TileCoordinate = TileCoordinate;

		Build.Task = UE::Tasks::Launch(TEXT("CubusTerrainLodTile"), [Input]() { return ACubusTerrainLodWorldActor::BuildTile(Input); });

		Tier->TilesBuilding.Add(TileCoordinate);
		Tier->ActiveBuilds.Add(MoveTemp(Build));
		++StartedThisTick;
	}
}

void ACubusTerrainLodWorldActor::UploadCompletedBuilds()
{
	if (!IsValid(BlockWorld))
	{
		return;
	}

	if (Lod1Runtime.CompletedBuilds.IsEmpty() && Lod2Runtime.CompletedBuilds.IsEmpty() && Lod3Runtime.CompletedBuilds.IsEmpty())
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

	const float CanonicalVoxelSize = FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

	UMaterialInterface* TerrainMaterial = ResolveTerrainMaterial();

	const int32 UploadLimit = FMath::Clamp(MaxLodUploadsPerTick, 1, 16);

	int32  UploadedThisTick			  = 0;
	double WorkerMillisecondsThisTick = 0.0;

	const double UploadTickStart = FPlatformTime::Seconds();

	while (UploadedThisTick < UploadLimit)
	{
		bool bUploaded = false;

		if (!Lod1Runtime.CompletedBuilds.IsEmpty())
		{
			bUploaded = UploadOneCompletedBuild(Lod1Runtime, CanonicalVoxelSize, TerrainMaterial, WorkerMillisecondsThisTick);
		}
		else if (!Lod2Runtime.CompletedBuilds.IsEmpty())
		{
			bUploaded = UploadOneCompletedBuild(Lod2Runtime, CanonicalVoxelSize, TerrainMaterial, WorkerMillisecondsThisTick);
		}
		else if (!Lod3Runtime.CompletedBuilds.IsEmpty())
		{
			bUploaded = UploadOneCompletedBuild(Lod3Runtime, CanonicalVoxelSize, TerrainMaterial, WorkerMillisecondsThisTick);
		}
		else
		{
			break;
		}

		if (bUploaded)
		{
			++UploadedThisTick;
		}
	}

	const auto RetireStaleTilesIfResident = [this](FCubusTerrainLodTierRuntime& Tier)
	{
		if (Tier.RequiredTiles.IsEmpty())
		{
			return;
		}

		for (const FIntVector& TileCoordinate : Tier.RequiredTiles)
		{
			if (!Tier.TileComponents.Contains(TileCoordinate))
			{
				return;
			}
		}

		RemoveUnneededTiles(Tier);
	};

	RetireStaleTilesIfResident(Lod1Runtime);
	RetireStaleTilesIfResident(Lod2Runtime);
	RetireStaleTilesIfResident(Lod3Runtime);

	if (UploadedThisTick > 0)
	{
		const double UploadMilliseconds = (FPlatformTime::Seconds() - UploadTickStart) * 1000.0;

		UE_LOG(LogTemp, Display,
			   TEXT("Cubus terrain LOD upload: tiles=%d worker=%.2fms upload=%.2fms loaded=(lod1=%d lod2=%d lod3=%d) completed=(%d,%d,%d) "
					"building=(%d,%d,%d) pending=(%d,%d,%d)"),
			   UploadedThisTick, WorkerMillisecondsThisTick, UploadMilliseconds, Lod1Runtime.TileComponents.Num(),
			   Lod2Runtime.TileComponents.Num(), Lod3Runtime.TileComponents.Num(), Lod1Runtime.CompletedBuilds.Num(),
			   Lod2Runtime.CompletedBuilds.Num(), Lod3Runtime.CompletedBuilds.Num(), Lod1Runtime.ActiveBuilds.Num(),
			   Lod2Runtime.ActiveBuilds.Num(), Lod3Runtime.ActiveBuilds.Num(), Lod1Runtime.PendingTiles.Num(),
			   Lod2Runtime.PendingTiles.Num(), Lod3Runtime.PendingTiles.Num());
	}
}

bool ACubusTerrainLodWorldActor::UploadOneCompletedBuild(FCubusTerrainLodTierRuntime& Tier, const float CanonicalVoxelSize,
														 UMaterialInterface* TerrainMaterial, double& WorkerMilliseconds)
{
	if (Tier.CompletedBuilds.IsEmpty())
	{
		return false;
	}

	FCubusTerrainLodTileBuildResult Result = MoveTemp(Tier.CompletedBuilds[0]);

	Tier.CompletedBuilds.RemoveAt(0, 1, EAllowShrinking::No);

	if (!Tier.RequiredTiles.Contains(Result.TileCoordinate))
	{
		return false;
	}

	const float TileWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize * static_cast<float>(Tier.CanonicalVoxelStride);

	UProceduralMeshComponent* Component = CreateTileComponent(Tier, Result.TileCoordinate, TileWorldSize);

	if (!IsValid(Component))
	{
		return false;
	}

	if (FCubusMeshData* MeshData = Result.MaterialMeshes.Find(FCubusDensityMesher::UnifiedDensityMaterialKey))
	{
		if (MeshData->IsValid())
		{
			Component->CreateMeshSection_LinearColor(0, MeshData->Vertices, MeshData->Triangles, MeshData->Normals, MeshData->UV0,
													 MeshData->VertexColors, MeshData->Tangents, false);

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

	Tier.TileComponents.Add(Result.TileCoordinate, Component);

	WorkerMilliseconds += Result.BuildTimeMilliseconds;
	return true;
}

void ACubusTerrainLodWorldActor::RemoveUnneededTiles(FCubusTerrainLodTierRuntime& Tier)
{
	TArray<FIntVector> ExistingCoordinates;
	Tier.TileComponents.GetKeys(ExistingCoordinates);

	for (const FIntVector& TileCoordinate : ExistingCoordinates)
	{
		if (Tier.RequiredTiles.Contains(TileCoordinate))
		{
			continue;
		}

		if (TObjectPtr<UProceduralMeshComponent>* ComponentPtr = Tier.TileComponents.Find(TileCoordinate))
		{
			if (IsValid(ComponentPtr->Get()))
			{
				ComponentPtr->Get()->ClearAllMeshSections();
				ComponentPtr->Get()->DestroyComponent();
			}
		}

		Tier.TileComponents.Remove(TileCoordinate);
	}

	Tier.PendingTiles.RemoveAll([&Tier](const FIntVector& TileCoordinate) { return !Tier.RequiredTiles.Contains(TileCoordinate); });

	Tier.CompletedBuilds.RemoveAll([&Tier](const FCubusTerrainLodTileBuildResult& Result)
								   { return !Tier.RequiredTiles.Contains(Result.TileCoordinate); });
}

void ACubusTerrainLodWorldActor::ClearAllTiles()
{
	ClearTier(Lod1Runtime);
	ClearTier(Lod2Runtime);
	ClearTier(Lod3Runtime);

	LoadedLodTileCount	 = 0;
	BuildingLodTileCount = 0;
	PendingLodTileCount	 = 0;
}

void ACubusTerrainLodWorldActor::ClearTier(FCubusTerrainLodTierRuntime& Tier)
{
	for (const auto& Pair : Tier.TileComponents)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->ClearAllMeshSections();
			Pair.Value->DestroyComponent();
		}
	}

	Tier.TileComponents.Reset();
	Tier.RequiredTiles.Reset();
	Tier.TilesBuilding.Reset();
	Tier.PendingTiles.Reset();
	Tier.ActiveBuilds.Reset();
	Tier.CompletedBuilds.Reset();
	Tier.LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);
}

UProceduralMeshComponent* ACubusTerrainLodWorldActor::CreateTileComponent(FCubusTerrainLodTierRuntime& Tier,
																		  const FIntVector& TileCoordinate, const float TileWorldSize)
{
	if (TObjectPtr<UProceduralMeshComponent>* Existing = Tier.TileComponents.Find(TileCoordinate))
	{
		return Existing->Get();
	}

	const FName ComponentName(
		*FString::Printf(TEXT("CubusTerrainLod%d_%d_%d_%d"), Tier.LodLevel, TileCoordinate.X, TileCoordinate.Y, TileCoordinate.Z));

	UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(this, ComponentName);

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
	Component->SetVisibleInRayTracing(false);
	Component->bUseAsyncCooking = true;

	const int32 SafeStride = FMath::Clamp(Tier.CanonicalVoxelStride, 2, 256);

	const float CanonicalChunkWorldSize = TileWorldSize / static_cast<float>(SafeStride);

	const FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);

	Component->SetWorldLocation(WorldGridOrigin + FVector(static_cast<double>(TileCoordinate.X) * TileWorldSize,
														  static_cast<double>(TileCoordinate.Y) * TileWorldSize,
														  static_cast<double>(TileCoordinate.Z) * TileWorldSize));

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

		UProceduralMeshComponent* TerrainMesh = Chunk->GetTerrainMeshComponent();

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

FVector ACubusTerrainLodWorldActor::ResolveWorldGridOrigin(const float CanonicalChunkWorldSize) const
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

		const FIntVector ChunkCoordinate = Chunk->GetChunkCoordinate();

		return Chunk->GetActorLocation() - FVector(static_cast<double>(ChunkCoordinate.X) * CanonicalChunkWorldSize,
												   static_cast<double>(ChunkCoordinate.Y) * CanonicalChunkWorldSize,
												   static_cast<double>(ChunkCoordinate.Z) * CanonicalChunkWorldSize);
	}

	return FVector::ZeroVector;
}

FCubusTerrainLodTileBuildResult ACubusTerrainLodWorldActor::BuildTile(const FCubusTerrainLodTileBuildInput& Input)
{
	const double BuildStartTime = FPlatformTime::Seconds();

	FCubusTerrainLodTileBuildResult Result;
	Result.TileCoordinate = Input.TileCoordinate;

	const int32 SafeStride = FMath::Clamp(Input.CanonicalVoxelStride, 2, 256);

	const FCubusTerrainDensityField SourceField(Input.DensitySettings);

	const FVector LogicalOrigin(static_cast<double>(Input.TileCoordinate.X * Cubus::ChunkSize),
								static_cast<double>(Input.TileCoordinate.Y * Cubus::ChunkSize),
								static_cast<double>(Input.TileCoordinate.Z * Cubus::ChunkSize));

	/*
	 * Coarse tiles must be a lower-resolution view of the exact same absolute
	 * density field as LOD0. No centring offset belongs here: the component is
	 * already placed at TileCoordinate * ChunkSize * stride in world space.
	 * Offsetting source X/Y sampled a different landscape in every LOD tier;
	 * offsetting Z lifted the outer rings by half a coarse tile (1008 canonical
	 * voxels at stride 64), producing the false mountain/snow horizon ring.
	 */
	const FVector SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);

	const FCubusScaledDensityField ScaledField(SourceField, LogicalOrigin, SourceOrigin, static_cast<float>(SafeStride));

	FCubusDensitySamplingBuffer DensityBuffer;
	DensityBuffer.Build(Input.TileCoordinate, ScaledField);

	FCubusDensityMesher::BuildChunk(DensityBuffer, Input.CanonicalVoxelSize * static_cast<float>(SafeStride), Input.IsoLevel,
									Result.MaterialMeshes, Result.GeneratedTriangleCount, &ScaledField);

	Result.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;

	return Result;
}
