from pathlib import Path
import re

ROOT = Path('.')
H = ROOT / 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.h'
CPP = ROOT / 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp'


def read(p): return p.read_text(encoding='utf-8')
def write(p, s): p.write_text(s, encoding='utf-8')

def replace_once(p, old, new):
    s = read(p); n = s.count(old)
    if n != 1: raise RuntimeError(f'{p}: exact anchor count {n}: {old[:140]!r}')
    write(p, s.replace(old, new, 1))

def regex_once(p, pattern, repl):
    s = read(p); out, n = re.subn(pattern, repl, s, count=1, flags=re.S)
    if n != 1: raise RuntimeError(f'{p}: regex anchor count {n}: {pattern[:140]!r}')
    write(p, out)

# Runtime tier ownership is exact nested bounds, not independent radii/centres.
regex_once(H, r'''struct FCubusTerrainLodTierRuntime\n\{.*?\n\};''', '''struct FCubusTerrainLodTierRuntime
{
\tint32 LodLevel = 1;
\tint32 CanonicalVoxelStride = 2;
\tint32 MeshingSubdivisions = 2;
\tint32 VerticalRadiusTiles = 0;

\tFCubusDensityTileBounds2D InnerBounds;
\tFCubusDensityTileBounds2D OuterBounds;

\tTMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> TileComponents;
\tTSet<FIntVector> RequiredTiles;
\tTSet<FIntVector> ResolvedTiles;
\tTSet<FIntVector> TilesBuilding;
\tTArray<FIntVector> PendingTiles;
\tTArray<FCubusTerrainLodTileBuild> ActiveBuilds;
\tTArray<FCubusTerrainLodTileBuildResult> CompletedBuilds;
\tTMap<FIntVector, uint32> ResolvedTransitionSignatures;
};''')

regex_once(H, r'''\tvoid UpdateTierStreaming\(FCubusTerrainLodTierRuntime& Tier, const FVector& StreamingGridLocation,\n.*?int32 OuterRadiusTiles, int32 VerticalRadiusTiles\);''', '''\tvoid UpdateTierStreaming(
\t\tFCubusTerrainLodTierRuntime& Tier,
\t\tconst FCubusTerrainDensityField& DensityField,
\t\tfloat CanonicalChunkWorldSize,
\t\tconst FCubusDensityTileBounds2D& InnerBounds,
\t\tconst FCubusDensityTileBounds2D& OuterBounds,
\t\tint32 CanonicalVoxelStride,
\t\tint32 VerticalRadiusTiles
\t);''')

replace_once(H, '''\tvoid RemoveUnneededTiles(FCubusTerrainLodTierRuntime& Tier);\n\tvoid ClearAllTiles();''', '''\tvoid RemoveUnneededTiles(FCubusTerrainLodTierRuntime& Tier);
\tbool IsTierWindowResident(const FCubusTerrainLodTierRuntime& Tier) const;
\tvoid RetireStableTierWindows();
\tvoid ClearAllTiles();''')

replace_once(H, '''\tstatic int32\t\t\t\t\t\tResolveTierSubdivisions(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);\n\tstatic FCubusDensityTransitionFaces BuildTierTransitionFaces(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);\n\tvoid InvalidateTierTransitionDependencies(FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);\n''', '''\tstatic int32 ResolveTierSubdivisions(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);
\tstatic FCubusDensityTransitionFaces BuildTierTransitionFaces(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);
''')

# Tick: near-to-far diagnostics and one global make-before-break retirement.
replace_once(CPP, '''\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod6Runtime, &Lod5Runtime, &Lod4Runtime, &Lod3Runtime, &Lod2Runtime, &Lod1Runtime};\n''', '''\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};
''')
replace_once(CPP, '''\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tLoadedLodTileCount += Tier->TileComponents.Num();\n\t\tBuildingLodTileCount += Tier->ActiveBuilds.Num();\n\t\tPendingLodTileCount += Tier->PendingTiles.Num();\n\t}\n}\n''', '''\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)
\t{
\t\tLoadedLodTileCount += Tier->TileComponents.Num();
\t\tBuildingLodTileCount += Tier->ActiveBuilds.Num();
\t\tPendingLodTileCount += Tier->PendingTiles.Num();
\t}

\tRetireStableTierWindows();
}
''')

