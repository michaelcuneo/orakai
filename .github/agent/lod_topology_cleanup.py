from pathlib import Path
import re

ROOT = Path('.')


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding='utf-8')


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one anchor, found {count}: {old[:140]!r}')
    write(path, text.replace(old, new, 1))


def regex_once(path: str, pattern: str, replacement: str, flags=re.S) -> None:
    text = read(path)
    new_text, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f'{path}: regex expected one match, found {count}: {pattern[:140]!r}')
    write(path, new_text)


world_cpp = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
lod_h = 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.h'
lod_cpp = 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp'

# ---------------------------------------------------------------------------
# Gameplay streaming: keep startup support-first and make async topology
# validation mode-aware. Block meshes have no density transition signature.
# Hybrid meshes do, and should use the same async streaming path as Density.
# ---------------------------------------------------------------------------
replace_once(
    world_cpp,
    '''\t\t\tif (bEnableRuntimeStreaming && Chunk->GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)\n\t\t\t{\n\t\t\t\tStreamingChunksReady.Remove(Coordinate);\n\t\t\t}\n\t\t\telse\n''',
    '''\t\t\tconst ECubusVoxelRenderMode ChunkRenderMode = Chunk->GetEffectiveRenderMode();\n\t\t\tif (bEnableRuntimeStreaming &&\n\t\t\t\t(ChunkRenderMode == ECubusVoxelRenderMode::Density || ChunkRenderMode == ECubusVoxelRenderMode::Hybrid))\n\t\t\t{\n\t\t\t\tStreamingChunksReady.Remove(Coordinate);\n\t\t\t}\n\t\t\telse\n'''
)

regex_once(
    world_cpp,
    r'''void ACubusBlockWorldActor::ProcessInitialStreaming\(\)\n\{.*?\n\}\n\nvoid ACubusBlockWorldActor::ProcessCompletedStreamingChunkBuilds\(\)''',
    '''void ACubusBlockWorldActor::ProcessInitialStreaming()\n{\n\tif (bInitialSpawnAreaReady)\n\t{\n\t\treturn;\n\t}\n\n\tconst bool bHasSupportCoordinate =\n\t\tLastTrackedChunk.X != MAX_int32 &&\n\t\tLastTrackedChunk.Y != MAX_int32 &&\n\t\tLastTrackedChunk.Z != MAX_int32;\n\n\tif (!bHasSupportCoordinate || !IsInitialChunkReady(LastTrackedChunk))\n\t{\n\t\treturn;\n\t}\n\n\t/*\n\t * The enlarged initial area is important for immediate walking quality,\n\t * but it is not a loading-screen dependency. Release the pawn as soon as\n\t * the support chunk has committed collision; the rest of the LOD0 ring\n\t * continues through the normal support-first streaming queue.\n\t */\n\tbInitialSpawnAreaReady = true;\n\n\tUE_LOG(LogTemp, Display,\n\t\tTEXT("Cubus spawn support ready at (%d, %d, %d); %d initial chunks continue streaming"),\n\t\tLastTrackedChunk.X, LastTrackedChunk.Y, LastTrackedChunk.Z, InitialRequiredCoordinates.Num());\n\n\tUpdateRuntimeStreaming(true);\n}\n\nvoid ACubusBlockWorldActor::ProcessCompletedStreamingChunkBuilds()'''
)

