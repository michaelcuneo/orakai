from pathlib import Path
import re

P = Path('Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp')
H = Path('Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.h')
s = P.read_text(encoding='utf-8')

inc = '#include "CubusCore/Generation/CubusTerrainForm.h"\n'
geo = '#include "CubusCore/Data/CubusGeologyProfile.h"\n'
if geo not in s:
    if s.count(inc) != 1:
        raise RuntimeError('terrain form include anchor mismatch')
    s = s.replace(inc, inc + geo, 1)

pattern = r'''void ACubusBlockWorldActor::BuildDensitySurfaceRequiredCoordinates\(.*?\n\}\n\nbool ACubusBlockWorldActor::AreRequiredStreamingChunksReady'''
replacement = '''void ACubusBlockWorldActor::BuildDensitySurfaceRequiredCoordinates(
\tconst FCubusDensityTileBounds2D& CoverageBounds,
\tconst FCubusTerrainFormSettings& TerrainSettings,
\tconst int32 TerrainOffsetX,
\tconst int32 TerrainOffsetY,
\tconst int32 VerticalPadding,
\tTSet<FIntVector>& OutCoordinates
) const
{
\tOutCoordinates.Reset();
\tif (!CoverageBounds.IsValid())
\t{
\t\treturn;
\t}

\tconst int32 SafePadding = FMath::Clamp(VerticalPadding, 0, 2);
\tconstexpr int32 SurfaceSamplesPerAxis = 3;
\tconst float ChunkSizeVoxels = static_cast<float>(Cubus::ChunkSize);

\t/*
\t * The fine surface and bounded geology can move the scalar zero by less
\t * than about two canonical voxels. River lowering is the only larger
\t * downward displacement in the current density biome, so reserve exactly
\t * its configured depth rather than loading whole empty chunks above/below
\t * every XY column.
\t */
\tconst float RiverDownwardSafety = IsValid(GeologyProfile) && GeologyProfile->bGenerateRivers
\t\t? FMath::Max(0.0f, GeologyProfile->RiverValleyDepth) +
\t\t  static_cast<float>(FMath::Max(0, GeologyProfile->RiverChannelDepth))
\t\t: 0.0f;
\tconst float GeneralSurfaceSafety = 2.0f + static_cast<float>(SafePadding) * 2.0f;
\tconst float DownwardSafety = GeneralSurfaceSafety + RiverDownwardSafety;
\tconst float UpwardSafety = GeneralSurfaceSafety;

\tfor (int32 ChunkY = CoverageBounds.Min.Y; ChunkY < CoverageBounds.MaxExclusive.Y; ++ChunkY)
\t{
\t\tfor (int32 ChunkX = CoverageBounds.Min.X; ChunkX < CoverageBounds.MaxExclusive.X; ++ChunkX)
\t\t{
\t\t\tfloat MinimumSurfaceVoxelZ = MAX_flt;
\t\t\tfloat MaximumSurfaceVoxelZ = -MAX_flt;
\t\t\tfor (int32 SampleY = 0; SampleY < SurfaceSamplesPerAxis; ++SampleY)
\t\t\t{
\t\t\t\tfor (int32 SampleX = 0; SampleX < SurfaceSamplesPerAxis; ++SampleX)
\t\t\t\t{
\t\t\t\t\tconst float AlphaX = static_cast<float>(SampleX) / static_cast<float>(SurfaceSamplesPerAxis - 1);
\t\t\t\t\tconst float AlphaY = static_cast<float>(SampleY) / static_cast<float>(SurfaceSamplesPerAxis - 1);
\t\t\t\t\tconst float SurfaceSampleX =
\t\t\t\t\t\t(static_cast<float>(ChunkX) + AlphaX) * ChunkSizeVoxels + static_cast<float>(TerrainOffsetX);
\t\t\t\t\tconst float SurfaceSampleY =
\t\t\t\t\t\t(static_cast<float>(ChunkY) + AlphaY) * ChunkSizeVoxels + static_cast<float>(TerrainOffsetY);
\t\t\t\t\tconst float SurfaceVoxelZ = bUseHeightTerrain
\t\t\t\t\t\t? FCubusTerrainForm::Sample(SurfaceSampleX, SurfaceSampleY, TerrainSettings).Height
\t\t\t\t\t\t: static_cast<float>(TerrainSurfaceWorldZ);
\t\t\t\t\tMinimumSurfaceVoxelZ = FMath::Min(MinimumSurfaceVoxelZ, SurfaceVoxelZ);
\t\t\t\t\tMaximumSurfaceVoxelZ = FMath::Max(MaximumSurfaceVoxelZ, SurfaceVoxelZ);
\t\t\t\t}
\t\t\t}

\t\t\tconst int32 MinimumChunkZ = FMath::FloorToInt(
\t\t\t\t(MinimumSurfaceVoxelZ - DownwardSafety) / ChunkSizeVoxels
\t\t\t);
\t\t\tconst int32 MaximumChunkZ = FMath::FloorToInt(
\t\t\t\t(MaximumSurfaceVoxelZ + UpwardSafety) / ChunkSizeVoxels
\t\t\t);
\t\t\tfor (int32 ChunkZ = MinimumChunkZ; ChunkZ <= MaximumChunkZ; ++ChunkZ)
\t\t\t{
\t\t\t\tOutCoordinates.Add(FIntVector(ChunkX, ChunkY, ChunkZ));
\t\t\t}
\t\t}
\t}
}

bool ACubusBlockWorldActor::AreRequiredStreamingChunksReady'''
out, n = re.subn(pattern, replacement, s, count=1, flags=re.S)
if n != 1:
    raise RuntimeError(f'surface coordinate helper match count {n}')