# Replace outer streaming with an exact clipmap derived from authoritative LOD0 bounds.
regex_once(CPP, r'''void ACubusTerrainLodWorldActor::UpdateStreaming\(\)\n\{.*?\n\}\n\nvoid ACubusTerrainLodWorldActor::UpdateTierStreaming''', '''void ACubusTerrainLodWorldActor::UpdateStreaming()
{
\tif (!bEnableTerrainLod || !IsValid(BlockWorld) || !BlockWorld->IsDensityStreamingCoverageReady())
\t{
\t\treturn;
\t}

\tACubusVoxelVolumeActor* SnapshotChunk = nullptr;
\tfor (const auto& Pair : BlockWorld->GetRegisteredChunks())
\t{
\t\tACubusVoxelVolumeActor* Candidate = Pair.Value.Get();
\t\tif (IsValid(Candidate) && Candidate->GetChunkData() != nullptr)
\t\t{
\t\t\tSnapshotChunk = Candidate;
\t\t\tbreak;
\t\t}
\t}
\tif (!IsValid(SnapshotChunk))
\t{
\t\treturn;
\t}

\tconst float CanonicalVoxelSize = FMath::Max(1.0f, SnapshotChunk->GetVoxelSize());
\tconst float CanonicalChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;
\tconst FCubusTerrainDensityField DensityField(SnapshotChunk->CaptureTerrainDensitySettings());

\tstruct FTierUpdate
\t{
\t\tFCubusTerrainLodTierRuntime* Runtime;
\t\tint32 Stride;
\t\tint32 OuterRadius;
\t\tint32 VerticalRadius;
\t};
\tFTierUpdate TierUpdates[] =
\t{
\t\t{ &Lod1Runtime, 2,  Lod1OuterRadiusTiles, Lod1VerticalRadiusTiles },
\t\t{ &Lod2Runtime, 4,  Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles },
\t\t{ &Lod3Runtime, 8,  Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles },
\t\t{ &Lod4Runtime, 16, Lod4OuterRadiusTiles, Lod4VerticalRadiusTiles },
\t\t{ &Lod5Runtime, 32, Lod5OuterRadiusTiles, Lod5VerticalRadiusTiles },
\t\t{ &Lod6Runtime, 64, Lod6OuterRadiusTiles, Lod6VerticalRadiusTiles }
\t};

\t/*
\t * Every tier is derived from the exact committed bounds of the previous
\t * finer tier. No tier independently follows the pawn. Bounds are even and
\t * power-of-two aligned, so a parent face always coincides with a complete
\t * group of child faces and moves only on the child grid cadence.
\t */
\tFCubusDensityTileBounds2D PreviousBounds = BlockWorld->GetDensityStreamingCoverageBounds();
\tfor (const FTierUpdate& Update : TierUpdates)
\t{
\t\tconst FCubusDensityTileBounds2D InnerBounds = FCubusDensityLod::ScaleDownExact(PreviousBounds, 2);
\t\tconst int32 MinimumHalfSpan = FMath::Max(
\t\t\t(InnerBounds.WidthX() + 1) / 2,
\t\t\t(InnerBounds.WidthY() + 1) / 2
\t\t);
\t\t/* +1 preserves/slightly exceeds the old radius+0.5 physical reach while keeping an even tile width. */
\t\tconst int32 OuterHalfSpan = FMath::Max(MinimumHalfSpan, Update.OuterRadius + 1);
\t\tconst FCubusDensityTileBounds2D OuterBounds = FCubusDensityLod::BuildAlignedOuterBounds(
\t\t\tInnerBounds, OuterHalfSpan, 2
\t\t);

\t\tUpdateTierStreaming(
\t\t\t*Update.Runtime,
\t\t\tDensityField,
\t\t\tCanonicalChunkWorldSize,
\t\t\tInnerBounds,
\t\t\tOuterBounds,
\t\t\tUpdate.Stride,
\t\t\tUpdate.VerticalRadius
\t\t);
\t\tPreviousBounds = OuterBounds;
\t}
}

void ACubusTerrainLodWorldActor::UpdateTierStreaming''')