regex_once(
    world_cpp,
    r'''\t\tconst bool bResolutionStillCurrent =\n\t\t\tBuild\.bIsBlockBuild \|\| \(IsValid\(Chunk\) && Chunk->GetDensitySubdivisionsPerVoxel\(\) == Build\.SubdivisionsPerVoxel\);\n\t\tconst FCubusDensityTransitionFaces CurrentTransitionFaces =\n\t\t\tbResolutionStillCurrent \? BuildDensityTransitionFaces\(Build\.Coordinate, Build\.SubdivisionsPerVoxel\)\n\t\t\t\t\t\t\t\t\t\t: FCubusDensityTransitionFaces\(\);\n\t\tconst bool bTopologyStillCurrent =\n\t\t\tbResolutionStillCurrent && CurrentTransitionFaces\.GetSignature\(Build\.SubdivisionsPerVoxel\) == Build\.TransitionSignature;''',
    '''\t\tconst bool bResolutionStillCurrent =\n\t\t\tBuild.bIsBlockBuild ||\n\t\t\t(IsValid(Chunk) && Chunk->GetDensitySubdivisionsPerVoxel() == Build.SubdivisionsPerVoxel);\n\n\t\t/*\n\t\t * Pure block jobs do not participate in density LOD topology. Their\n\t\t * TransitionSignature is intentionally zero, so never compare it to a\n\t\t * synthetic density-neighbour signature. Density and Hybrid jobs retain\n\t\t * the full stale-topology rejection.\n\t\t */\n\t\tconst bool bTopologyStillCurrent = Build.bIsBlockBuild ||\n\t\t\t(bResolutionStillCurrent &&\n\t\t\t BuildDensityTransitionFaces(Build.Coordinate, Build.SubdivisionsPerVoxel)\n\t\t\t\t .GetSignature(Build.SubdivisionsPerVoxel) == Build.TransitionSignature);'''
)

replace_once(
    world_cpp,
    '''\t\tif (IsValid(ChunkActor))\n\t\t{\n\t\t\tStreamingChunksReady.Remove(Coordinate);\n\n\t\t\tUnregisterChunk(ChunkActor);\n''',
    '''\t\tif (IsValid(ChunkActor))\n\t\t{\n\t\t\tStreamingChunksReady.Remove(Coordinate);\n\t\t\tDensitySurfaceRetryCoordinates.Remove(Coordinate);\n\n\t\t\tUnregisterChunk(ChunkActor);\n'''
)

# ---------------------------------------------------------------------------
# Coarse terrain LOD data model: one fixed 2x adaptive mesh per visual tier,
# with optional local 4x promotion and explicit transition signatures.
# ---------------------------------------------------------------------------
replace_once(
    lod_h,
    '#include "CubusCore/Meshing/CubusMeshData.h"\n',
    '#include "CubusCore/Meshing/CubusMeshData.h"\n#include "CubusCore/Meshing/CubusDensityLod.h"\n'
)

regex_once(
    lod_h,
    r'''struct FCubusTerrainLodTileBuildInput\n\{.*?\n\};''',
    '''struct FCubusTerrainLodTileBuildInput\n{\n\tFCubusTerrainDensitySettings DensitySettings;\n\tFIntVector TileCoordinate = FIntVector::ZeroValue;\n\tint32 CanonicalVoxelStride = 2;\n\tint32 MeshingSubdivisions = 2;\n\tFCubusDensityTransitionFaces TransitionFaces;\n\tfloat CanonicalVoxelSize = 100.0f;\n\tfloat IsoLevel = 0.0f;\n};'''
)

regex_once(
    lod_h,
    r'''struct FCubusTerrainLodTileBuildResult\n\{.*?\n\};''',
    '''struct FCubusTerrainLodTileBuildResult\n{\n\tFIntVector TileCoordinate = FIntVector::ZeroValue;\n\tTMap<int32, FCubusMeshData> MaterialMeshes;\n\tint32 GeneratedTriangleCount = 0;\n\tdouble BuildTimeMilliseconds = 0.0;\n\tint32 AppliedRefinement = 2;\n\tuint32 TransitionSignature = 0;\n};'''
)

regex_once(
    lod_h,
    r'''struct FCubusTerrainLodTierRuntime\n\{.*?\n\};''',
    '''struct FCubusTerrainLodTierRuntime\n{\n\tint32 LodLevel = 1;\n\tint32 CanonicalVoxelStride = 2;\n\tint32 MeshingSubdivisions = 2;\n\tint32 InnerRadiusTiles = 0;\n\tint32 OuterRadiusTiles = 4;\n\tint32 VerticalRadiusTiles = 0;\n\tint32 OverlapTiles = 0;\n\n\tTMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> TileComponents;\n\tTSet<FIntVector> RequiredTiles;\n\tTSet<FIntVector> ResolvedTiles;\n\tTSet<FIntVector> TilesBuilding;\n\tTArray<FIntVector> PendingTiles;\n\tTArray<FCubusTerrainLodTileBuild> ActiveBuilds;\n\tTArray<FCubusTerrainLodTileBuildResult> CompletedBuilds;\n\tTMap<FIntVector, int32> RefinementOverrides;\n\n\tFIntVector LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n};'''
)

