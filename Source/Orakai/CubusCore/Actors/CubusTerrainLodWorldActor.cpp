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
#include "HAL/IConsoleManager.h"

namespace CubusTerrainLodWorldActor
{
TAutoConsoleVariable<float> CVarTileBoundsScale(TEXT("cubus.Terrain.LodTileBoundsScale"), 1.5f,
												TEXT("Bounds scale multiplier for Cubus coarse terrain LOD tile meshes."), ECVF_Default);
}

ACubusTerrainLodWorldActor::ACubusTerrainLodWorldActor()
{
	PrimaryActorTick.bCanEverTick		   = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Lod1Runtime.LodLevel = 1;
	Lod2Runtime.LodLevel = 2;
	Lod3Runtime.LodLevel = 3;
	Lod4Runtime.LodLevel = 4;
	Lod5Runtime.LodLevel = 5;
	Lod6Runtime.LodLevel = 6;
}

void ACubusTerrainLodWorldActor::BeginPlay()
{
	Super::BeginPlay();

	ResolveBlockWorld();
	TimeUntilStreamingUpdate = 0.0f;

	UE_LOG(LogTemp, Display,
		   TEXT("Cubus terrain LOD started: enabled=%s fixed-strides=(2,4,8,16,32,64) outer=(%d,%d,%d,%d,%d,%d) concurrent=%d uploads=%d"),
		   bEnableTerrainLod ? TEXT("true") : TEXT("false"), Lod1OuterRadiusTiles, Lod2OuterRadiusTiles, Lod3OuterRadiusTiles,
		   Lod4OuterRadiusTiles, Lod5OuterRadiusTiles, Lod6OuterRadiusTiles, MaxConcurrentLodBuilds, MaxLodUploadsPerTick);
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

	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};

	LoadedLodTileCount	 = 0;
	BuildingLodTileCount = 0;
	PendingLodTileCount	 = 0;
	for (const FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		LoadedLodTileCount += Tier->TileComponents.Num();
		BuildingLodTileCount += Tier->ActiveBuilds.Num();
		PendingLodTileCount += Tier->PendingTiles.Num();
	}

	RetireStableTierWindows();
}

void ACubusTerrainLodWorldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllTiles();
	Super::EndPlay(EndPlayReason);
}

bool ACubusTerrainLodWorldActor::IsInitialVisualCoverageReady() const
{
	return IsTierWindowResident(Lod1Runtime);
}

