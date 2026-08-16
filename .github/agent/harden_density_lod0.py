from pathlib import Path
import re

ROOT = Path('.')
LOD_H = ROOT / 'Source/Orakai/CubusCore/Meshing/CubusDensityLod.h'
WORLD_H = ROOT / 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.h'
WORLD_CPP = ROOT / 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
MESHER_CPP = ROOT / 'Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp'


def read(path):
    return path.read_text(encoding='utf-8')


def write(path, text):
    path.write_text(text, encoding='utf-8')


def replace_once(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: exact anchor count {count}: {old[:160]!r}')
    write(path, text.replace(old, new, 1))


def regex_once(path, pattern, replacement):
    text = read(path)
    output, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f'{path}: regex anchor count {count}: {pattern[:160]!r}')
    write(path, output)


# ---------------------------------------------------------------------------
# Shared 2D aligned bounds. These are half-open integer tile bounds so every
# power-of-two LOD can derive an exact child/parent boundary with no phase drift.
# ---------------------------------------------------------------------------
replace_once(
    LOD_H,
    '''/**\n * Density-LOD scale rules shared by streaming, chunks and meshing.\n''',
    '''/** Half-open aligned XY bounds used by the density clipmap hierarchy. */\nstruct ORAKAI_API FCubusDensityTileBounds2D\n{\n    FIntPoint Min = FIntPoint::ZeroValue;\n    FIntPoint MaxExclusive = FIntPoint::ZeroValue;\n\n    bool IsValid() const\n    {\n        return MaxExclusive.X > Min.X && MaxExclusive.Y > Min.Y;\n    }\n\n    int32 WidthX() const { return MaxExclusive.X - Min.X; }\n    int32 WidthY() const { return MaxExclusive.Y - Min.Y; }\n\n    bool Contains(const int32 X, const int32 Y) const\n    {\n        return X >= Min.X && X < MaxExclusive.X &&\n               Y >= Min.Y && Y < MaxExclusive.Y;\n    }\n\n    bool operator==(const FCubusDensityTileBounds2D& Other) const\n    {\n        return Min == Other.Min && MaxExclusive == Other.MaxExclusive;\n    }\n\n    bool operator!=(const FCubusDensityTileBounds2D& Other) const\n    {\n        return !(*this == Other);\n    }\n};\n\n/**\n * Density-LOD scale rules shared by streaming, chunks and meshing.\n'''
)