replace_once(
    lod_h,
    '''\t/** LOD1 samples one point every four canonical voxels. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "2", ClampMax = "64"))\n\tint32 Lod1CanonicalVoxelStride = 4;\n''',
    '''\t/** LOD1 parent scale. Runtime enforces the watertight 2 -> 4 -> 8 stride chain. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "2", ClampMax = "2"))\n\tint32 Lod1CanonicalVoxelStride = 2;\n'''
)
replace_once(
    lod_h,
    '''\tint32 Lod1OverlapTiles = 1;\n''',
    '''\tint32 Lod1OverlapTiles = 0;\n'''
)
replace_once(
    lod_h,
    '''\t/** LOD2 samples one point every sixteen canonical voxels. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "4", ClampMax = "256"))\n\tint32 Lod2CanonicalVoxelStride = 16;\n''',
    '''\t/** LOD2 is exactly twice the LOD1 parent scale. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "4", ClampMax = "4"))\n\tint32 Lod2CanonicalVoxelStride = 4;\n'''
)
replace_once(lod_h, '\tint32 Lod2OverlapTiles = 1;\n', '\tint32 Lod2OverlapTiles = 0;\n')
replace_once(
    lod_h,
    '''\t/** LOD3 samples one point every sixty-four canonical voxels. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "16", ClampMax = "256"))\n\tint32 Lod3CanonicalVoxelStride = 64;\n''',
    '''\t/** LOD3 is exactly twice the LOD2 parent scale. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "8", ClampMax = "8"))\n\tint32 Lod3CanonicalVoxelStride = 8;\n'''
)
replace_once(lod_h, '\tint32 Lod3OverlapTiles = 1;\n', '\tint32 Lod3OverlapTiles = 0;\n')

replace_once(
    lod_h,
    '''\tvoid ClearTier(FCubusTerrainLodTierRuntime& Tier);\n\n\tUProceduralMeshComponent* CreateTileComponent''',
    '''\tvoid ClearTier(FCubusTerrainLodTierRuntime& Tier);\n\n\tstatic int32 ResolveTierSubdivisions(const FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);\n\tstatic FCubusDensityTransitionFaces BuildTierTransitionFaces(\n\t\tconst FCubusTerrainLodTierRuntime& Tier,\n\t\tconst FIntVector& TileCoordinate\n\t);\n\tvoid InvalidateTierTransitionDependencies(FCubusTerrainLodTierRuntime& Tier, const FIntVector& TileCoordinate);\n\n\tUProceduralMeshComponent* CreateTileComponent'''
)