float ACubusTerrainLodWorldActor::GetInitialVisualCoverageProgress() const
{
	if (Lod1Runtime.RequiredTiles.IsEmpty())
	{
		return 0.0f;
	}
	int32 ReadyCount = 0;
	for (const FIntVector& Coordinate : Lod1Runtime.RequiredTiles)
	{
		ReadyCount += Lod1Runtime.ResolvedTiles.Contains(Coordinate) ? 1 : 0;
	}
	return static_cast<float>(ReadyCount) / static_cast<float>(Lod1Runtime.RequiredTiles.Num());
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
	if (!bEnableTerrainLod || !IsValid(BlockWorld) || !BlockWorld->IsDensityStreamingCoverageReady())
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

	const float						CanonicalVoxelSize		= FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());
	const float						CanonicalChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;
	const FCubusTerrainDensityField DensityField(SnapshotChunk->CaptureTerrainDensitySettings());

	struct FTierUpdate
	{
		FCubusTerrainLodTierRuntime* Runtime;
		int32						 Stride;
		int32						 OuterRadius;
		int32						 VerticalRadius;
	};
	FTierUpdate TierUpdates[] = {{&Lod1Runtime, 2, Lod1OuterRadiusTiles, Lod1VerticalRadiusTiles},
								 {&Lod2Runtime, 4, Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles},
								 {&Lod3Runtime, 8, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles},
								 {&Lod4Runtime, 16, Lod4OuterRadiusTiles, Lod4VerticalRadiusTiles},
								 {&Lod5Runtime, 32, Lod5OuterRadiusTiles, Lod5VerticalRadiusTiles},
								 {&Lod6Runtime, 64, Lod6OuterRadiusTiles, Lod6VerticalRadiusTiles}};

	/*
	 * Every tier is derived from the exact committed bounds of the previous
	 * finer tier. No tier independently follows the pawn. Bounds are even and
	 * power-of-two aligned, so a parent face always coincides with a complete
	 * group of child faces and moves only on the child grid cadence.
	 */
	FCubusDensityTileBounds2D PreviousBounds = BlockWorld->GetDensityStreamingCoverageBounds();
	for (const FTierUpdate& Update : TierUpdates)
	{
		const FCubusDensityTileBounds2D InnerBounds		= FCubusDensityLod::ScaleDownExact(PreviousBounds, 2);
		const int32						MinimumHalfSpan = FMath::Max((InnerBounds.WidthX() + 1) / 2, (InnerBounds.WidthY() + 1) / 2);
		/* +1 preserves/slightly exceeds the old radius+0.5 physical reach while keeping an even tile width. */
		const int32						OuterHalfSpan = FMath::Max(MinimumHalfSpan + 1, Update.OuterRadius + 1);
		const FCubusDensityTileBounds2D OuterBounds	  = FCubusDensityLod::BuildAlignedOuterBounds(InnerBounds, OuterHalfSpan, 2);

		UpdateTierStreaming(*Update.Runtime, DensityField, CanonicalChunkWorldSize, InnerBounds, OuterBounds, Update.Stride,
							Update.VerticalRadius);
		PreviousBounds = OuterBounds;
	}
}