replace_once(
    LOD_H,
    '''    static int32 ChunkDistance(\n        const FIntVector& A,\n        const FIntVector& B\n    )\n    {\n        const FIntVector Delta = A - B;\n        return FMath::Max3(\n            FMath::Abs(Delta.X),\n            FMath::Abs(Delta.Y),\n            FMath::Abs(Delta.Z)\n        );\n    }\n};''',
    '''    static int32 ChunkDistance(\n        const FIntVector& A,\n        const FIntVector& B\n    )\n    {\n        const FIntVector Delta = A - B;\n        return FMath::Max3(\n            FMath::Abs(Delta.X),\n            FMath::Abs(Delta.Y),\n            FMath::Abs(Delta.Z)\n        );\n    }\n\n    static int32 HorizontalChunkDistance(\n        const FIntVector& A,\n        const FIntVector& B\n    )\n    {\n        return FMath::Max(\n            FMath::Abs(A.X - B.X),\n            FMath::Abs(A.Y - B.Y)\n        );\n    }\n\n    static int32 FloorDivide(const int32 Value, const int32 Divisor)\n    {\n        check(Divisor > 0);\n        int32 Quotient = Value / Divisor;\n        const int32 Remainder = Value % Divisor;\n        if (Remainder < 0)\n        {\n            --Quotient;\n        }\n        return Quotient;\n    }\n\n    static int32 AlignDown(const int32 Value, const int32 Alignment)\n    {\n        const int32 SafeAlignment = FMath::Max(1, Alignment);\n        return FloorDivide(Value, SafeAlignment) * SafeAlignment;\n    }\n\n    static FCubusDensityTileBounds2D BuildAlignedCoverage(\n        const FIntPoint& Centre,\n        const int32 HalfSpan,\n        const int32 Alignment = 2\n    )\n    {\n        const int32 SafeHalfSpan = FMath::Max(1, HalfSpan);\n        const int32 SafeAlignment = FMath::Max(1, Alignment);\n        const int32 Width = SafeHalfSpan * 2;\n\n        FCubusDensityTileBounds2D Result;\n        Result.Min.X = AlignDown(Centre.X - SafeHalfSpan, SafeAlignment);\n        Result.Min.Y = AlignDown(Centre.Y - SafeHalfSpan, SafeAlignment);\n        Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);\n\n        if (!Result.Contains(Centre.X, Centre.Y))\n        {\n            if (Centre.X < Result.Min.X)\n            {\n                Result.Min.X -= SafeAlignment;\n            }\n            else if (Centre.X >= Result.MaxExclusive.X)\n            {\n                Result.Min.X += SafeAlignment;\n            }\n\n            if (Centre.Y < Result.Min.Y)\n            {\n                Result.Min.Y -= SafeAlignment;\n            }\n            else if (Centre.Y >= Result.MaxExclusive.Y)\n            {\n                Result.Min.Y += SafeAlignment;\n            }\n            Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);\n        }\n\n        return Result;\n    }\n\n    static FCubusDensityTileBounds2D ScaleDownExact(\n        const FCubusDensityTileBounds2D& Bounds,\n        const int32 Factor = 2\n    )\n    {\n        const int32 SafeFactor = FMath::Max(1, Factor);\n        ensureMsgf(\n            Bounds.Min.X % SafeFactor == 0 && Bounds.Min.Y % SafeFactor == 0 &&\n            Bounds.MaxExclusive.X % SafeFactor == 0 && Bounds.MaxExclusive.Y % SafeFactor == 0,\n            TEXT("Cubus density clipmap bounds must be exactly aligned before scaling")\n        );\n\n        FCubusDensityTileBounds2D Result;\n        Result.Min = FIntPoint(\n            FloorDivide(Bounds.Min.X, SafeFactor),\n            FloorDivide(Bounds.Min.Y, SafeFactor)\n        );\n        Result.MaxExclusive = FIntPoint(\n            FloorDivide(Bounds.MaxExclusive.X, SafeFactor),\n            FloorDivide(Bounds.MaxExclusive.Y, SafeFactor)\n        );\n        return Result;\n    }\n\n    static FCubusDensityTileBounds2D BuildAlignedOuterBounds(\n        const FCubusDensityTileBounds2D& InnerBounds,\n        const int32 HalfSpan,\n        const int32 Alignment = 2\n    )\n    {\n        check(InnerBounds.IsValid());\n        const int32 SafeHalfSpan = FMath::Max(2, HalfSpan);\n        const int32 SafeAlignment = FMath::Max(1, Alignment);\n        const int32 Width = SafeHalfSpan * 2;\n        check(Width >= InnerBounds.WidthX() && Width >= InnerBounds.WidthY());\n\n        auto ResolveAxis = [Width, SafeAlignment](const int32 InnerMin, const int32 InnerMax)\n        {\n            const int32 TwiceCentre = InnerMin + InnerMax;\n            int32 Minimum = AlignDown((TwiceCentre - Width) / 2, SafeAlignment);\n            while (Minimum > InnerMin)\n            {\n                Minimum -= SafeAlignment;\n            }\n            while (Minimum + Width < InnerMax)\n            {\n                Minimum += SafeAlignment;\n            }\n            return Minimum;\n        };\n\n        FCubusDensityTileBounds2D Result;\n        Result.Min.X = ResolveAxis(InnerBounds.Min.X, InnerBounds.MaxExclusive.X);\n        Result.Min.Y = ResolveAxis(InnerBounds.Min.Y, InnerBounds.MaxExclusive.Y);\n        Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);\n        return Result;\n    }\n};'''
)

# ---------------------------------------------------------------------------
# Block-world public contract and density-only tuning/state.
# ---------------------------------------------------------------------------
replace_once(
    WORLD_H,
    '''\tFCubusDensityTransitionFaces BuildDensityTransitionFaces(const FIntVector& ChunkCoordinate, int32 SelfSubdivisions) const;\n\n\tbool IsWorldVegetationEnabled() const { return bEnableWorldVegetation; }\n''',
    '''\tFCubusDensityTransitionFaces BuildDensityTransitionFaces(const FIntVector& ChunkCoordinate, int32 SelfSubdivisions) const;\n\n\tconst FCubusDensityTileBounds2D& GetDensityStreamingCoverageBounds() const { return DensityStreamingCoverageBounds; }\n\tbool IsDensityStreamingCoverageReady() const;\n\n\tbool IsWorldVegetationEnabled() const { return bEnableWorldVegetation; }\n'''
)

replace_once(
    WORLD_H,
    '''\tUPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "16"))\n\tint32 VerticalViewRadius = 2;\n''',
    '''\tUPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "16"))\n\tint32 VerticalViewRadius = 2;\n\n\t/**\n\t * Density terrain follows the generated surface per XY column instead of\n\t * loading a tall ellipsoid around one Z anchor. One chunk of padding above\n\t * and below the surface is enough for the bounded geology band and removes\n\t * large amounts of empty/deep density work.\n\t */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Density LOD", meta = (ClampMin = "0", ClampMax = "2", UIMax = "2"))\n\tint32 DensitySurfaceVerticalPaddingChunks = 1;\n'''
)

replace_once(
    WORLD_H,
    '''\tTSet<FIntVector> StreamingChunksReady;\n\tTSet<FIntVector> DensitySurfaceRetryCoordinates;\n''',
    '''\tTSet<FIntVector> StreamingChunksReady;\n\tTSet<FIntVector> DensitySurfaceRetryCoordinates;\n\n\t/* LOD resolution swaps are staged and published as one local topology transaction. */\n\tTSet<FIntVector> ActiveDensityLodTransitionCoordinates;\n\tTSet<FIntVector> StagedDensityLodTransitionCoordinates;\n\tbool bDensityLodTransitionActive = false;\n'''
)