# ---------------------------------------------------------------------------
# Coarse terrain LOD implementation.
# ---------------------------------------------------------------------------
# Shared corner origin: all power-of-two tile boundaries become nested with the
# LOD0 chunk boundaries. Force the 2/4/8 chain even for old serialized BP data.
replace_once(
    lod_cpp,
    '''\tconst FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);\n\n\t/*\n\t * World ownership follows the controlled pawn, not the camera.\n''',
    '''\tconst FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);\n\tconst FVector WorldGridCornerOrigin =\n\t\tWorldGridOrigin - FVector(CanonicalChunkWorldSize * 0.5f);\n\n\t/*\n\t * World ownership follows the controlled pawn, not the camera.\n'''
)
replace_once(
    lod_cpp,
    '''\tconst FVector StreamingGridLocation = PlayerPawn->GetActorLocation() - WorldGridOrigin;\n''',
    '''\tconst FVector StreamingGridLocation = PlayerPawn->GetActorLocation() - WorldGridCornerOrigin;\n'''
)
regex_once(
    lod_cpp,
    r'''\tconst int32 SafeLod1Stride = FMath::Clamp\(Lod1CanonicalVoxelStride, 2, 64\);.*?\tUpdateTierStreaming\(Lod3Runtime, StreamingGridLocation, DensityField, CanonicalChunkWorldSize, Lod2HalfExtentCanonicalChunks,\n\t\t\t\t\t\tSafeLod3Stride, Lod3OverlapTiles, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles\);''',
    '''\t/*\n\t * Keep every visual tier exactly one 2:1 sampling step from the previous\n\t * tier. Each tile is meshed at 2x internally, so with an 80 cm canonical\n\t * voxel the effective sequence is 80 cm -> 160 cm -> 320 cm outside the\n\t * 40 cm gameplay ring. Old Blueprint stride values are intentionally not\n\t * allowed to recreate the former 4:1 jumps.\n\t */\n\tconstexpr int32 SafeLod1Stride = 2;\n\tconstexpr int32 SafeLod2Stride = 4;\n\tconstexpr int32 SafeLod3Stride = 8;\n\n\tUpdateTierStreaming(Lod1Runtime, StreamingGridLocation, DensityField, CanonicalChunkWorldSize, Lod0HalfExtentCanonicalChunks,\n\t\tSafeLod1Stride, 0, Lod1OuterRadiusTiles, Lod1VerticalRadiusTiles);\n\n\tconst int32 SafeLod1OuterRadius = FMath::Max(Lod1Runtime.InnerRadiusTiles + 1, Lod1Runtime.OuterRadiusTiles);\n\tconst double Lod1HalfExtentCanonicalChunks =\n\t\t(static_cast<double>(SafeLod1OuterRadius) + 0.5) * static_cast<double>(SafeLod1Stride);\n\n\tUpdateTierStreaming(Lod2Runtime, StreamingGridLocation, DensityField, CanonicalChunkWorldSize, Lod1HalfExtentCanonicalChunks,\n\t\tSafeLod2Stride, 0, Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles);\n\n\tconst int32 SafeLod2OuterRadius = FMath::Max(Lod2Runtime.InnerRadiusTiles + 1, Lod2Runtime.OuterRadiusTiles);\n\tconst double Lod2HalfExtentCanonicalChunks =\n\t\t(static_cast<double>(SafeLod2OuterRadius) + 0.5) * static_cast<double>(SafeLod2Stride);\n\n\tUpdateTierStreaming(Lod3Runtime, StreamingGridLocation, DensityField, CanonicalChunkWorldSize, Lod2HalfExtentCanonicalChunks,\n\t\tSafeLod3Stride, 0, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles);'''
)

replace_once(
    lod_cpp,
    '''\tconst FIntVector CentreTile(FMath::FloorToInt((StreamingGridLocation.X + TileWorldSize * 0.5f) / TileWorldSize),\n\t\t\t\t\t\t\t\tFMath::FloorToInt((StreamingGridLocation.Y + TileWorldSize * 0.5f) / TileWorldSize), 0);\n''',
    '''\tconst FIntVector CentreTile(\n\t\tFMath::FloorToInt(StreamingGridLocation.X / TileWorldSize),\n\t\tFMath::FloorToInt(StreamingGridLocation.Y / TileWorldSize),\n\t\t0\n\t);\n'''
)

replace_once(
    lod_cpp,
    '''\tconst double\tTileSizeVoxels\t\t  = static_cast<double>(Cubus::ChunkSize * SafeStride);\n\tconst double\tAlignmentOffset\t\t  = static_cast<double>(Cubus::ChunkSize) * 0.5 * static_cast<double>(SafeStride - 1);\n\tconstexpr int32 SurfaceSamplesPerAxis = 5;\n''',
    '''\tconst double TileSizeVoxels = static_cast<double>(Cubus::ChunkSize * SafeStride);\n\tconstexpr int32 SurfaceSamplesPerAxis = 7;\n'''
)
replace_once(
    lod_cpp,
    '''\t\t\tconst double SourceMinimumX = static_cast<double>(TileX) * TileSizeVoxels - AlignmentOffset;\n\t\t\tconst double SourceMinimumY = static_cast<double>(TileY) * TileSizeVoxels - AlignmentOffset;\n''',
    '''\t\t\tconst double SourceMinimumX = static_cast<double>(TileX) * TileSizeVoxels;\n\t\t\tconst double SourceMinimumY = static_cast<double>(TileY) * TileSizeVoxels;\n'''
)
replace_once(
    lod_cpp,
    '''\t\t\t\t\tconst int32 SurfaceTileZ = FMath::FloorToInt((static_cast<double>(Height) + AlignmentOffset) / TileSizeVoxels);\n''',
    '''\t\t\t\t\tconst int32 SurfaceTileZ = FMath::FloorToInt(static_cast<double>(Height) / TileSizeVoxels);\n'''
)