void ACubusTerrainLodWorldActor::UpdateTierStreaming(FCubusTerrainLodTierRuntime& Tier, const FCubusTerrainDensityField& DensityField,
													 const float CanonicalChunkWorldSize, const FCubusDensityTileBounds2D& InnerBounds,
													 const FCubusDensityTileBounds2D& OuterBounds, const int32 CanonicalVoxelStride,
													 const int32 VerticalRadiusTiles)
{
	const int32 SafeStride			  = FMath::Clamp(CanonicalVoxelStride, 2, 256);
	const int32 SafeVerticalRadius	  = FMath::Clamp(VerticalRadiusTiles, 0, 4);
	const bool	bConfigurationChanged = Tier.CanonicalVoxelStride != SafeStride || Tier.VerticalRadiusTiles != SafeVerticalRadius ||
										Tier.InnerBounds != InnerBounds || Tier.OuterBounds != OuterBounds;

	if (!bConfigurationChanged && !Tier.RequiredTiles.IsEmpty())
	{
		return;
	}

	Tier.CanonicalVoxelStride = SafeStride;
	Tier.VerticalRadiusTiles  = SafeVerticalRadius;
	Tier.InnerBounds		  = InnerBounds;
	Tier.OuterBounds		  = OuterBounds;
	Tier.RequiredTiles.Reset();
	Tier.PendingTiles.Reset();

	const double	TileSizeVoxels		  = static_cast<double>(Cubus::ChunkSize * SafeStride);
	constexpr int32 SurfaceSamplesPerAxis = 17;

	for (int32 TileY = OuterBounds.Min.Y; TileY < OuterBounds.MaxExclusive.Y; ++TileY)
	{
		for (int32 TileX = OuterBounds.Min.X; TileX < OuterBounds.MaxExclusive.X; ++TileX)
		{
			if (InnerBounds.Contains(TileX, TileY))
			{
				continue;
			}

			const double SourceMinimumX = static_cast<double>(TileX) * TileSizeVoxels;
			const double SourceMinimumY = static_cast<double>(TileY) * TileSizeVoxels;
			int32		 MinimumTileZ	= MAX_int32;
			int32		 MaximumTileZ	= MIN_int32;

			for (int32 SampleY = 0; SampleY < SurfaceSamplesPerAxis; ++SampleY)
			{
				for (int32 SampleX = 0; SampleX < SurfaceSamplesPerAxis; ++SampleX)
				{
					const double AlphaX = static_cast<double>(SampleX) / static_cast<double>(SurfaceSamplesPerAxis - 1);
					const double AlphaY = static_cast<double>(SampleY) / static_cast<double>(SurfaceSamplesPerAxis - 1);
					const float	 Height =
						DensityField.SampleSurfaceVoxelHeight(static_cast<float>(SourceMinimumX + AlphaX * TileSizeVoxels),
															  static_cast<float>(SourceMinimumY + AlphaY * TileSizeVoxels));
					const int32 SurfaceTileZ = FMath::FloorToInt(static_cast<double>(Height) / TileSizeVoxels);
					MinimumTileZ			 = FMath::Min(MinimumTileZ, SurfaceTileZ);
					MaximumTileZ			 = FMath::Max(MaximumTileZ, SurfaceTileZ);
				}
			}

			for (int32 TileZ = MinimumTileZ - SafeVerticalRadius; TileZ <= MaximumTileZ + SafeVerticalRadius; ++TileZ)
			{
				Tier.RequiredTiles.Add(FIntVector(TileX, TileY, TileZ));
			}
		}
	}

	/*
	 * A coordinate can remain in the ring while its inner-boundary face changes.
	 * Rebuild only those retained tiles whose transition signature changed.
	 * The old component stays visible until the replacement uploads.
	 */
	for (const FIntVector& TileCoordinate : Tier.RequiredTiles)
	{
		const uint32 ExpectedSignature = BuildTierTransitionFaces(Tier, TileCoordinate).GetSignature(Tier.MeshingSubdivisions);
		if (Tier.ResolvedTiles.Contains(TileCoordinate))
		{
			const uint32* ExistingSignature = Tier.ResolvedTransitionSignatures.Find(TileCoordinate);
			if (ExistingSignature == nullptr || *ExistingSignature != ExpectedSignature)
			{
				Tier.ResolvedTiles.Remove(TileCoordinate);
			}
		}

		const bool bCompletedPendingUpload = Tier.CompletedBuilds.ContainsByPredicate(
			[&TileCoordinate](const FCubusTerrainLodTileBuildResult& Result) { return Result.TileCoordinate == TileCoordinate; });
		if (!Tier.ResolvedTiles.Contains(TileCoordinate) && !Tier.TilesBuilding.Contains(TileCoordinate) && !bCompletedPendingUpload)
		{
			Tier.PendingTiles.Add(TileCoordinate);
		}
	}

	auto DistanceFromInner = [&InnerBounds](const FIntVector& Coordinate)
	{
		const int32 DeltaX = Coordinate.X < InnerBounds.Min.X
								 ? InnerBounds.Min.X - Coordinate.X
								 : (Coordinate.X >= InnerBounds.MaxExclusive.X ? Coordinate.X - InnerBounds.MaxExclusive.X + 1 : 0);
		const int32 DeltaY = Coordinate.Y < InnerBounds.Min.Y
								 ? InnerBounds.Min.Y - Coordinate.Y
								 : (Coordinate.Y >= InnerBounds.MaxExclusive.Y ? Coordinate.Y - InnerBounds.MaxExclusive.Y + 1 : 0);
		return FMath::Max(DeltaX, DeltaY);
	};
	Tier.PendingTiles.Sort(
		[&DistanceFromInner](const FIntVector& A, const FIntVector& B)
		{
			const int32 DistanceA = DistanceFromInner(A);
			const int32 DistanceB = DistanceFromInner(B);
			if (DistanceA != DistanceB)
			{
				return DistanceA > DistanceB; // Pop() => seam first.
			}
			return FMath::Abs(A.Z) > FMath::Abs(B.Z);
		});

	UE_LOG(LogTemp, Display,
		   TEXT("Cubus terrain LOD%d clipmap: stride=%d inner=[%d,%d)-[%d,%d) outer=[%d,%d)-[%d,%d) required=%d loaded=%d pending=%d "
				"building=%d tile=%.0fm"),
		   Tier.LodLevel, SafeStride, InnerBounds.Min.X, InnerBounds.Min.Y, InnerBounds.MaxExclusive.X, InnerBounds.MaxExclusive.Y,
		   OuterBounds.Min.X, OuterBounds.Min.Y, OuterBounds.MaxExclusive.X, OuterBounds.MaxExclusive.Y, Tier.RequiredTiles.Num(),
		   Tier.TileComponents.Num(), Tier.PendingTiles.Num(), Tier.ActiveBuilds.Num(),
		   CanonicalChunkWorldSize * static_cast<float>(SafeStride) / 100.0f);
}