# Replace tier window construction: exact Outer - Inner ring, no independent centre/radius.
regex_once(CPP, r'''void ACubusTerrainLodWorldActor::UpdateTierStreaming\(.*?\n\}\n\nvoid ACubusTerrainLodWorldActor::CollectCompletedBuilds\(\)''', '''void ACubusTerrainLodWorldActor::UpdateTierStreaming(
\tFCubusTerrainLodTierRuntime& Tier,
\tconst FCubusTerrainDensityField& DensityField,
\tconst float CanonicalChunkWorldSize,
\tconst FCubusDensityTileBounds2D& InnerBounds,
\tconst FCubusDensityTileBounds2D& OuterBounds,
\tconst int32 CanonicalVoxelStride,
\tconst int32 VerticalRadiusTiles
)
{
\tconst int32 SafeStride = FMath::Clamp(CanonicalVoxelStride, 2, 256);
\tconst int32 SafeVerticalRadius = FMath::Clamp(VerticalRadiusTiles, 0, 4);
\tconst bool bConfigurationChanged =
\t\tTier.CanonicalVoxelStride != SafeStride ||
\t\tTier.VerticalRadiusTiles != SafeVerticalRadius ||
\t\tTier.InnerBounds != InnerBounds ||
\t\tTier.OuterBounds != OuterBounds;

\tif (!bConfigurationChanged && !Tier.RequiredTiles.IsEmpty())
\t{
\t\treturn;
\t}

\tTier.CanonicalVoxelStride = SafeStride;
\tTier.VerticalRadiusTiles = SafeVerticalRadius;
\tTier.InnerBounds = InnerBounds;
\tTier.OuterBounds = OuterBounds;
\tTier.RequiredTiles.Reset();
\tTier.PendingTiles.Reset();

\tconst double TileSizeVoxels = static_cast<double>(Cubus::ChunkSize * SafeStride);
\tconstexpr int32 SurfaceSamplesPerAxis = 7;

\tfor (int32 TileY = OuterBounds.Min.Y; TileY < OuterBounds.MaxExclusive.Y; ++TileY)
\t{
\t\tfor (int32 TileX = OuterBounds.Min.X; TileX < OuterBounds.MaxExclusive.X; ++TileX)
\t\t{
\t\t\tif (InnerBounds.Contains(TileX, TileY))
\t\t\t{
\t\t\t\tcontinue;
\t\t\t}

\t\t\tconst double SourceMinimumX = static_cast<double>(TileX) * TileSizeVoxels;
\t\t\tconst double SourceMinimumY = static_cast<double>(TileY) * TileSizeVoxels;
\t\t\tint32 MinimumTileZ = MAX_int32;
\t\t\tint32 MaximumTileZ = MIN_int32;

\t\t\tfor (int32 SampleY = 0; SampleY < SurfaceSamplesPerAxis; ++SampleY)
\t\t\t{
\t\t\t\tfor (int32 SampleX = 0; SampleX < SurfaceSamplesPerAxis; ++SampleX)
\t\t\t\t{
\t\t\t\t\tconst double AlphaX = static_cast<double>(SampleX) / static_cast<double>(SurfaceSamplesPerAxis - 1);
\t\t\t\t\tconst double AlphaY = static_cast<double>(SampleY) / static_cast<double>(SurfaceSamplesPerAxis - 1);
\t\t\t\t\tconst float Height = DensityField.SampleSurfaceVoxelHeight(
\t\t\t\t\t\tstatic_cast<float>(SourceMinimumX + AlphaX * TileSizeVoxels),
\t\t\t\t\t\tstatic_cast<float>(SourceMinimumY + AlphaY * TileSizeVoxels)
\t\t\t\t\t);
\t\t\t\t\tconst int32 SurfaceTileZ = FMath::FloorToInt(static_cast<double>(Height) / TileSizeVoxels);
\t\t\t\t\tMinimumTileZ = FMath::Min(MinimumTileZ, SurfaceTileZ);
\t\t\t\t\tMaximumTileZ = FMath::Max(MaximumTileZ, SurfaceTileZ);
\t\t\t\t}
\t\t\t}

\t\t\tfor (int32 TileZ = MinimumTileZ - SafeVerticalRadius; TileZ <= MaximumTileZ + SafeVerticalRadius; ++TileZ)
\t\t\t{
\t\t\t\tTier.RequiredTiles.Add(FIntVector(TileX, TileY, TileZ));
\t\t\t}
\t\t}
\t}

\t/*
\t * A coordinate can remain in the ring while its inner-boundary face changes.
\t * Rebuild only those retained tiles whose transition signature changed.
\t * The old component stays visible until the replacement uploads.
\t */
\tfor (const FIntVector& TileCoordinate : Tier.RequiredTiles)
\t{
\t\tconst uint32 ExpectedSignature = BuildTierTransitionFaces(Tier, TileCoordinate)
\t\t\t.GetSignature(Tier.MeshingSubdivisions);
\t\tif (Tier.ResolvedTiles.Contains(TileCoordinate))
\t\t{
\t\t\tconst uint32* ExistingSignature = Tier.ResolvedTransitionSignatures.Find(TileCoordinate);
\t\t\tif (ExistingSignature == nullptr || *ExistingSignature != ExpectedSignature)
\t\t\t{
\t\t\t\tTier.ResolvedTiles.Remove(TileCoordinate);
\t\t\t}
\t\t}

\t\tconst bool bCompletedPendingUpload = Tier.CompletedBuilds.ContainsByPredicate(
\t\t\t[&TileCoordinate](const FCubusTerrainLodTileBuildResult& Result)
\t\t\t{
\t\t\t\treturn Result.TileCoordinate == TileCoordinate;
\t\t\t}
\t\t);
\t\tif (!Tier.ResolvedTiles.Contains(TileCoordinate) &&
\t\t\t!Tier.TilesBuilding.Contains(TileCoordinate) &&
\t\t\t!bCompletedPendingUpload)
\t\t{
\t\t\tTier.PendingTiles.Add(TileCoordinate);
\t\t}
\t}

\tauto DistanceFromInner = [&InnerBounds](const FIntVector& Coordinate)
\t{
\t\tconst int32 DeltaX = Coordinate.X < InnerBounds.Min.X
\t\t\t? InnerBounds.Min.X - Coordinate.X
\t\t\t: (Coordinate.X >= InnerBounds.MaxExclusive.X ? Coordinate.X - InnerBounds.MaxExclusive.X + 1 : 0);
\t\tconst int32 DeltaY = Coordinate.Y < InnerBounds.Min.Y
\t\t\t? InnerBounds.Min.Y - Coordinate.Y
\t\t\t: (Coordinate.Y >= InnerBounds.MaxExclusive.Y ? Coordinate.Y - InnerBounds.MaxExclusive.Y + 1 : 0);
\t\treturn FMath::Max(DeltaX, DeltaY);
\t};
\tTier.PendingTiles.Sort(
\t\t[&DistanceFromInner](const FIntVector& A, const FIntVector& B)
\t\t{
\t\t\tconst int32 DistanceA = DistanceFromInner(A);
\t\t\tconst int32 DistanceB = DistanceFromInner(B);
\t\t\tif (DistanceA != DistanceB)
\t\t\t{
\t\t\t\treturn DistanceA > DistanceB; // Pop() => seam first.
\t\t\t}
\t\t\treturn FMath::Abs(A.Z) > FMath::Abs(B.Z);
\t\t}
\t);

\tUE_LOG(LogTemp, Display,
\t\tTEXT("Cubus terrain LOD%d clipmap: stride=%d inner=[%d,%d)-[%d,%d) outer=[%d,%d)-[%d,%d) required=%d loaded=%d pending=%d building=%d tile=%.0fm"),
\t\tTier.LodLevel, SafeStride,
\t\tInnerBounds.Min.X, InnerBounds.Min.Y, InnerBounds.MaxExclusive.X, InnerBounds.MaxExclusive.Y,
\t\tOuterBounds.Min.X, OuterBounds.Min.Y, OuterBounds.MaxExclusive.X, OuterBounds.MaxExclusive.Y,
\t\tTier.RequiredTiles.Num(), Tier.TileComponents.Num(), Tier.PendingTiles.Num(), Tier.ActiveBuilds.Num(),
\t\tCanonicalChunkWorldSize * static_cast<float>(SafeStride) / 100.0f);
}

void ACubusTerrainLodWorldActor::CollectCompletedBuilds()''')