# Drop refinement overrides that no longer belong to the streaming window.
replace_once(
    lod_cpp,
    '''\tTier.RequiredTiles.Reset();\n\tTier.PendingTiles.Reset();\n''',
    '''\tTier.RequiredTiles.Reset();\n\tTier.PendingTiles.Reset();\n'''
)
replace_once(
    lod_cpp,
    '''\t/*\n\t * Make-before-break streaming.\n''',
    '''\tfor (auto Iterator = Tier.RefinementOverrides.CreateIterator(); Iterator; ++Iterator)\n\t{\n\t\tif (!Tier.RequiredTiles.Contains(Iterator.Key()))\n\t\t{\n\t\t\tIterator.RemoveCurrent();\n\t\t}\n\t}\n\n\t/*\n\t * Make-before-break streaming.\n'''
)

# Insert transition/refinement helpers before CreateTileComponent.
replace_once(
    lod_cpp,
    '''UProceduralMeshComponent* ACubusTerrainLodWorldActor::CreateTileComponent''',
    '''int32 ACubusTerrainLodWorldActor::ResolveTierSubdivisions(\n\tconst FCubusTerrainLodTierRuntime& Tier,\n\tconst FIntVector& TileCoordinate\n)\n{\n\tif (const int32* Override = Tier.RefinementOverrides.Find(TileCoordinate))\n\t{\n\t\treturn FCubusDensityLod::NormalizeSubdivisions(*Override);\n\t}\n\n\treturn FCubusDensityLod::NormalizeSubdivisions(Tier.MeshingSubdivisions);\n}\n\nFCubusDensityTransitionFaces ACubusTerrainLodWorldActor::BuildTierTransitionFaces(\n\tconst FCubusTerrainLodTierRuntime& Tier,\n\tconst FIntVector& TileCoordinate\n)\n{\n\tFCubusDensityTransitionFaces Result;\n\tconst int32 SelfSubdivisions = ResolveTierSubdivisions(Tier, TileCoordinate);\n\n\tfor (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)\n\t{\n\t\tconst ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);\n\t\tconst FIntVector NeighbourCoordinate =\n\t\t\tTileCoordinate + FCubusDensityTransitionFaces::GetOffset(Face);\n\n\t\tif (Tier.RequiredTiles.Contains(NeighbourCoordinate))\n\t\t{\n\t\t\tconst int32 NeighbourSubdivisions = ResolveTierSubdivisions(Tier, NeighbourCoordinate);\n\t\t\tif (NeighbourSubdivisions == SelfSubdivisions * 2)\n\t\t\t{\n\t\t\t\tResult.Set(Face, NeighbourSubdivisions);\n\t\t\t}\n\t\t\tcontinue;\n\t\t}\n\n\t\t/*\n\t\t * The inner X/Y boundary is owned by this coarser tier. A base 2x tile\n\t\t * sees the preceding tier at an equivalent 4x resolution in this\n\t\t * scaled coordinate space. A locally promoted 4x tile already matches\n\t\t * that finer tier and therefore needs no transition cell.\n\t\t */\n\t\tif (Face == ECubusDensityFace::NegativeX || Face == ECubusDensityFace::PositiveX ||\n\t\t\tFace == ECubusDensityFace::NegativeY || Face == ECubusDensityFace::PositiveY)\n\t\t{\n\t\t\tconst FIntVector Offset = NeighbourCoordinate - Tier.LastCentreTile;\n\t\t\tconst int32 HorizontalDistance = FMath::Max(FMath::Abs(Offset.X), FMath::Abs(Offset.Y));\n\t\t\tif (HorizontalDistance <= Tier.InnerRadiusTiles && SelfSubdivisions == Tier.MeshingSubdivisions)\n\t\t\t{\n\t\t\t\tResult.Set(Face, SelfSubdivisions * 2);\n\t\t\t}\n\t\t}\n\t}\n\n\treturn Result;\n}\n\nvoid ACubusTerrainLodWorldActor::InvalidateTierTransitionDependencies(\n\tFCubusTerrainLodTierRuntime& Tier,\n\tconst FIntVector& TileCoordinate\n)\n{\n\tauto Invalidate = [&Tier](const FIntVector& Coordinate)\n\t{\n\t\tif (!Tier.RequiredTiles.Contains(Coordinate))\n\t\t{\n\t\t\treturn;\n\t\t}\n\n\t\tTier.ResolvedTiles.Remove(Coordinate);\n\t\tif (!Tier.TilesBuilding.Contains(Coordinate))\n\t\t{\n\t\t\tTier.PendingTiles.AddUnique(Coordinate);\n\t\t}\n\t};\n\n\tInvalidate(TileCoordinate);\n\tfor (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)\n\t{\n\t\tInvalidate(\n\t\t\tTileCoordinate + FCubusDensityTransitionFaces::GetOffset(static_cast<ECubusDensityFace>(FaceIndex))\n\t\t);\n\t}\n}\n\nUProceduralMeshComponent* ACubusTerrainLodWorldActor::CreateTileComponent'''
)