void ACubusTerrainLodWorldActor::CollectCompletedBuilds()
{
	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};
	for (FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		CollectCompletedBuildsForTier(*Tier);
	}
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

	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};

	bool bHasPendingTiles = false;
	for (const FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		bHasPendingTiles |= !Tier->PendingTiles.IsEmpty();
	}
	if (!bHasPendingTiles)
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

	const bool	bPreSpawnCoverageIncomplete = !IsPreSpawnVisualCoverageReady();
	const int32 SafeConcurrentBuilds		= bPreSpawnCoverageIncomplete ? FMath::Max(12, FMath::Clamp(MaxConcurrentLodBuilds, 1, 16))
																		  : FMath::Clamp(MaxConcurrentLodBuilds, 1, 16);

	const int32 SafeStartsPerTick = bPreSpawnCoverageIncomplete ? FMath::Max(12, FMath::Clamp(MaxLodBuildStartsPerTick, 1, 16))
																: FMath::Clamp(MaxLodBuildStartsPerTick, 1, 16);

	const FCubusTerrainDensitySettings DensitySettings = SnapshotChunk->CaptureTerrainDensitySettings();

	const float CanonicalVoxelSize = FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());

	int32 StartedThisTick = 0;

	while (StartedThisTick < SafeStartsPerTick)
	{
		int32 ActiveBuildCount = 0;
		for (const FCubusTerrainLodTierRuntime* CandidateTier : Tiers)
		{
			ActiveBuildCount += CandidateTier->ActiveBuilds.Num();
		}

		if (ActiveBuildCount >= SafeConcurrentBuilds)
		{
			break;
		}

		FCubusTerrainLodTierRuntime* Tier = nullptr;
		for (int32 Offset = 0; Offset < UE_ARRAY_COUNT(Tiers); ++Offset)
		{
			const int32 TierIndex = bPreSpawnCoverageIncomplete ? Offset : (NextLodBuildTierIndex + Offset) % UE_ARRAY_COUNT(Tiers);
			FCubusTerrainLodTierRuntime* CandidateTier = Tiers[TierIndex];
			if (!CandidateTier->PendingTiles.IsEmpty())
			{
				Tier = CandidateTier;
				if (!bPreSpawnCoverageIncomplete)
				{
					NextLodBuildTierIndex = (TierIndex + 1) % UE_ARRAY_COUNT(Tiers);
				}
				break;
			}
		}
		if (Tier == nullptr)
		{
			break;
		}

		const FIntVector TileCoordinate = Tier->PendingTiles.Pop(EAllowShrinking::No);

		if (!Tier->RequiredTiles.Contains(TileCoordinate) || Tier->ResolvedTiles.Contains(TileCoordinate) ||
			Tier->TilesBuilding.Contains(TileCoordinate))
		{
			continue;
		}

		FCubusTerrainLodTileBuildInput Input;
		Input.DensitySettings	   = DensitySettings;
		Input.TileCoordinate	   = TileCoordinate;
		Input.CanonicalVoxelStride = Tier->CanonicalVoxelStride;
		Input.MeshingSubdivisions  = ResolveTierSubdivisions(*Tier, TileCoordinate);
		Input.TransitionFaces	   = BuildTierTransitionFaces(*Tier, TileCoordinate);
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

	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};

	bool bHasCompletedBuilds = false;
	for (const FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		bHasCompletedBuilds |= !Tier->CompletedBuilds.IsEmpty();
	}
	if (!bHasCompletedBuilds)
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

	const bool	bPreSpawnCoverageIncomplete = !IsPreSpawnVisualCoverageReady();
	const int32 UploadLimit =
		bPreSpawnCoverageIncomplete ? FMath::Max(6, FMath::Clamp(MaxLodUploadsPerTick, 1, 16)) : FMath::Clamp(MaxLodUploadsPerTick, 1, 16);

	int32  UploadedThisTick			  = 0;
	double WorkerMillisecondsThisTick = 0.0;

	const double UploadTickStart = FPlatformTime::Seconds();

	while (UploadedThisTick < UploadLimit)
	{
		bool bUploaded			 = false;
		bool bFoundCompletedTier = false;
		for (int32 Offset = 0; Offset < UE_ARRAY_COUNT(Tiers); ++Offset)
		{
			const int32 TierIndex = bPreSpawnCoverageIncomplete ? Offset : (NextLodUploadTierIndex + Offset) % UE_ARRAY_COUNT(Tiers);
			FCubusTerrainLodTierRuntime* Tier = Tiers[TierIndex];
			if (Tier->CompletedBuilds.IsEmpty())
			{
				continue;
			}
			bFoundCompletedTier = true;
			bUploaded			= UploadOneCompletedBuild(*Tier, CanonicalVoxelSize, TerrainMaterial, WorkerMillisecondsThisTick);
			if (!bPreSpawnCoverageIncomplete)
			{
				NextLodUploadTierIndex = (TierIndex + 1) % UE_ARRAY_COUNT(Tiers);
			}
			break;
		}
		if (!bFoundCompletedTier)
		{
			break;
		}

		if (bUploaded)
		{
			++UploadedThisTick;
		}
	}

	if (UploadedThisTick > 0)
	{
		const double UploadMilliseconds = (FPlatformTime::Seconds() - UploadTickStart) * 1000.0;
		int32		 LoadedCount		= 0;
		int32		 CompletedCount		= 0;
		int32		 ActiveCount		= 0;
		int32		 PendingCount		= 0;
		for (const FCubusTerrainLodTierRuntime* Tier : Tiers)
		{
			LoadedCount += Tier->TileComponents.Num();
			CompletedCount += Tier->CompletedBuilds.Num();
			ActiveCount += Tier->ActiveBuilds.Num();
			PendingCount += Tier->PendingTiles.Num();
		}

		UE_LOG(LogTemp, Display,
			   TEXT("Cubus terrain LOD upload: tiles=%d worker=%.2fms upload=%.2fms loaded=%d completed=%d building=%d pending=%d"),
			   UploadedThisTick, WorkerMillisecondsThisTick, UploadMilliseconds, LoadedCount, CompletedCount, ActiveCount, PendingCount);
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

	const int32	 ExpectedSubdivisions		 = ResolveTierSubdivisions(Tier, Result.TileCoordinate);
	const uint32 ExpectedTransitionSignature = BuildTierTransitionFaces(Tier, Result.TileCoordinate).GetSignature(ExpectedSubdivisions);

	if (Result.TransitionSignature != ExpectedTransitionSignature || Result.AppliedRefinement != ExpectedSubdivisions)
	{
		if (!Tier.TilesBuilding.Contains(Result.TileCoordinate))
		{
			Tier.PendingTiles.AddUnique(Result.TileCoordinate);
		}
		WorkerMilliseconds += Result.BuildTimeMilliseconds;
		return true;
	}

	Tier.ResolvedTiles.Add(Result.TileCoordinate);
	Tier.ResolvedTransitionSignatures.Add(Result.TileCoordinate, Result.TransitionSignature);

	FCubusMeshData* MeshData = Result.MaterialMeshes.Find(FCubusDensityMesher::UnifiedDensityMaterialKey);
	if (MeshData == nullptr || !MeshData->IsValid())
	{
		WorkerMilliseconds += Result.BuildTimeMilliseconds;
		return true;
	}

	const float TileWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize * static_cast<float>(Tier.CanonicalVoxelStride);
	UProceduralMeshComponent* Component = CreateTileComponent(Tier, Result.TileCoordinate, TileWorldSize);
	if (!IsValid(Component))
	{
		Tier.ResolvedTiles.Remove(Result.TileCoordinate);
		return false;
	}

	if (MeshData->IsValid())
	{
		Component->CreateMeshSection_LinearColor(0, MeshData->Vertices, MeshData->Triangles, MeshData->Normals, MeshData->UV0,
												 MeshData->VertexColors, MeshData->Tangents, false);

		if (IsValid(TerrainMaterial))
		{
			Component->SetMaterial(0, TerrainMaterial);
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
		Tier.ResolvedTiles.Remove(TileCoordinate);
		Tier.ResolvedTransitionSignatures.Remove(TileCoordinate);
	}

	for (auto Iterator = Tier.ResolvedTiles.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Tier.RequiredTiles.Contains(*Iterator))
		{
			Iterator.RemoveCurrent();
		}
	}

	Tier.PendingTiles.RemoveAll([&Tier](const FIntVector& TileCoordinate) { return !Tier.RequiredTiles.Contains(TileCoordinate); });

	Tier.CompletedBuilds.RemoveAll([&Tier](const FCubusTerrainLodTileBuildResult& Result)
								   { return !Tier.RequiredTiles.Contains(Result.TileCoordinate); });
}