# All build/upload priority arrays near-to-far.
CPP_TEXT = read(CPP)
CPP_TEXT = CPP_TEXT.replace('{&Lod6Runtime, &Lod5Runtime, &Lod4Runtime, &Lod3Runtime, &Lod2Runtime, &Lod1Runtime}',
                            '{&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime}')
write(CPP, CPP_TEXT)

# Upload: fixed tier subdivisions, no individual promotion/churn.
regex_once(CPP, r'''\tconst int32 CurrentSubdivisions = ResolveTierSubdivisions\(Tier, Result.TileCoordinate\);.*?\n\tTier.ResolvedTiles.Add\(Result.TileCoordinate\);\n\n\tif \(Result.AppliedRefinement > Tier.MeshingSubdivisions\).*?\n\t\}\n''', '''\tconst int32 ExpectedSubdivisions = ResolveTierSubdivisions(Tier, Result.TileCoordinate);
\tconst uint32 ExpectedTransitionSignature = BuildTierTransitionFaces(Tier, Result.TileCoordinate)
\t\t.GetSignature(ExpectedSubdivisions);

\tif (Result.TransitionSignature != ExpectedTransitionSignature ||
\t\tResult.AppliedRefinement != ExpectedSubdivisions)
\t{
\t\tif (!Tier.TilesBuilding.Contains(Result.TileCoordinate))
\t\t{
\t\t\tTier.PendingTiles.AddUnique(Result.TileCoordinate);
\t\t}
\t\tWorkerMilliseconds += Result.BuildTimeMilliseconds;
\t\treturn true;
\t}

\tTier.ResolvedTiles.Add(Result.TileCoordinate);
\tTier.ResolvedTransitionSignatures.Add(Result.TileCoordinate, Result.TransitionSignature);
''')