replace_once(
    WORLD_H,
    '''\tFIntVector LastTrackedChunk\t\t\t\t  = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n\tFVector\t   HeldPawnLocation''',
    '''\tFIntVector LastTrackedChunk\t\t\t\t  = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n\tFIntVector DensityLodCentreChunk\t\t  = FIntVector(MAX_int32, MAX_int32, MAX_int32);\n\tFCubusDensityTileBounds2D DensityStreamingCoverageBounds;\n\tFVector\t   HeldPawnLocation'''
)

replace_once(
    WORLD_H,
    '''\tvoid\t   BuildRequiredCoordinates(const FIntVector& CentreCoordinate, int32 HorizontalRadius, int32 VerticalRadius,\n\t\t\t\t\t\t\t\t\t\tTSet<FIntVector>& OutCoordinates) const;\n\tFIntVector WorldLocationToChunkCoordinate(const FVector& WorldLocation) const;\n''',
    '''\tvoid\t   BuildRequiredCoordinates(const FIntVector& CentreCoordinate, int32 HorizontalRadius, int32 VerticalRadius,\n\t\t\t\t\t\t\t\t\t\tTSet<FIntVector>& OutCoordinates) const;\n\tvoid BuildDensitySurfaceRequiredCoordinates(\n\t\tconst FCubusDensityTileBounds2D& CoverageBounds,\n\t\tconst FCubusTerrainFormSettings& TerrainSettings,\n\t\tint32 TerrainOffsetX,\n\t\tint32 TerrainOffsetY,\n\t\tint32 VerticalPadding,\n\t\tTSet<FIntVector>& OutCoordinates\n\t) const;\n\tbool AreRequiredStreamingChunksReady() const;\n\tvoid TryCommitDensityLodTransition();\n\tFIntVector WorldLocationToChunkCoordinate(const FVector& WorldLocation) const;\n'''
)

# ---------------------------------------------------------------------------
# Surface-following density footprint helper + readiness contract.
# ---------------------------------------------------------------------------
replace_once(
    WORLD_CPP,
    '''FIntVector ACubusBlockWorldActor::WorldLocationToChunkCoordinate(const FVector& WorldLocation) const\n''',
    '''void ACubusBlockWorldActor::BuildDensitySurfaceRequiredCoordinates(\n\tconst FCubusDensityTileBounds2D& CoverageBounds,\n\tconst FCubusTerrainFormSettings& TerrainSettings,\n\tconst int32 TerrainOffsetX,\n\tconst int32 TerrainOffsetY,\n\tconst int32 VerticalPadding,\n\tTSet<FIntVector>& OutCoordinates\n) const\n{\n\tOutCoordinates.Reset();\n\tif (!CoverageBounds.IsValid())\n\t{\n\t\treturn;\n\t}\n\n\tconst int32 SafePadding = FMath::Clamp(VerticalPadding, 0, 2);\n\tconst float HalfChunk = static_cast<float>(Cubus::ChunkSize) * 0.5f;\n\n\tfor (int32 ChunkY = CoverageBounds.Min.Y; ChunkY < CoverageBounds.MaxExclusive.Y; ++ChunkY)\n\t{\n\t\tfor (int32 ChunkX = CoverageBounds.Min.X; ChunkX < CoverageBounds.MaxExclusive.X; ++ChunkX)\n\t\t{\n\t\t\tconst float SurfaceSampleX =\n\t\t\t\tstatic_cast<float>(ChunkX * Cubus::ChunkSize) + HalfChunk + static_cast<float>(TerrainOffsetX);\n\t\t\tconst float SurfaceSampleY =\n\t\t\t\tstatic_cast<float>(ChunkY * Cubus::ChunkSize) + HalfChunk + static_cast<float>(TerrainOffsetY);\n\n\t\t\tconst float SurfaceVoxelZ = bUseHeightTerrain\n\t\t\t\t? FCubusTerrainForm::Sample(SurfaceSampleX, SurfaceSampleY, TerrainSettings).Height\n\t\t\t\t: static_cast<float>(TerrainSurfaceWorldZ);\n\t\t\tconst int32 SurfaceChunkZ = FMath::FloorToInt(\n\t\t\t\tSurfaceVoxelZ / static_cast<float>(Cubus::ChunkSize)\n\t\t\t);\n\n\t\t\tfor (int32 DeltaZ = -SafePadding; DeltaZ <= SafePadding; ++DeltaZ)\n\t\t\t{\n\t\t\t\tOutCoordinates.Add(FIntVector(ChunkX, ChunkY, SurfaceChunkZ + DeltaZ));\n\t\t\t}\n\t\t}\n\t}\n}\n\nbool ACubusBlockWorldActor::AreRequiredStreamingChunksReady() const\n{\n\tif (RequiredChunkCoordinates.IsEmpty())\n\t{\n\t\treturn false;\n\t}\n\n\tfor (const FIntVector& Coordinate : RequiredChunkCoordinates)\n\t{\n\t\tif (!IsValid(FindChunk(Coordinate)) || !StreamingChunksReady.Contains(Coordinate))\n\t\t{\n\t\t\treturn false;\n\t\t}\n\t}\n\treturn true;\n}\n\nbool ACubusBlockWorldActor::IsDensityStreamingCoverageReady() const\n{\n\tconst ECubusVoxelRenderMode RenderMode = GetVoxelRenderMode();\n\tif (RenderMode != ECubusVoxelRenderMode::Density && RenderMode != ECubusVoxelRenderMode::Hybrid)\n\t{\n\t\treturn false;\n\t}\n\treturn DensityStreamingCoverageBounds.IsValid() && AreRequiredStreamingChunksReady();\n}\n\nFIntVector ACubusBlockWorldActor::WorldLocationToChunkCoordinate(const FVector& WorldLocation) const\n'''
)