bool ACubusTerrainLodWorldActor::IsTierWindowResident(const FCubusTerrainLodTierRuntime& Tier) const
{
	if (Tier.RequiredTiles.IsEmpty())
	{
		return false;
	}
	for (const FIntVector& Coordinate : Tier.RequiredTiles)
	{
		if (!Tier.ResolvedTiles.Contains(Coordinate))
		{
			return false;
		}
	}
	return true;
}

void ACubusTerrainLodWorldActor::RetireStableTierWindows()
{
	if (!IsValid(BlockWorld) || !BlockWorld->IsDensityStreamingCoverageReady())
	{
		return;
	}

	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};
	for (FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		/*
		 * Retire each old ring as soon as its own replacement is resident.
		 * Waiting for LOD6 previously left stale LOD1-LOD5 components visible
		 * inside newer finer windows, producing duplicate floating surfaces.
		 * Iterating fine-to-coarse guarantees the replacement beneath this ring
		 * is already resident before stale components are removed.
		 */
		if (!IsTierWindowResident(*Tier))
		{
			break;
		}
		RemoveUnneededTiles(*Tier);
	}
}

void ACubusTerrainLodWorldActor::ClearAllTiles()
{
	FCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};
	for (FCubusTerrainLodTierRuntime* Tier : Tiers)
	{
		ClearTier(*Tier);
	}

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
	Tier.ResolvedTiles.Reset();
	Tier.TilesBuilding.Reset();
	Tier.PendingTiles.Reset();
	Tier.ActiveBuilds.Reset();
	Tier.CompletedBuilds.Reset();
	Tier.ResolvedTransitionSignatures.Reset();
	Tier.InnerBounds = FCubusDensityTileBounds2D();
	Tier.OuterBounds = FCubusDensityTileBounds2D();
}