# Remove immediate per-tier retirement from UploadCompletedBuilds; global retirement happens only when all tiers resident.
regex_once(CPP, r'''\tconst auto RetireStaleTilesIfResident = \[this\].*?\n\tfor \(FCubusTerrainLodTierRuntime\* Tier : Tiers\)\n\t\{\n\t\tRetireStaleTilesIfResident\(\*Tier\);\n\t\}\n''', '')

# Remove signature state with stale components.
replace_once(CPP, '''\t\tTier.TileComponents.Remove(TileCoordinate);\n\t\tTier.ResolvedTiles.Remove(TileCoordinate);\n''', '''\t\tTier.TileComponents.Remove(TileCoordinate);
\t\tTier.ResolvedTiles.Remove(TileCoordinate);
\t\tTier.ResolvedTransitionSignatures.Remove(TileCoordinate);
''')

# Add global atomic-ish make-before-break retirement contract before ClearAllTiles.
replace_once(CPP, '''void ACubusTerrainLodWorldActor::ClearAllTiles()\n''', '''bool ACubusTerrainLodWorldActor::IsTierWindowResident(const FCubusTerrainLodTierRuntime& Tier) const
{
\tif (Tier.RequiredTiles.IsEmpty())
\t{
\t\treturn false;
\t}
\tfor (const FIntVector& Coordinate : Tier.RequiredTiles)
\t{
\t\tif (!Tier.ResolvedTiles.Contains(Coordinate))
\t\t{
\t\t\treturn false;
\t\t}
\t}
\treturn true;
}

void ACubusTerrainLodWorldActor::RetireStableTierWindows()
{
\tif (!IsValid(BlockWorld) || !BlockWorld->IsDensityStreamingCoverageReady())
\t{
\t\treturn;
\t}

\tFCubusTerrainLodTierRuntime* Tiers[] =
\t{
\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,
\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime
\t};
\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)
\t{
\t\tif (!IsTierWindowResident(*Tier))
\t\t{
\t\t\treturn;
\t\t}
\t}

\t/*
\t * Retire the previous hierarchy only after the complete replacement
\t * hierarchy is resident. This trades a short-lived overlap for zero holes
\t * and prevents one tier from exposing a gap while its neighbour catches up.
\t */
\tfor (FCubusTerrainLodTierRuntime* Tier : Tiers)
\t{
\t\tRemoveUnneededTiles(*Tier);
\t}
}

void ACubusTerrainLodWorldActor::ClearAllTiles()
''')

replace_once(CPP, '''\tTier.CompletedBuilds.Reset();\n\tTier.RefinementOverrides.Reset();\n\tTier.LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n''', '''\tTier.CompletedBuilds.Reset();
\tTier.ResolvedTransitionSignatures.Reset();
\tTier.InnerBounds = FCubusDensityTileBounds2D();
\tTier.OuterBounds = FCubusDensityTileBounds2D();
''')