# StartPendingBuilds captures subdivision and transition topology.
replace_once(
    lod_cpp,
    '''\t\tInput.DensitySettings\t   = DensitySettings;\n\t\tInput.TileCoordinate\t   = TileCoordinate;\n\t\tInput.CanonicalVoxelStride = Tier->CanonicalVoxelStride;\n\t\tInput.CanonicalVoxelSize   = CanonicalVoxelSize;\n\t\tInput.IsoLevel\t\t\t   = 0.0f;\n''',
    '''\t\tInput.DensitySettings = DensitySettings;\n\t\tInput.TileCoordinate = TileCoordinate;\n\t\tInput.CanonicalVoxelStride = Tier->CanonicalVoxelStride;\n\t\tInput.MeshingSubdivisions = ResolveTierSubdivisions(*Tier, TileCoordinate);\n\t\tInput.TransitionFaces = BuildTierTransitionFaces(*Tier, TileCoordinate);\n\t\tInput.CanonicalVoxelSize = CanonicalVoxelSize;\n\t\tInput.IsoLevel = 0.0f;\n'''
)

# Validate topology on upload, and adopt 4x worker promotion atomically before
# rebuilding same-tier seam owners.
replace_once(
    lod_cpp,
    '''\tTier.ResolvedTiles.Add(Result.TileCoordinate);\n\n\tif (Result.AppliedRefinement > 1)\n\t{\n\t\tUE_LOG(LogTemp, Warning, TEXT("Cubus terrain LOD%d tile (%d, %d, %d) refined to %dx after a zero-section coarse pass"),\n\t\t\t   Tier.LodLevel, Result.TileCoordinate.X, Result.TileCoordinate.Y, Result.TileCoordinate.Z, Result.AppliedRefinement);\n\t}\n''',
    '''\tconst int32 CurrentSubdivisions = ResolveTierSubdivisions(Tier, Result.TileCoordinate);\n\tif (Result.AppliedRefinement > CurrentSubdivisions)\n\t{\n\t\tTier.RefinementOverrides.Add(Result.TileCoordinate, Result.AppliedRefinement);\n\t\tInvalidateTierTransitionDependencies(Tier, Result.TileCoordinate);\n\t}\n\n\tconst int32 ExpectedSubdivisions = ResolveTierSubdivisions(Tier, Result.TileCoordinate);\n\tconst uint32 ExpectedTransitionSignature =\n\t\tBuildTierTransitionFaces(Tier, Result.TileCoordinate).GetSignature(ExpectedSubdivisions);\n\n\tif (Result.TransitionSignature != ExpectedTransitionSignature ||\n\t\tResult.AppliedRefinement != ExpectedSubdivisions)\n\t{\n\t\tif (!Tier.TilesBuilding.Contains(Result.TileCoordinate))\n\t\t{\n\t\t\tTier.PendingTiles.AddUnique(Result.TileCoordinate);\n\t\t}\n\t\tWorkerMilliseconds += Result.BuildTimeMilliseconds;\n\t\treturn true;\n\t}\n\n\tTier.ResolvedTiles.Add(Result.TileCoordinate);\n\n\tif (Result.AppliedRefinement > Tier.MeshingSubdivisions)\n\t{\n\t\tUE_LOG(LogTemp, Warning, TEXT("Cubus terrain LOD%d tile (%d, %d, %d) promoted to %dx after an empty %dx pass"),\n\t\t\tTier.LodLevel, Result.TileCoordinate.X, Result.TileCoordinate.Y, Result.TileCoordinate.Z,\n\t\t\tResult.AppliedRefinement, Tier.MeshingSubdivisions);\n\t}\n'''
)