# Replace runtime streaming as a whole. Density/Hybrid get an aligned, per-column
# surface footprint; Blocks retain the old ellipsoid.
regex_once(
    WORLD_CPP,
    r'''void ACubusBlockWorldActor::UpdateRuntimeStreaming\(const bool bForce\)\n\{.*?\n\}\n\nint32 ACubusBlockWorldActor::ResolveDensitySubdivisions''',
    '''void ACubusBlockWorldActor::UpdateRuntimeStreaming(const bool bForce)\n{\n\tAPawn* PlayerPawn = TrackedPawn.Get();\n\tif (!IsValid(PlayerPawn))\n\t{\n\t\tPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);\n\t\tif (IsValid(PlayerPawn))\n\t\t{\n\t\t\tTrackedPawn = PlayerPawn;\n\t\t}\n\t}\n\n\tconst FVector TrackingLocation =\n\t\tbPawnHeldForStreaming ? HeldPawnLocation : (IsValid(PlayerPawn) ? PlayerPawn->GetActorLocation() : GetActorLocation());\n\tconst FIntVector PawnCoordinate = WorldLocationToChunkCoordinate(TrackingLocation);\n\n\tFCubusTerrainFormSettings StreamingTerrainSettings;\n\tStreamingTerrainSettings.BaseHeight = static_cast<float>(TerrainBaseHeight);\n\tStreamingTerrainSettings.VoxelSizeCm = GeneratedVoxelSize;\n\tStreamingTerrainSettings.ContinentAmplitude = TerrainContinentAmplitude;\n\tStreamingTerrainSettings.ContinentFrequency = TerrainContinentFrequency;\n\tStreamingTerrainSettings.HillAmplitude = TerrainHillAmplitude;\n\tStreamingTerrainSettings.HillFrequency = TerrainHillFrequency;\n\tStreamingTerrainSettings.DetailAmplitude = TerrainDetailAmplitude;\n\tStreamingTerrainSettings.DetailFrequency = TerrainDetailFrequency;\n\tStreamingTerrainSettings.RidgeAmplitude = TerrainRidgeAmplitude;\n\tStreamingTerrainSettings.RidgeFrequency = TerrainRidgeFrequency;\n\tStreamingTerrainSettings.ValleyDepth = TerrainValleyDepth;\n\tStreamingTerrainSettings.ValleyFrequency = TerrainValleyFrequency;\n\tStreamingTerrainSettings.ValleyWidth = TerrainValleyWidth;\n\tStreamingTerrainSettings.ValleyFalloff = TerrainValleyFalloff;\n\tStreamingTerrainSettings.ValleyWarpAmplitude = TerrainValleyWarpAmplitude;\n\tStreamingTerrainSettings.ValleyWarpFrequency = TerrainValleyWarpFrequency;\n\tStreamingTerrainSettings.RegionFrequency = TerrainRegionFrequency;\n\tStreamingTerrainSettings.PlainsThreshold = TerrainPlainsThreshold;\n\tStreamingTerrainSettings.PlainsBlend = TerrainPlainsBlend;\n\tStreamingTerrainSettings.MountainThreshold = TerrainMountainThreshold;\n\tStreamingTerrainSettings.MountainBlend = TerrainMountainBlend;\n\n\tconst FCubusGenerationSeeds Seeds = GetGenerationSeeds();\n\tconst int32 TerrainOffsetX =\n\t\t(FCubusGenerationSeeds::DomainOffsetX(Seeds.Terrain) / Cubus::ChunkSize) * Cubus::ChunkSize;\n\tconst int32 TerrainOffsetY =\n\t\t(FCubusGenerationSeeds::DomainOffsetY(Seeds.Terrain) / Cubus::ChunkSize) * Cubus::ChunkSize;\n\n\tconst float HorizontalChunkCentreOffset = static_cast<float>(Cubus::ChunkSize) * 0.5f;\n\tconst float SurfaceSampleX =\n\t\tstatic_cast<float>(PawnCoordinate.X * Cubus::ChunkSize) + HorizontalChunkCentreOffset + static_cast<float>(TerrainOffsetX);\n\tconst float SurfaceSampleY =\n\t\tstatic_cast<float>(PawnCoordinate.Y * Cubus::ChunkSize) + HorizontalChunkCentreOffset + static_cast<float>(TerrainOffsetY);\n\tconst float TerrainSurfaceVoxelZ = bUseHeightTerrain\n\t\t? FCubusTerrainForm::Sample(SurfaceSampleX, SurfaceSampleY, StreamingTerrainSettings).Height\n\t\t: static_cast<float>(TerrainSurfaceWorldZ);\n\tconst int32 TerrainChunkZ = FMath::FloorToInt(\n\t\tTerrainSurfaceVoxelZ / static_cast<float>(Cubus::ChunkSize)\n\t);\n\tconst FIntVector CentreCoordinate(PawnCoordinate.X, PawnCoordinate.Y, TerrainChunkZ);\n\n\tconst ECubusVoxelRenderMode RenderMode = GetVoxelRenderMode();\n\tconst bool bDensityWorld =\n\t\tRenderMode == ECubusVoxelRenderMode::Density || RenderMode == ECubusVoxelRenderMode::Hybrid;\n\n\tconst int32 HorizontalRadius = bInitialSpawnAreaReady ? HorizontalViewRadius : InitialLoadRadius;\n\tconst int32 VerticalRadius = bInitialSpawnAreaReady ? VerticalViewRadius : InitialVerticalLoadRadius;\n\tTSet<FIntVector> DesiredRequiredCoordinates;\n\tFCubusDensityTileBounds2D DesiredCoverageBounds;\n\n\tif (bDensityWorld)\n\t{\n\t\tDesiredCoverageBounds = FCubusDensityLod::BuildAlignedCoverage(\n\t\t\tFIntPoint(CentreCoordinate.X, CentreCoordinate.Y),\n\t\t\tFMath::Max(1, HorizontalRadius),\n\t\t\t2\n\t\t);\n\t\tBuildDensitySurfaceRequiredCoordinates(\n\t\t\tDesiredCoverageBounds,\n\t\t\tStreamingTerrainSettings,\n\t\t\tTerrainOffsetX,\n\t\t\tTerrainOffsetY,\n\t\t\tFMath::Min(VerticalRadius, DensitySurfaceVerticalPaddingChunks),\n\t\t\tDesiredRequiredCoordinates\n\t\t);\n\t}\n\telse\n\t{\n\t\tBuildRequiredCoordinates(\n\t\t\tCentreCoordinate, HorizontalRadius, VerticalRadius, DesiredRequiredCoordinates\n\t\t);\n\t}\n\n\tconst bool bCentreChanged = CentreCoordinate != LastTrackedChunk;\n\tconst bool bCoverageChanged = bDensityWorld\n\t\t? DesiredCoverageBounds != DensityStreamingCoverageBounds\n\t\t: DensityStreamingCoverageBounds.IsValid();\n\n\tif (!bForce && !bCentreChanged && !bCoverageChanged)\n\t{\n\t\treturn;\n\t}\n\n\tLastTrackedChunk = CentreCoordinate;\n\tif (bDensityWorld)\n\t{\n\t\tDensityStreamingCoverageBounds = DesiredCoverageBounds;\n\t\tconst FIntPoint CoverageCentre(\n\t\t\t(DesiredCoverageBounds.Min.X + DesiredCoverageBounds.MaxExclusive.X) / 2,\n\t\t\t(DesiredCoverageBounds.Min.Y + DesiredCoverageBounds.MaxExclusive.Y) / 2\n\t\t);\n\n\t\t/*\n\t\t * The LOD centre follows the aligned coverage block, not every player\n\t\t * chunk. That moves the 4x/2x rings only once per two-chunk clipmap step\n\t\t * and guarantees every outer LOD0 edge chunk is the 2x seam resolution.\n\t\t */\n\t\tif (!bDensityLodTransitionActive || DensityLodCentreChunk.X == MAX_int32)\n\t\t{\n\t\t\tconst FIntVector NewLodCentre(CoverageCentre.X, CoverageCentre.Y, CentreCoordinate.Z);\n\t\t\tif (NewLodCentre.X != DensityLodCentreChunk.X || NewLodCentre.Y != DensityLodCentreChunk.Y)\n\t\t\t{\n\t\t\t\tDensityLodCentreChunk = NewLodCentre;\n\t\t\t\tUpdateDensityLods();\n\t\t\t}\n\t\t}\n\t}\n\telse\n\t{\n\t\tDensityStreamingCoverageBounds = FCubusDensityTileBounds2D();\n\t}\n\n\tif (!bInitialSpawnAreaReady)\n\t{\n\t\tInitialRequiredCoordinates = DesiredRequiredCoordinates;\n\t\tRequiredChunkCoordinates = InitialRequiredCoordinates;\n\t}\n\telse\n\t{\n\t\tRequiredChunkCoordinates = MoveTemp(DesiredRequiredCoordinates);\n\t}\n\n\tPendingChunkGeneration.Reset();\n\tPendingChunkRemoval.Reset();\n\n\tfor (const FIntVector& Coordinate : RequiredChunkCoordinates)\n\t{\n\t\tif (!IsValid(FindChunk(Coordinate)))\n\t\t{\n\t\t\tPendingChunkGeneration.Add(Coordinate);\n\t\t}\n\t}\n\n\tPendingChunkGeneration.Sort(\n\t\t[CentreCoordinate](const FIntVector& A, const FIntVector& B)\n\t\t{\n\t\t\tconst int32 DistanceA = FCubusDensityLod::ChunkDistance(A, CentreCoordinate);\n\t\t\tconst int32 DistanceB = FCubusDensityLod::ChunkDistance(B, CentreCoordinate);\n\t\t\treturn DistanceA > DistanceB;\n\t\t}\n\t);\n\n\tfor (const auto& Entry : ChunksByCoordinate)\n\t{\n\t\tif (!RequiredChunkCoordinates.Contains(Entry.Key))\n\t\t{\n\t\t\tPendingChunkRemoval.Add(Entry.Key);\n\t\t}\n\t}\n\n\tPendingRuntimeChunkCount = PendingChunkGeneration.Num();\n}\n\nint32 ACubusBlockWorldActor::ResolveDensitySubdivisions'''
)