s = out

old = '''\tTSet<FIntVector> DesiredRequiredCoordinates;\n\tFCubusDensityTileBounds2D DesiredCoverageBounds;\n\n\tif (bDensityWorld)\n\t{\n\t\tDesiredCoverageBounds = FCubusDensityLod::BuildAlignedCoverage(\n\t\t\tFIntPoint(CentreCoordinate.X, CentreCoordinate.Y),\n\t\t\tFMath::Max(1, HorizontalRadius),\n\t\t\t2\n\t\t);\n\t\tBuildDensitySurfaceRequiredCoordinates(\n\t\t\tDesiredCoverageBounds,\n\t\t\tStreamingTerrainSettings,\n\t\t\tTerrainOffsetX,\n\t\t\tTerrainOffsetY,\n\t\t\tFMath::Min(VerticalRadius, DensitySurfaceVerticalPaddingChunks),\n\t\t\tDesiredRequiredCoordinates\n\t\t);\n\t}\n\telse\n\t{\n\t\tBuildRequiredCoordinates(\n\t\t\tCentreCoordinate, HorizontalRadius, VerticalRadius, DesiredRequiredCoordinates\n\t\t);\n\t}\n\n\tconst bool bCentreChanged = CentreCoordinate != LastTrackedChunk;\n\tconst bool bCoverageChanged = bDensityWorld\n\t\t? DesiredCoverageBounds != DensityStreamingCoverageBounds\n\t\t: DensityStreamingCoverageBounds.IsValid();\n\n\tif (!bForce && !bCentreChanged && !bCoverageChanged)\n\t{\n\t\treturn;\n\t}\n\n\tLastTrackedChunk = CentreCoordinate;\n'''
new = '''\tTSet<FIntVector> DesiredRequiredCoordinates;
\tFCubusDensityTileBounds2D DesiredCoverageBounds;
\tif (bDensityWorld)
\t{
\t\tDesiredCoverageBounds = FCubusDensityLod::BuildAlignedCoverage(
\t\t\tFIntPoint(CentreCoordinate.X, CentreCoordinate.Y),
\t\t\tFMath::Max(1, HorizontalRadius),
\t\t\t2
\t\t);
\t}

\tconst bool bCentreChanged = CentreCoordinate != LastTrackedChunk;
\tconst bool bCoverageChanged = bDensityWorld
\t\t? DesiredCoverageBounds != DensityStreamingCoverageBounds
\t\t: DensityStreamingCoverageBounds.IsValid();

\t/*
\t * Keep the tracking coordinate current for spawn/priority work, but density
\t * streaming itself moves only when its aligned 2-chunk clipmap moves. This
\t * removes the full surface prepass and queue rebuild on every single chunk
\t * crossed by the player.
\t */
\tLastTrackedChunk = CentreCoordinate;
\tconst bool bStreamingWindowChanged = bDensityWorld ? bCoverageChanged : (bCentreChanged || bCoverageChanged);
\tif (!bForce && !bStreamingWindowChanged)
\t{
\t\treturn;
\t}

\tif (bDensityWorld)
\t{
\t\tBuildDensitySurfaceRequiredCoordinates(
\t\t\tDesiredCoverageBounds,
\t\t\tStreamingTerrainSettings,
\t\t\tTerrainOffsetX,
\t\t\tTerrainOffsetY,
\t\t\tFMath::Min(VerticalRadius, DensitySurfaceVerticalPaddingChunks),
\t\t\tDesiredRequiredCoordinates
\t\t);
\t}
\telse
\t{
\t\tBuildRequiredCoordinates(
\t\t\tCentreCoordinate, HorizontalRadius, VerticalRadius, DesiredRequiredCoordinates
\t\t);
\t}
'''
if s.count(old) != 1:
    raise RuntimeError(f'window prepass anchor count {s.count(old)}')
s = s.replace(old, new, 1)

P.write_text(s, encoding='utf-8')

h = H.read_text(encoding='utf-8')
h = h.replace(
'''\t * Density terrain follows the generated surface per XY column instead of\n\t * loading a tall ellipsoid around one Z anchor. One chunk of padding above\n\t * and below the surface is enough for the bounded geology band and removes\n\t * large amounts of empty/deep density work.\n''',
'''\t * Density terrain follows the generated surface per XY column instead of
\t * loading a tall ellipsoid around one Z anchor. This value adds a small
\t * voxel-space safety allowance to the sampled per-column height range; it
\t * no longer forces whole empty chunks above and below every column.
''')
H.write_text(h, encoding='utf-8')

check = P.read_text(encoding='utf-8')
for token in ['SurfaceSamplesPerAxis = 3', 'RiverDownwardSafety', 'bStreamingWindowChanged', 'BuildDensitySurfaceRequiredCoordinates(']:
    if token not in check:
        raise RuntimeError(f'missing guard {token}')