# Nested tile placement uses the shared LOD0 corner grid.
replace_once(
    lod_cpp,
    '''\tconst FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);\n\n\tComponent->SetWorldLocation(WorldGridOrigin + FVector(static_cast<double>(TileCoordinate.X) * TileWorldSize,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t  static_cast<double>(TileCoordinate.Y) * TileWorldSize,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t  static_cast<double>(TileCoordinate.Z) * TileWorldSize));\n''',
    '''\tconst FVector WorldGridOrigin = ResolveWorldGridOrigin(CanonicalChunkWorldSize);\n\tconst FVector WorldGridCornerOrigin =\n\t\tWorldGridOrigin - FVector(CanonicalChunkWorldSize * 0.5f);\n\n\tComponent->SetWorldLocation(\n\t\tWorldGridCornerOrigin +\n\t\tFVector(\n\t\t\t(static_cast<double>(TileCoordinate.X) + 0.5) * TileWorldSize,\n\t\t\t(static_cast<double>(TileCoordinate.Y) + 0.5) * TileWorldSize,\n\t\t\t(static_cast<double>(TileCoordinate.Z) + 0.5) * TileWorldSize\n\t\t)\n\t);\n'''
)

# Clear per-tile refinement state with each tier.
replace_once(
    lod_cpp,
    '''\tTier.CompletedBuilds.Reset();\n\tTier.LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n''',
    '''\tTier.CompletedBuilds.Reset();\n\tTier.RefinementOverrides.Reset();\n\tTier.LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n'''
)