# Horizontal LOD only: surface-following columns should not become coarse just
# because their actual terrain Z differs from the player's column.
regex_once(
    WORLD_CPP,
    r'''int32 ACubusBlockWorldActor::ResolveDensitySubdivisions\(const FIntVector& ChunkCoordinate\) const\n\{.*?\n\}\n\nFCubusDensityTransitionFaces ACubusBlockWorldActor::BuildDensityTransitionFaces''',
    '''int32 ACubusBlockWorldActor::ResolveDensitySubdivisions(const FIntVector& ChunkCoordinate) const\n{\n\tif (!bEnableDensityLod)\n\t{\n\t\treturn 1;\n\t}\n\n\tconst bool bHasLodCentre =\n\t\tDensityLodCentreChunk.X != MAX_int32 && DensityLodCentreChunk.Y != MAX_int32;\n\tfloat TargetSpacing = DensityFarSampleSpacing;\n\n\tif (bHasLodCentre)\n\t{\n\t\tconst int32 Distance = FCubusDensityLod::HorizontalChunkDistance(\n\t\t\tChunkCoordinate, DensityLodCentreChunk\n\t\t);\n\t\tif (Distance <= DensityNearChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityNearSampleSpacing;\n\t\t}\n\t\telse if (Distance <= DensityMiddleChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityMiddleSampleSpacing;\n\t\t}\n\t}\n\n\tconst int32 Resolved = FCubusDensityLod::ResolveSubdivisionsForSpacing(\n\t\tGeneratedVoxelSize, TargetSpacing\n\t);\n\treturn DensitySurfaceRetryCoordinates.Contains(ChunkCoordinate)\n\t\t? 4\n\t\t: FMath::Clamp(Resolved, 2, 4);\n}\n\nFCubusDensityTransitionFaces ACubusBlockWorldActor::BuildDensityTransitionFaces'''
)