# Fixed subdivision and exact inner-boundary transition ownership.
regex_once(CPP, r'''int32 ACubusTerrainLodWorldActor::ResolveTierSubdivisions\(.*?\n\}\n\nFCubusDensityTransitionFaces ACubusTerrainLodWorldActor::BuildTierTransitionFaces\(.*?\n\}\n\nvoid ACubusTerrainLodWorldActor::InvalidateTierTransitionDependencies\(.*?\n\}\n\nUProceduralMeshComponent\* ACubusTerrainLodWorldActor::CreateTileComponent''', '''int32 ACubusTerrainLodWorldActor::ResolveTierSubdivisions(
\tconst FCubusTerrainLodTierRuntime& Tier,
\tconst FIntVector& TileCoordinate
)
{
\t(void)TileCoordinate;
\treturn FCubusDensityLod::NormalizeSubdivisions(Tier.MeshingSubdivisions);
}

FCubusDensityTransitionFaces ACubusTerrainLodWorldActor::BuildTierTransitionFaces(
\tconst FCubusTerrainLodTierRuntime& Tier,
\tconst FIntVector& TileCoordinate
)
{
\tFCubusDensityTransitionFaces Result;
\tconst int32 SelfSubdivisions = ResolveTierSubdivisions(Tier, TileCoordinate);

\t/*
\t * Same-tier neighbours are identical resolution. Only a horizontal face
\t * bordering the exact inner clipmap rectangle meets the previous finer tier.
\t * The coarser tile owns that Transvoxel transition cell.
\t */
\tconst ECubusDensityFace HorizontalFaces[] =
\t{
\t\tECubusDensityFace::NegativeX, ECubusDensityFace::PositiveX,
\t\tECubusDensityFace::NegativeY, ECubusDensityFace::PositiveY
\t};
\tfor (const ECubusDensityFace Face : HorizontalFaces)
\t{
\t\tconst FIntVector Neighbour = TileCoordinate + FCubusDensityTransitionFaces::GetOffset(Face);
\t\tif (Tier.InnerBounds.Contains(Neighbour.X, Neighbour.Y))
\t\t{
\t\t\tResult.Set(Face, SelfSubdivisions * 2);
\t\t}
\t}
\treturn Result;
}

UProceduralMeshComponent* ACubusTerrainLodWorldActor::CreateTileComponent''')

# BuildTile fixed resolution: remove empty-tile per-coordinate 4x promotion.
regex_once(CPP, r'''\tFCubusDensityMesher::BuildAdaptiveChunk\(ScaledField, Input.TileCoordinate, ScaledVoxelSize, BaseSubdivisions, Input.IsoLevel,\n\t\t\t\t\t\t\t\t\t\t\tResult.MaterialMeshes, Result.GeneratedTriangleCount, Input.TransitionFaces, &ScaledField\);\n\tResult.AppliedRefinement   = BaseSubdivisions;\n\tResult.TransitionSignature = Input.TransitionFaces.GetSignature\(BaseSubdivisions\);\n\n\tif \(Result.MaterialMeshes.IsEmpty\(\) && BaseSubdivisions < 4\)\n\t\{.*?\n\t\}\n''', '''\tFCubusDensityMesher::BuildAdaptiveChunk(
\t\tScaledField, Input.TileCoordinate, ScaledVoxelSize, BaseSubdivisions, Input.IsoLevel,
\t\tResult.MaterialMeshes, Result.GeneratedTriangleCount, Input.TransitionFaces, &ScaledField
\t);
\tResult.AppliedRefinement = BaseSubdivisions;
\tResult.TransitionSignature = Input.TransitionFaces.GetSignature(BaseSubdivisions);
''')

# Guards.
checks = {
    H: ['FCubusDensityTileBounds2D InnerBounds;', 'ResolvedTransitionSignatures', 'RetireStableTierWindows'],
    CPP: ['ScaleDownExact(PreviousBounds, 2)', 'BuildAlignedOuterBounds(', 'InnerBounds.Contains(TileX, TileY)',
          'ResolvedTransitionSignatures.Add', 'Retire the previous hierarchy only after the complete replacement',
          'Tier.InnerBounds.Contains(Neighbour.X, Neighbour.Y)']
}
for p, toks in checks.items():
    s = read(p)
    for t in toks:
        if t not in s: raise RuntimeError(f'{p}: missing guard {t!r}')

for forbidden in ['RefinementOverrides', 'LastCentreTile', 'InvalidateTierTransitionDependencies']:
    if forbidden in read(H) or forbidden in read(CPP):
        raise RuntimeError(f'legacy churn mechanism remains: {forbidden}')