# BuildTile: remove the legacy alignment offset, use the nested corner mapping,
# mesh every visual tier at 2x, and if an empty tile must promote to 4x build it
# at the finer equivalent resolution without stale transition faces.
regex_once(
    lod_cpp,
    r'''FCubusTerrainLodTileBuildResult ACubusTerrainLodWorldActor::BuildTile\(const FCubusTerrainLodTileBuildInput& Input\)\n\{.*?\n\}\s*$''',
    '''FCubusTerrainLodTileBuildResult ACubusTerrainLodWorldActor::BuildTile(const FCubusTerrainLodTileBuildInput& Input)\n{\n\tconst double BuildStartTime = FPlatformTime::Seconds();\n\n\tFCubusTerrainLodTileBuildResult Result;\n\tResult.TileCoordinate = Input.TileCoordinate;\n\n\tconst int32 SafeStride = FMath::Clamp(Input.CanonicalVoxelStride, 2, 256);\n\tconst int32 BaseSubdivisions = FCubusDensityLod::NormalizeSubdivisions(Input.MeshingSubdivisions);\n\tconst FCubusTerrainDensityField SourceField(Input.DensitySettings);\n\n\tconst FVector LogicalOrigin(\n\t\tstatic_cast<double>(Input.TileCoordinate.X * Cubus::ChunkSize),\n\t\tstatic_cast<double>(Input.TileCoordinate.Y * Cubus::ChunkSize),\n\t\tstatic_cast<double>(Input.TileCoordinate.Z * Cubus::ChunkSize)\n\t);\n\n\t/*\n\t * All coarse tiers now share the LOD0 chunk-corner grid. A coarse tile at\n\t * stride S covers exactly S canonical chunks, so its source minimum is\n\t * simply logicalOrigin*S. Parent boundaries therefore coincide with child\n\t * boundaries instead of carrying the old half-tile phase offset.\n\t */\n\tconst FVector SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);\n\tconst FCubusScaledDensityField ScaledField(\n\t\tSourceField, LogicalOrigin, SourceOrigin, static_cast<float>(SafeStride)\n\t);\n\n\tconst float ScaledVoxelSize = Input.CanonicalVoxelSize * static_cast<float>(SafeStride);\n\n\tFCubusDensityMesher::BuildAdaptiveChunk(\n\t\tScaledField,\n\t\tInput.TileCoordinate,\n\t\tScaledVoxelSize,\n\t\tBaseSubdivisions,\n\t\tInput.IsoLevel,\n\t\tResult.MaterialMeshes,\n\t\tResult.GeneratedTriangleCount,\n\t\tInput.TransitionFaces\n\t);\n\tResult.AppliedRefinement = BaseSubdivisions;\n\tResult.TransitionSignature = Input.TransitionFaces.GetSignature(BaseSubdivisions);\n\n\tif (Result.MaterialMeshes.IsEmpty() && BaseSubdivisions < 4)\n\t{\n\t\tResult.GeneratedTriangleCount = 0;\n\t\tFCubusDensityTransitionFaces FineEquivalentFaces;\n\t\tFCubusDensityMesher::BuildAdaptiveChunk(\n\t\t\tScaledField,\n\t\t\tInput.TileCoordinate,\n\t\t\tScaledVoxelSize,\n\t\t\t4,\n\t\t\tInput.IsoLevel,\n\t\t\tResult.MaterialMeshes,\n\t\t\tResult.GeneratedTriangleCount,\n\t\t\tFineEquivalentFaces\n\t\t);\n\t\tResult.AppliedRefinement = 4;\n\t\tResult.TransitionSignature = FineEquivalentFaces.GetSignature(4);\n\t}\n\n\tResult.BuildTimeMilliseconds = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;\n\treturn Result;\n}\n'''
)

# Source guards.
checks = {
    world_cpp: [
        'Cubus spawn support ready',
        'Build.bIsBlockBuild ||',
        'ChunkRenderMode == ECubusVoxelRenderMode::Hybrid',
        'DensitySurfaceRetryCoordinates.Remove(Coordinate);',
    ],
    lod_h: [
        'Lod1CanonicalVoxelStride = 2;',
        'Lod2CanonicalVoxelStride = 4;',
        'Lod3CanonicalVoxelStride = 8;',
        'MeshingSubdivisions = 2;',
        'TransitionSignature = 0;',
        'RefinementOverrides',
    ],
    lod_cpp: [
        'WorldGridCornerOrigin',
        'constexpr int32 SafeLod1Stride = 2;',
        'constexpr int32 SafeLod2Stride = 4;',
        'constexpr int32 SafeLod3Stride = 8;',
        'BuildTierTransitionFaces',
        'InvalidateTierTransitionDependencies',
        'SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);',
        'BuildAdaptiveChunk(',
    ],
}
for path, tokens in checks.items():
    text = read(path)
    for token in tokens:
        if token not in text:
            raise RuntimeError(f'{path}: missing required token {token!r}')

# The old independent-grid alignment offset must be gone from the coarse LOD actor.
if 'AlignmentOffset' in read(lod_cpp):
    raise RuntimeError('coarse LOD actor still contains the old half-tile AlignmentOffset')

# Prevent the old visible 4:1 stride defaults from reappearing.
for bad in ['Lod2CanonicalVoxelStride = 16;', 'Lod3CanonicalVoxelStride = 64;']:
    if bad in read(lod_h):
        raise RuntimeError(f'legacy 4:1 stride remains: {bad}')