# LOD0 resolution changes are now a local atomic topology transaction. Every
# changed chunk and face neighbour stages first; none publish mixed topology.
regex_once(
    WORLD_CPP,
    r'''void ACubusBlockWorldActor::UpdateDensityLods\(\)\n\{.*?\n\}\n\nvoid ACubusBlockWorldActor::QueueStreamingChunkBuilds\(\)''',
    '''void ACubusBlockWorldActor::UpdateDensityLods()\n{\n\tconst ECubusVoxelRenderMode RenderMode = GetVoxelRenderMode();\n\tif (RenderMode != ECubusVoxelRenderMode::Density && RenderMode != ECubusVoxelRenderMode::Hybrid)\n\t{\n\t\treturn;\n\t}\n\n\tTSet<FIntVector> AffectedCoordinates;\n\tfor (const auto& Entry : ChunksByCoordinate)\n\t{\n\t\tif (!RequiredChunkCoordinates.IsEmpty() && !RequiredChunkCoordinates.Contains(Entry.Key))\n\t\t{\n\t\t\tcontinue;\n\t\t}\n\n\t\tACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();\n\t\tif (!IsValid(ChunkActor))\n\t\t{\n\t\t\tcontinue;\n\t\t}\n\n\t\tif (ChunkActor->ConfigureDensityResolution(ResolveDensitySubdivisions(Entry.Key)))\n\t\t{\n\t\t\tAffectedCoordinates.Add(Entry.Key);\n\t\t\tfor (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)\n\t\t\t{\n\t\t\t\tconst FIntVector Neighbour = Entry.Key + FCubusDensityTransitionFaces::GetOffset(\n\t\t\t\t\tstatic_cast<ECubusDensityFace>(FaceIndex)\n\t\t\t\t);\n\t\t\t\tif (RequiredChunkCoordinates.Contains(Neighbour) && IsValid(FindChunk(Neighbour)))\n\t\t\t\t{\n\t\t\t\t\tAffectedCoordinates.Add(Neighbour);\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n\n\tif (AffectedCoordinates.IsEmpty())\n\t{\n\t\treturn;\n\t}\n\n\t/* Restart cleanly if an empty-surface promotion changes topology mid-batch. */\n\tif (bDensityLodTransitionActive)\n\t{\n\t\tfor (const FIntVector& Coordinate : StagedDensityLodTransitionCoordinates)\n\t\t{\n\t\t\tif (ACubusVoxelVolumeActor* Chunk = FindChunk(Coordinate))\n\t\t\t{\n\t\t\t\tChunk->DiscardStagedVolume();\n\t\t\t}\n\t\t}\n\t\tAffectedCoordinates.Append(ActiveDensityLodTransitionCoordinates);\n\t}\n\n\tActiveDensityLodTransitionCoordinates = MoveTemp(AffectedCoordinates);\n\tStagedDensityLodTransitionCoordinates.Reset();\n\tbDensityLodTransitionActive = true;\n\n\tfor (const FIntVector& Coordinate : ActiveDensityLodTransitionCoordinates)\n\t{\n\t\tStreamingChunksReady.Remove(Coordinate);\n\t}\n}\n\nvoid ACubusBlockWorldActor::TryCommitDensityLodTransition()\n{\n\tif (!bDensityLodTransitionActive)\n\t{\n\t\treturn;\n\t}\n\n\tfor (auto Iterator = ActiveDensityLodTransitionCoordinates.CreateIterator(); Iterator; ++Iterator)\n\t{\n\t\tif (!RequiredChunkCoordinates.Contains(*Iterator) || !IsValid(FindChunk(*Iterator)))\n\t\t{\n\t\t\tStagedDensityLodTransitionCoordinates.Remove(*Iterator);\n\t\t\tIterator.RemoveCurrent();\n\t\t}\n\t}\n\n\tfor (const FIntVector& Coordinate : ActiveDensityLodTransitionCoordinates)\n\t{\n\t\tif (!StagedDensityLodTransitionCoordinates.Contains(Coordinate))\n\t\t{\n\t\t\treturn;\n\t\t}\n\t}\n\n\tfor (const FIntVector& Coordinate : ActiveDensityLodTransitionCoordinates)\n\t{\n\t\tif (ACubusVoxelVolumeActor* Chunk = FindChunk(Coordinate))\n\t\t{\n\t\t\tChunk->CommitStagedVolume();\n\t\t\tStreamingChunksReady.Add(Coordinate);\n\t\t}\n\t}\n\n\tActiveDensityLodTransitionCoordinates.Reset();\n\tStagedDensityLodTransitionCoordinates.Reset();\n\tbDensityLodTransitionActive = false;\n\tTimeUntilStreamingUpdate = 0.0f;\n}\n\nvoid ACubusBlockWorldActor::QueueStreamingChunkBuilds()'''
)