int32 ACubusTerrainLodWorldActor::ResolveTierSubdivisions(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate)
{
	(void) TileCoordinate;
	return FCubusDensityLod::NormalizeSubdivisions(Tier.MeshingSubdivisions);
}

FCubusDensityTransitionFaces ACubusTerrainLodWorldActor::BuildTierTransitionFaces(const FCubusTerrainLodTierRuntime& Tier,
																				  const FIntVector&					 TileCoordinate)
{
	FCubusDensityTransitionFaces Result;
	const int32					 SelfSubdivisions = ResolveTierSubdivisions(Tier, TileCoordinate);
	if (Tier.LodLevel == 1)
	{
		return Result;
	}

	/*
	 * LOD1 stride 2 with two subdivisions has the same effective 80 cm lattice
	 * as gameplay LOD0 stride 1 with one subdivision, so that boundary needs no
	 * transition deformation. Higher coarse tiers are true 2:1 steps; only a
	 * horizontal face bordering the exact inner clipmap rectangle owns a
	 * Transvoxel transition cell.
	 */
	const ECubusDensityFace HorizontalFaces[] = {ECubusDensityFace::NegativeX, ECubusDensityFace::PositiveX, ECubusDensityFace::NegativeY,
												 ECubusDensityFace::PositiveY};
	for (const ECubusDensityFace Face : HorizontalFaces)
	{
		const FIntVector Neighbour = TileCoordinate + FCubusDensityTransitionFaces::GetOffset(Face);
		if (Tier.InnerBounds.Contains(Neighbour.X, Neighbour.Y))
		{
			Result.Set(Face, SelfSubdivisions * 2);
		}
	}
	return Result;
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
	Component->BoundsScale		= FMath::Max(1.0f, CubusTerrainLodWorldActor::CVarTileBoundsScale.GetValueOnGameThread());
	Component->SetCullDistance(0.0f);

	const int32 SafeStride = FMath::Clamp(Tier.CanonicalVoxelStride, 2, 256);

	const float CanonicalChunkWorldSize = TileWorldSize / static_cast<float>(SafeStride);

	const FVector WorldGridOrigin		= ResolveWorldGridOrigin(CanonicalChunkWorldSize);
	const FVector WorldGridCornerOrigin = WorldGridOrigin - FVector(CanonicalChunkWorldSize * 0.5f);

	Component->SetWorldLocation(WorldGridCornerOrigin + FVector((static_cast<double>(TileCoordinate.X) + 0.5) * TileWorldSize,
																(static_cast<double>(TileCoordinate.Y) + 0.5) * TileWorldSize,
																(static_cast<double>(TileCoordinate.Z) + 0.5) * TileWorldSize));

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

	const int32						SafeStride		 = FMath::Clamp(Input.CanonicalVoxelStride, 2, 256);
	const int32						BaseSubdivisions = FCubusDensityLod::NormalizeSubdivisions(Input.MeshingSubdivisions);
	const FCubusTerrainDensityField SourceField(Input.DensitySettings);

	const FVector LogicalOrigin(static_cast<double>(Input.TileCoordinate.X * Cubus::ChunkSize),
								static_cast<double>(Input.TileCoordinate.Y * Cubus::ChunkSize),
								static_cast<double>(Input.TileCoordinate.Z * Cubus::ChunkSize));

	/*
	 * All coarse tiers now share the LOD0 chunk-corner grid. A coarse tile at
	 * stride S covers exactly S canonical chunks, so its source minimum is
	 * simply logicalOrigin*S. Parent boundaries therefore coincide with child
	 * boundaries instead of carrying the old half-tile phase offset.
	 */
	const FVector				   SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);
	const FCubusScaledDensityField ScaledField(SourceField, LogicalOrigin, SourceOrigin, static_cast<float>(SafeStride));

	const float ScaledVoxelSize = Input.CanonicalVoxelSize * static_cast<float>(SafeStride);

	FCubusDensityMesher::BuildAdaptiveChunk(ScaledField, Input.TileCoordinate, ScaledVoxelSize, BaseSubdivisions, Input.IsoLevel,
											Result.MaterialMeshes, Result.GeneratedTriangleCount, Input.TransitionFaces, &ScaledField);
	Result.AppliedRefinement   = BaseSubdivisions;
	Result.TransitionSignature = Input.TransitionFaces.GetSignature(BaseSubdivisions);

	Result.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;
	return Result;
}