# Staged LOD participants must not be queued a second time while waiting for the
# other chunks in their topology transaction.
replace_once(
    WORLD_CPP,
    '''\t\tif (StreamingChunksReady.Contains(Coordinate) || StreamingChunksBuilding.Contains(Coordinate))\n''',
    '''\t\tif (StreamingChunksReady.Contains(Coordinate) || StreamingChunksBuilding.Contains(Coordinate) ||\n\t\t\tStagedDensityLodTransitionCoordinates.Contains(Coordinate))\n'''
)

# Publish density/hybrid results atomically when they belong to the active LOD
# topology batch. Ordinary streaming still commits immediately.
replace_once(
    WORLD_CPP,
    '''\t\t\t\telse if (Chunk->BuildStagedVolumeFromDensityMesh(Result))\n\t\t\t\t{\n\t\t\t\t\tChunk->CommitStagedVolume();\n\t\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t\t}\n''',
    '''\t\t\t\telse if (Chunk->BuildStagedVolumeFromDensityMesh(Result))\n\t\t\t\t{\n\t\t\t\t\tif (bDensityLodTransitionActive && ActiveDensityLodTransitionCoordinates.Contains(Build.Coordinate))\n\t\t\t\t\t{\n\t\t\t\t\t\tStagedDensityLodTransitionCoordinates.Add(Build.Coordinate);\n\t\t\t\t\t}\n\t\t\t\t\telse\n\t\t\t\t\t{\n\t\t\t\t\t\tChunk->CommitStagedVolume();\n\t\t\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t\t\t}\n\t\t\t\t}\n'''
)

replace_once(
    WORLD_CPP,
    '''\t\t\t\tif (bStaged)\n\t\t\t\t{\n\t\t\t\t\tChunk->CommitStagedVolume();\n\n\t\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t\t}\n''',
    '''\t\t\t\tif (bStaged)\n\t\t\t\t{\n\t\t\t\t\tif (Build.bIsHybridBuild && bDensityLodTransitionActive &&\n\t\t\t\t\t\tActiveDensityLodTransitionCoordinates.Contains(Build.Coordinate))\n\t\t\t\t\t{\n\t\t\t\t\t\tStagedDensityLodTransitionCoordinates.Add(Build.Coordinate);\n\t\t\t\t\t}\n\t\t\t\t\telse\n\t\t\t\t\t{\n\t\t\t\t\t\tChunk->CommitStagedVolume();\n\t\t\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t\t\t}\n\t\t\t\t}\n'''
)

replace_once(
    WORLD_CPP,
    '''\t\t++UploadedCount;\n\t}\n}\n\nvoid ACubusBlockWorldActor::ProcessRuntimeQueues()\n''',
    '''\t\t++UploadedCount;\n\t}\n\n\tTryCommitDensityLodTransition();\n}\n\nvoid ACubusBlockWorldActor::ProcessRuntimeQueues()\n'''
)

# Never retire the previous coverage while any required replacement chunk is
# absent, building, staged, or undergoing an LOD topology swap.
replace_once(
    WORLD_CPP,
    '''\tint32 RemovedCount = 0;\n\n\twhile (RemovedCount < MaxChunksRemovedPerTick && !PendingChunkRemoval.IsEmpty())\n''',
    '''\tint32 RemovedCount = 0;\n\n\tconst bool bReplacementCoverageReady = AreRequiredStreamingChunksReady();\n\twhile (bReplacementCoverageReady && RemovedCount < MaxChunksRemovedPerTick && !PendingChunkRemoval.IsEmpty())\n'''
)

# ---------------------------------------------------------------------------
# Transition cell face bases must all have identical handedness. Previously
# NegativeX, PositiveY and NegativeZ were mirrored relative to the table.
# Winding correction cannot repair an ambiguous-case topology reflection.
# ---------------------------------------------------------------------------
regex_once(
    MESHER_CPP,
    r'''FTransitionFaceBasis GetTransitionFaceBasis\(const ECubusDensityFace Face\)\n\{.*?\n\treturn Result;\n\}''',
    '''FTransitionFaceBasis GetTransitionFaceBasis(const ECubusDensityFace Face)\n{\n\tFTransitionFaceBasis Result;\n\tswitch (Face)\n\t{\n\tcase ECubusDensityFace::NegativeX:\n\t\tResult.BoundaryOrigin = FVector(0.0, 0.0, Cubus::ChunkSize);\n\t\tResult.Inward = FVector(1.0, 0.0, 0.0);\n\t\tResult.U = FVector(0.0, 1.0, 0.0);\n\t\tResult.V = FVector(0.0, 0.0, -1.0);\n\t\tbreak;\n\tcase ECubusDensityFace::PositiveX:\n\t\tResult.BoundaryOrigin = FVector(Cubus::ChunkSize, 0.0, 0.0);\n\t\tResult.Inward = FVector(-1.0, 0.0, 0.0);\n\t\tResult.U = FVector(0.0, 1.0, 0.0);\n\t\tResult.V = FVector(0.0, 0.0, 1.0);\n\t\tbreak;\n\tcase ECubusDensityFace::NegativeY:\n\t\tResult.BoundaryOrigin = FVector(0.0, 0.0, 0.0);\n\t\tResult.Inward = FVector(0.0, 1.0, 0.0);\n\t\tResult.U = FVector(1.0, 0.0, 0.0);\n\t\tResult.V = FVector(0.0, 0.0, 1.0);\n\t\tbreak;\n\tcase ECubusDensityFace::PositiveY:\n\t\tResult.BoundaryOrigin = FVector(0.0, Cubus::ChunkSize, Cubus::ChunkSize);\n\t\tResult.Inward = FVector(0.0, -1.0, 0.0);\n\t\tResult.U = FVector(1.0, 0.0, 0.0);\n\t\tResult.V = FVector(0.0, 0.0, -1.0);\n\t\tbreak;\n\tcase ECubusDensityFace::NegativeZ:\n\t\tResult.BoundaryOrigin = FVector(0.0, Cubus::ChunkSize, 0.0);\n\t\tResult.Inward = FVector(0.0, 0.0, 1.0);\n\t\tResult.U = FVector(1.0, 0.0, 0.0);\n\t\tResult.V = FVector(0.0, -1.0, 0.0);\n\t\tbreak;\n\tcase ECubusDensityFace::PositiveZ:\n\t\tResult.BoundaryOrigin = FVector(0.0, 0.0, Cubus::ChunkSize);\n\t\tResult.Inward = FVector(0.0, 0.0, -1.0);\n\t\tResult.U = FVector(1.0, 0.0, 0.0);\n\t\tResult.V = FVector(0.0, 1.0, 0.0);\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n\t}\n\n\tcheckSlow(FVector::DotProduct(FVector::CrossProduct(Result.U, Result.V), -Result.Inward) > 0.99);\n\treturn Result;\n}'''
)

# Guards.
checks = {
    LOD_H: ['FCubusDensityTileBounds2D', 'BuildAlignedCoverage', 'BuildAlignedOuterBounds', 'HorizontalChunkDistance'],
    WORLD_H: ['DensitySurfaceVerticalPaddingChunks', 'DensityStreamingCoverageBounds', 'ActiveDensityLodTransitionCoordinates', 'BuildDensitySurfaceRequiredCoordinates'],
    WORLD_CPP: ['BuildDensitySurfaceRequiredCoordinates(', 'AreRequiredStreamingChunksReady()', 'TryCommitDensityLodTransition()', 'StagedDensityLodTransitionCoordinates.Contains', 'bReplacementCoverageReady'],
    MESHER_CPP: ['FVector(0.0, 0.0, Cubus::ChunkSize)', 'FVector(0.0, Cubus::ChunkSize, Cubus::ChunkSize)', 'checkSlow(FVector::DotProduct'],
}
for path, tokens in checks.items():
    text = read(path)
    for token in tokens:
        if token not in text:
            raise RuntimeError(f'{path}: missing guard token {token!r}')
