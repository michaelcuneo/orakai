from pathlib import Path
from urllib.request import urlopen
import re

ROOT = Path('.')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def write(path, text):
    (ROOT / path).write_text(text, encoding='utf-8')


def replace_once(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected exactly one anchor, got {count}: {old[:160]!r}')
    write(path, text.replace(old, new, 1))


def regex_replace_once(path, pattern, replacement, flags=re.S):
    text = read(path)
    new_text, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f'{path}: regex anchor count={count}: {pattern[:160]!r}')
    write(path, new_text)


# ---------------------------------------------------------------------------
# Vendor the official MIT-licensed Transvoxel transition tables, pinned to a
# known upstream commit. Only transition tables are retained; Orakai continues
# using its existing Marching Cubes tables for regular cells.
# ---------------------------------------------------------------------------
UPSTREAM_COMMIT = '51a494f03c5b024cd153b596bcc7152eb3cc93a6'
UPSTREAM_URL = f'https://raw.githubusercontent.com/EricLengyel/Transvoxel/{UPSTREAM_COMMIT}/Transvoxel.cpp'
source = urlopen(UPSTREAM_URL, timeout=30).read().decode('utf-8')


def extract_array(declaration):
    start = source.index(declaration)
    end = source.index('\n};', start) + len('\n};')
    return source[start:end]

cell_class = extract_array('const unsigned char transitionCellClass[512]')
cell_data = extract_array('const TransitionCellData transitionCellData[56]')
vertex_data = extract_array('const unsigned short transitionVertexData[512][12]')

cell_class = cell_class.replace(
    'const unsigned char transitionCellClass[512]',
    'inline constexpr uint8 TransitionCellClass[512]'
)
cell_data = cell_data.replace(
    'const TransitionCellData transitionCellData[56]',
    'inline constexpr FTransitionCellData TransitionCellData[56]'
)
vertex_data = vertex_data.replace(
    'const unsigned short transitionVertexData[512][12]',
    'inline constexpr uint16 TransitionVertexData[512][12]'
)

license_text = '''MIT License

Copyright (c) 2009 Eric Lengyel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.'''

table_header = f'''#pragma once

#include "CoreMinimal.h"

/*
 * Transvoxel transition-cell lookup tables.
 *
 * Upstream: https://github.com/EricLengyel/Transvoxel
 * Pinned source commit: {UPSTREAM_COMMIT}
 *
 * {license_text.replace(chr(10), chr(10) + ' * ')}
 */
namespace CubusTransvoxelTables
{{
    struct FTransitionCellData
    {{
        uint8 GeometryCounts = 0;
        uint8 VertexIndex[36] = {{}};

        constexpr int32 GetVertexCount() const
        {{
            return GeometryCounts >> 4;
        }}

        constexpr int32 GetTriangleCount() const
        {{
            return GeometryCounts & 0x0F;
        }}
    }};

{cell_class}

{cell_data}

{vertex_data}

    static_assert(UE_ARRAY_COUNT(TransitionCellClass) == 512, "Transvoxel transition class table must contain 512 cases.");
    static_assert(UE_ARRAY_COUNT(TransitionCellData) == 56, "Transvoxel transition data must contain 56 triangulation classes.");
    static_assert(UE_ARRAY_COUNT(TransitionVertexData) == 512, "Transvoxel transition vertex table must contain 512 cases.");
}}
'''
write('Source/Orakai/CubusCore/Meshing/CubusTransvoxelTables.h', table_header)

# ---------------------------------------------------------------------------
# LOD face contract shared by streaming and meshing.
# ---------------------------------------------------------------------------
lod_path = 'Source/Orakai/CubusCore/Meshing/CubusDensityLod.h'
replace_once(
    lod_path,
    '''/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
''',
    '''enum class ECubusDensityFace : uint8
{
    NegativeX = 0,
    PositiveX,
    NegativeY,
    PositiveY,
    NegativeZ,
    PositiveZ,
    Count
};

/**
 * Snapshot of the target density resolution on all six neighbouring faces.
 * A transition cell is owned by the coarser chunk whenever a face neighbour
 * has exactly twice its subdivision count.
 */
struct ORAKAI_API FCubusDensityTransitionFaces
{
    int32 NeighbourSubdivisions[6] = { 0, 0, 0, 0, 0, 0 };

    int32 Get(const ECubusDensityFace Face) const
    {
        return NeighbourSubdivisions[static_cast<int32>(Face)];
    }

    void Set(const ECubusDensityFace Face, const int32 Subdivisions)
    {
        NeighbourSubdivisions[static_cast<int32>(Face)] = Subdivisions;
    }

    bool HasFinerNeighbour(const ECubusDensityFace Face, const int32 SelfSubdivisions) const
    {
        return Get(Face) == SelfSubdivisions * 2;
    }

    uint32 GetSignature(const int32 SelfSubdivisions) const
    {
        uint32 Hash = 2166136261u;
        auto Mix = [&Hash](const uint32 Value)
        {
            Hash ^= Value;
            Hash *= 16777619u;
        };

        Mix(static_cast<uint32>(SelfSubdivisions));
        for (const int32 Value : NeighbourSubdivisions)
        {
            Mix(static_cast<uint32>(Value));
        }
        return Hash;
    }

    static FIntVector GetOffset(const ECubusDensityFace Face)
    {
        switch (Face)
        {
        case ECubusDensityFace::NegativeX: return FIntVector(-1, 0, 0);
        case ECubusDensityFace::PositiveX: return FIntVector(1, 0, 0);
        case ECubusDensityFace::NegativeY: return FIntVector(0, -1, 0);
        case ECubusDensityFace::PositiveY: return FIntVector(0, 1, 0);
        case ECubusDensityFace::NegativeZ: return FIntVector(0, 0, -1);
        case ECubusDensityFace::PositiveZ: return FIntVector(0, 0, 1);
        default: return FIntVector::ZeroValue;
        }
    }
};

/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
'''
)

# ---------------------------------------------------------------------------
# Mesher API carries the neighbour topology contract.
# ---------------------------------------------------------------------------
mesher_h = 'Source/Orakai/CubusCore/Meshing/CubusDensityMesher.h'
replace_once(
    mesher_h,
    '#include "CubusCore/Meshing/CubusMeshData.h"\n',
    '#include "CubusCore/Meshing/CubusMeshData.h"\n#include "CubusCore/Meshing/CubusDensityLod.h"\n'
)
replace_once(
    mesher_h,
    '''        int32& OutGeneratedTriangleCount,
        const ICubusDensityField* SurfaceMaterialField = nullptr
    );''',
    '''        int32& OutGeneratedTriangleCount,
        const ICubusDensityField* SurfaceMaterialField = nullptr,
        const ICubusDensityField* TransitionField = nullptr,
        const FCubusDensityTransitionFaces& TransitionFaces = FCubusDensityTransitionFaces()
    );'''
)
replace_once(
    mesher_h,
    '''        float IsoLevel,
        TMap<int32, FCubusMeshData>& OutMaterialMeshes,
        int32& OutGeneratedTriangleCount
    );''',
    '''        float IsoLevel,
        TMap<int32, FCubusMeshData>& OutMaterialMeshes,
        int32& OutGeneratedTriangleCount,
        const FCubusDensityTransitionFaces& TransitionFaces = FCubusDensityTransitionFaces()
    );'''
)

# ---------------------------------------------------------------------------
# Build inputs carry face topology; async jobs can therefore reject stale
# results when either their own LOD or a neighbour LOD changes.
# ---------------------------------------------------------------------------
voxel_h = 'Source/Orakai/CubusCore/Actors/CubusVoxelVolumeActor.h'
replace_once(
    voxel_h,
    '''\tfloat VoxelSize\t\t\t   = 100.0f;
\tint32 SubdivisionsPerVoxel = 1;
\tfloat IsoLevel\t\t\t   = 0.0f;
''',
    '''\tfloat VoxelSize\t\t\t   = 100.0f;
\tint32 SubdivisionsPerVoxel = 1;
\tFCubusDensityTransitionFaces TransitionFaces;
\tfloat IsoLevel\t\t\t   = 0.0f;
'''
)

world_h = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.h'
replace_once(
    world_h,
    '''\t// Resolution captured by this worker. If LOD changes while the task is
\t// running, the result is discarded instead of publishing stale geometry.
\tint32 SubdivisionsPerVoxel = 1;
};''',
    '''\t// Resolution and six-face topology captured by this worker. If either
\t// changes while the task runs, the result is discarded rather than
\t// publishing a mesh with an obsolete seam contract.
\tint32 SubdivisionsPerVoxel = 1;
\tuint32 TransitionSignature = 0;
};'''
)
replace_once(
    world_h,
    '''\t/** Canonical LOD0 voxel size used by world-space systems such as vegetation. */
\tfloat GetGeneratedVoxelSize() const { return GeneratedVoxelSize; }
''',
    '''\t/** Canonical LOD0 voxel size used by world-space systems such as vegetation. */
\tfloat GetGeneratedVoxelSize() const { return GeneratedVoxelSize; }

\tFCubusDensityTransitionFaces BuildDensityTransitionFaces(
\t\tconst FIntVector& ChunkCoordinate,
\t\tint32 SelfSubdivisions
\t) const;
'''
)

# ---------------------------------------------------------------------------
# World streaming: enforce only 2:1 neighbours, rebuild seam dependencies, and
# reject async meshes whose neighbour topology changed while building.
# ---------------------------------------------------------------------------
world_cpp = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
regex_replace_once(
    world_cpp,
    r'int32 ACubusBlockWorldActor::ResolveDensitySubdivisions\(const FIntVector& ChunkCoordinate\) const\n\{.*?\n\}\n\nvoid ACubusBlockWorldActor::UpdateDensityLods\(\)',
    '''int32 ACubusBlockWorldActor::ResolveDensitySubdivisions(const FIntVector& ChunkCoordinate) const
{
\tif (!bEnableDensityLod)
\t{
\t\treturn 1;
\t}

\tconst bool bHasTrackedChunk =
\t\tLastTrackedChunk.X != MAX_int32 &&
\t\tLastTrackedChunk.Y != MAX_int32 &&
\t\tLastTrackedChunk.Z != MAX_int32;

\tfloat TargetSpacing = DensityFarSampleSpacing;
\tint32 Distance = MAX_int32;

\tif (bHasTrackedChunk)
\t{
\t\tDistance = FCubusDensityLod::ChunkDistance(ChunkCoordinate, LastTrackedChunk);
\t\tif (Distance <= DensityNearChunkRadius)
\t\t{
\t\t\tTargetSpacing = DensityNearSampleSpacing;
\t\t}
\t\telse if (Distance <= DensityMiddleChunkRadius)
\t\t{
\t\t\tTargetSpacing = DensityMiddleSampleSpacing;
\t\t}
\t}

\tint32 Resolved = FCubusDensityLod::ResolveSubdivisionsForSpacing(
\t\tGeneratedVoxelSize,
\t\tTargetSpacing
\t);

\t/*
\t * The support chunk intentionally starts at canonical resolution for fast
\t * collision. Its immediate ring is capped at 2x during bootstrap so no
\t * temporary 1x<->4x face can ever exist. After release the normal 4/2/1
\t * rings are restored.
\t */
\tif (bPawnHeldForStreaming && bHasTrackedChunk)
\t{
\t\tif (Distance == 0)
\t\t{
\t\t\treturn 1;
\t\t}
\t\tif (Distance == 1)
\t\t{
\t\t\tResolved = FMath::Min(Resolved, 2);
\t\t}
\t}

\treturn Resolved;
}

FCubusDensityTransitionFaces ACubusBlockWorldActor::BuildDensityTransitionFaces(
\tconst FIntVector& ChunkCoordinate,
\tconst int32 SelfSubdivisions
) const
{
\tFCubusDensityTransitionFaces Result;
\tfor (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
\t{
\t\tconst ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
\t\tconst FIntVector NeighbourCoordinate =
\t\t\tChunkCoordinate + FCubusDensityTransitionFaces::GetOffset(Face);
\t\tconst int32 NeighbourSubdivisions = ResolveDensitySubdivisions(NeighbourCoordinate);

\t\t/* Adjacent streamed tiers must differ by no more than one power-of-two step. */
\t\tensureMsgf(
\t\t\tNeighbourSubdivisions <= SelfSubdivisions * 2 || SelfSubdivisions <= NeighbourSubdivisions * 2,
\t\t\tTEXT("Unsupported Cubus density LOD jump %dx <-> %dx at (%d,%d,%d) face %d"),
\t\t\tSelfSubdivisions,
\t\t\tNeighbourSubdivisions,
\t\t\tChunkCoordinate.X,
\t\t\tChunkCoordinate.Y,
\t\t\tChunkCoordinate.Z,
\t\t\tFaceIndex
\t\t);
\t\tResult.Set(Face, NeighbourSubdivisions);
\t}
\treturn Result;
}

void ACubusBlockWorldActor::UpdateDensityLods()'''
)

regex_replace_once(
    world_cpp,
    r'void ACubusBlockWorldActor::UpdateDensityLods\(\)\n\{.*?\n\}\n\nvoid ACubusBlockWorldActor::QueueStreamingChunkBuilds\(\)',
    '''void ACubusBlockWorldActor::UpdateDensityLods()
{
\tconst ECubusVoxelRenderMode RenderMode = GetVoxelRenderMode();
\tif (RenderMode != ECubusVoxelRenderMode::Density && RenderMode != ECubusVoxelRenderMode::Hybrid)
\t{
\t\treturn;
\t}

\tTArray<FIntVector> ChangedCoordinates;
\tfor (const auto& Entry : ChunksByCoordinate)
\t{
\t\tACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();
\t\tif (!IsValid(ChunkActor))
\t\t{
\t\t\tcontinue;
\t\t}

\t\tif (ChunkActor->ConfigureDensityResolution(ResolveDensitySubdivisions(Entry.Key)))
\t\t{
\t\t\tChangedCoordinates.Add(Entry.Key);
\t\t}
\t}

\tfor (const FIntVector& ChangedCoordinate : ChangedCoordinates)
\t{
\t\tauto InvalidateCoordinate = [this](const FIntVector& Coordinate)
\t\t{
\t\t\tACubusVoxelVolumeActor* Chunk = FindChunk(Coordinate);
\t\t\tif (!IsValid(Chunk))
\t\t\t{
\t\t\t\treturn;
\t\t\t}

\t\t\tif (bEnableRuntimeStreaming && Chunk->GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)
\t\t\t{
\t\t\t\tStreamingChunksReady.Remove(Coordinate);
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tQueueChunkForRebuild(Coordinate);
\t\t\t}
\t\t};

\t\t/*
\t\t * A face neighbour can keep the same own LOD while gaining/losing a
\t\t * transition face. Rebuild both sides whenever one resolution changes.
\t\t */
\t\tInvalidateCoordinate(ChangedCoordinate);
\t\tfor (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
\t\t{
\t\t\tconst ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
\t\t\tInvalidateCoordinate(
\t\t\t\tChangedCoordinate + FCubusDensityTransitionFaces::GetOffset(Face)
\t\t\t);
\t\t}
\t}
}

void ACubusBlockWorldActor::QueueStreamingChunkBuilds()'''
)

replace_once(
    world_cpp,
    '''\t\tBuild.Coordinate = Coordinate;
\t\tBuild.Chunk\t\t = Chunk;
\t\tBuild.SubdivisionsPerVoxel = BuildInput.SubdivisionsPerVoxel;
''',
    '''\t\tBuild.Coordinate = Coordinate;
\t\tBuild.Chunk\t\t = Chunk;
\t\tBuild.SubdivisionsPerVoxel = BuildInput.SubdivisionsPerVoxel;
\t\tBuild.TransitionSignature = BuildInput.TransitionFaces.GetSignature(BuildInput.SubdivisionsPerVoxel);
'''
)

replace_once(
    world_cpp,
    '''\t\tconst bool bResolutionStillCurrent =
\t\t\tIsValid(Chunk) &&
\t\t\tChunk->GetDensitySubdivisionsPerVoxel() == Build.SubdivisionsPerVoxel;

\t\tif (IsValid(Chunk) && bStillRequired && bResolutionStillCurrent)
''',
    '''\t\tconst bool bResolutionStillCurrent =
\t\t\tIsValid(Chunk) &&
\t\t\tChunk->GetDensitySubdivisionsPerVoxel() == Build.SubdivisionsPerVoxel;
\t\tconst FCubusDensityTransitionFaces CurrentTransitionFaces =
\t\t\tbResolutionStillCurrent
\t\t\t\t? BuildDensityTransitionFaces(Build.Coordinate, Build.SubdivisionsPerVoxel)
\t\t\t\t: FCubusDensityTransitionFaces();
\t\tconst bool bTopologyStillCurrent =
\t\t\tbResolutionStillCurrent &&
\t\t\tCurrentTransitionFaces.GetSignature(Build.SubdivisionsPerVoxel) == Build.TransitionSignature;

\t\tif (IsValid(Chunk) && bStillRequired && bTopologyStillCurrent)
'''
)

# ---------------------------------------------------------------------------
# Chunk build input gets topology from the owning world and passes it to both
# canonical and adaptive meshing paths.
# ---------------------------------------------------------------------------
voxel_cpp = 'Source/Orakai/CubusCore/Actors/CubusVoxelVolumeActor.cpp'
replace_once(
    voxel_cpp,
    '''\tInput.SubdivisionsPerVoxel = DensitySubdivisionsPerVoxel;

\tInput.IsoLevel = 0.0f;
''',
    '''\tInput.SubdivisionsPerVoxel = DensitySubdivisionsPerVoxel;

\tif (IsValid(OwningBlockWorld.Get()))
\t{
\t\tInput.TransitionFaces = OwningBlockWorld->BuildDensityTransitionFaces(
\t\t\tChunkCoordinate,
\t\t\tInput.SubdivisionsPerVoxel
\t\t);
\t}

\tInput.IsoLevel = 0.0f;
'''
)

replace_once(
    voxel_cpp,
    '''\tconst int32 Subdivisions = FCubusDensityLod::NormalizeSubdivisions(Input.SubdivisionsPerVoxel);

\tif (Subdivisions <= 1)
''',
    '''\tconst int32 Subdivisions = FCubusDensityLod::NormalizeSubdivisions(Input.SubdivisionsPerVoxel);
\tconst FCubusDensityEditField EditedDensityField(DensityField, Input.DensityEdits);

\tif (Subdivisions <= 1)
'''
)
replace_once(
    voxel_cpp,
    '''\t\tFCubusDensityMesher::BuildChunk(DensityBuffer, Input.VoxelSize, Input.IsoLevel, Result.MaterialMeshes,
\t\t\t\t\t\t\t\t\t\tResult.GeneratedTriangleCount);
\t}
\telse
\t{
\t\t/*
\t\t * Fine adaptive density still needs continuous edit interpolation.
\t\t *
\t\t * Keep the existing path intact for when density subdivision is
\t\t * re-enabled later.
\t\t */
\t\tconst FCubusDensityEditField EditedDensityField(DensityField, Input.DensityEdits);

\t\tFCubusDensityMesher::BuildAdaptiveChunk(EditedDensityField, Input.ChunkCoordinate, Input.VoxelSize, Subdivisions, Input.IsoLevel,
\t\t\t\t\t\t\t\t\t\t\t\tResult.MaterialMeshes, Result.GeneratedTriangleCount);
''',
    '''\t\tFCubusDensityMesher::BuildChunk(
\t\t\tDensityBuffer,
\t\t\tInput.VoxelSize,
\t\t\tInput.IsoLevel,
\t\t\tResult.MaterialMeshes,
\t\t\tResult.GeneratedTriangleCount,
\t\t\tnullptr,
\t\t\t&EditedDensityField,
\t\t\tInput.TransitionFaces
\t\t);
\t}
\telse
\t{
\t\tFCubusDensityMesher::BuildAdaptiveChunk(
\t\t\tEditedDensityField,
\t\t\tInput.ChunkCoordinate,
\t\t\tInput.VoxelSize,
\t\t\tSubdivisions,
\t\t\tInput.IsoLevel,
\t\t\tResult.MaterialMeshes,
\t\t\tResult.GeneratedTriangleCount,
\t\t\tInput.TransitionFaces
\t\t);
'''
)

# ---------------------------------------------------------------------------
# Mesher core: consistent field samples, regular-cell deformation on coarser
# transition faces, and true Transvoxel transition cells.
# ---------------------------------------------------------------------------
mesher_cpp = 'Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp'
replace_once(
    mesher_cpp,
    '#include "CubusCore/Meshing/CubusMarchingCubesTables.h"\n',
    '#include "CubusCore/Meshing/CubusMarchingCubesTables.h"\n#include "CubusCore/Meshing/CubusTransvoxelTables.h"\n'
)

# Insert shared transition helpers before the ordinary edge interpolation.
replace_once(
    mesher_cpp,
    '''    FInterpolatedVertex InterpolateEdge(
''',
    '''    FCubusDensitySample SampleConsistentField(
        const ICubusDensityField& DensityField,
        const FVector& GlobalCoordinate
    )
    {
        const FIntVector Rounded(
            FMath::RoundToInt(GlobalCoordinate.X),
            FMath::RoundToInt(GlobalCoordinate.Y),
            FMath::RoundToInt(GlobalCoordinate.Z)
        );
        const bool bCanonicalLatticePoint =
            FMath::IsNearlyEqual(GlobalCoordinate.X, static_cast<double>(Rounded.X), 1.0e-6) &&
            FMath::IsNearlyEqual(GlobalCoordinate.Y, static_cast<double>(Rounded.Y), 1.0e-6) &&
            FMath::IsNearlyEqual(GlobalCoordinate.Z, static_cast<double>(Rounded.Z), 1.0e-6);

        return bCanonicalLatticePoint
            ? DensityField.Sample(Rounded)
            : DensityField.SampleContinuous(GlobalCoordinate);
    }

    struct FTransitionFaceBasis
    {
        FVector BoundaryOrigin = FVector::ZeroVector;
        FVector Inward = FVector::ZeroVector;
        FVector U = FVector::ZeroVector;
        FVector V = FVector::ZeroVector;
    };

    FTransitionFaceBasis GetTransitionFaceBasis(const ECubusDensityFace Face)
    {
        FTransitionFaceBasis Result;
        switch (Face)
        {
        case ECubusDensityFace::NegativeX:
            Result.BoundaryOrigin = FVector(0.0, 0.0, 0.0);
            Result.Inward = FVector(1.0, 0.0, 0.0);
            Result.U = FVector(0.0, 1.0, 0.0);
            Result.V = FVector(0.0, 0.0, 1.0);
            break;
        case ECubusDensityFace::PositiveX:
            Result.BoundaryOrigin = FVector(Cubus::ChunkSize, 0.0, 0.0);
            Result.Inward = FVector(-1.0, 0.0, 0.0);
            Result.U = FVector(0.0, 1.0, 0.0);
            Result.V = FVector(0.0, 0.0, 1.0);
            break;
        case ECubusDensityFace::NegativeY:
            Result.BoundaryOrigin = FVector(0.0, 0.0, 0.0);
            Result.Inward = FVector(0.0, 1.0, 0.0);
            Result.U = FVector(1.0, 0.0, 0.0);
            Result.V = FVector(0.0, 0.0, 1.0);
            break;
        case ECubusDensityFace::PositiveY:
            Result.BoundaryOrigin = FVector(0.0, Cubus::ChunkSize, 0.0);
            Result.Inward = FVector(0.0, -1.0, 0.0);
            Result.U = FVector(1.0, 0.0, 0.0);
            Result.V = FVector(0.0, 0.0, 1.0);
            break;
        case ECubusDensityFace::NegativeZ:
            Result.BoundaryOrigin = FVector(0.0, 0.0, 0.0);
            Result.Inward = FVector(0.0, 0.0, 1.0);
            Result.U = FVector(1.0, 0.0, 0.0);
            Result.V = FVector(0.0, 1.0, 0.0);
            break;
        case ECubusDensityFace::PositiveZ:
            Result.BoundaryOrigin = FVector(0.0, 0.0, Cubus::ChunkSize);
            Result.Inward = FVector(0.0, 0.0, -1.0);
            Result.U = FVector(1.0, 0.0, 0.0);
            Result.V = FVector(0.0, 1.0, 0.0);
            break;
        default:
            break;
        }
        return Result;
    }

    FVector ApplyTransitionTransform(
        const FVector& LocalSamplePosition,
        const int32 SelfSubdivisions,
        const FCubusDensityTransitionFaces& TransitionFaces
    )
    {
        FVector Result = LocalSamplePosition;
        const int32 SafeSubdivisions = FCubusDensityLod::NormalizeSubdivisions(SelfSubdivisions);
        const double CoarseSpacing = 1.0 / static_cast<double>(SafeSubdivisions);
        const double TransitionThickness = CoarseSpacing * 0.5;

        for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
        {
            const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
            if (!TransitionFaces.HasFinerNeighbour(Face, SafeSubdivisions))
            {
                continue;
            }

            const FTransitionFaceBasis Basis = GetTransitionFaceBasis(Face);
            const double DistanceFromBoundary = FVector::DotProduct(
                LocalSamplePosition - Basis.BoundaryOrigin,
                Basis.Inward
            );
            if (DistanceFromBoundary < -UE_KINDA_SMALL_NUMBER || DistanceFromBoundary >= CoarseSpacing)
            {
                continue;
            }

            const double Weight = 1.0 - FMath::Clamp(DistanceFromBoundary / CoarseSpacing, 0.0, 1.0);
            Result += Basis.Inward * (TransitionThickness * Weight);
        }
        return Result;
    }

    FVector SampleFieldGradient(
        const ICubusDensityField& DensityField,
        const FVector& GlobalCoordinate,
        const float Step
    )
    {
        const float SafeStep = FMath::Max(Step, 0.0001f);
        const float NegativeX = SampleConsistentField(DensityField, GlobalCoordinate - FVector(SafeStep, 0.0, 0.0)).Density;
        const float PositiveX = SampleConsistentField(DensityField, GlobalCoordinate + FVector(SafeStep, 0.0, 0.0)).Density;
        const float NegativeY = SampleConsistentField(DensityField, GlobalCoordinate - FVector(0.0, SafeStep, 0.0)).Density;
        const float PositiveY = SampleConsistentField(DensityField, GlobalCoordinate + FVector(0.0, SafeStep, 0.0)).Density;
        const float NegativeZ = SampleConsistentField(DensityField, GlobalCoordinate - FVector(0.0, 0.0, SafeStep)).Density;
        const float PositiveZ = SampleConsistentField(DensityField, GlobalCoordinate + FVector(0.0, 0.0, SafeStep)).Density;
        return FVector(
            PositiveX - NegativeX,
            PositiveY - NegativeY,
            PositiveZ - NegativeZ
        ) / (2.0f * SafeStep);
    }

    FInterpolatedVertex InterpolateEdge(
'''
)

# Extend ordinary edge interpolation with the transition transform.
replace_once(
    mesher_cpp,
    '''        const FVector& ChunkMinimum,
        const float VoxelSize,
        const float IsoLevel
    )
''',
    '''        const FVector& ChunkMinimum,
        const float VoxelSize,
        const float IsoLevel,
        const int32 SelfSubdivisions,
        const FCubusDensityTransitionFaces& TransitionFaces
    )
'''
)
replace_once(
    mesher_cpp,
    '''        Result.LocalPosition =
            ChunkMinimum +
            LocalSamplePosition *
            VoxelSize;
''',
    '''        Result.LocalPosition =
            ChunkMinimum +
            ApplyTransitionTransform(
                LocalSamplePosition,
                SelfSubdivisions,
                TransitionFaces
            ) * VoxelSize;
'''
)

# Replace adaptive boundary sampling: same-LOD boundaries now retain true fine
# detail, while exact canonical lattice points remain identical across 1/2/4x.
regex_replace_once(
    mesher_cpp,
    r'''            const FVector GlobalCoordinate =\n                GetGlobalCoordinate\(FineCoordinate\);\n\n            const bool bOnChunkBoundary =.*?\n                \);\n\n            return AddedSample;''',
    '''            const FVector GlobalCoordinate =
                GetGlobalCoordinate(FineCoordinate);

            FCubusDensitySample& AddedSample =
                Samples.Add(
                    PackedCoordinate,
                    SampleConsistentField(DensityField, GlobalCoordinate)
                );

            return AddedSample;'''
)
# Remove obsolete canonical-boundary interpolation function.
regex_replace_once(
    mesher_cpp,
    r'''    private:\n        FCubusDensitySample SampleCanonicalBoundary\(.*?\n        const ICubusDensityField& DensityField;''',
    '''    private:
        const ICubusDensityField& DensityField;'''
)

# Extend adaptive edge interpolation and transform its rendered position.
replace_once(
    mesher_cpp,
    '''        const FVector& ChunkMinimum,
        const float CanonicalVoxelSize,
        const float IsoLevel
    )
''',
    '''        const FVector& ChunkMinimum,
        const float CanonicalVoxelSize,
        const float IsoLevel,
        const int32 SelfSubdivisions,
        const FCubusDensityTransitionFaces& TransitionFaces
    )
'''
)
replace_once(
    mesher_cpp,
    '''        Result.LocalPosition =
            ChunkMinimum + LocalSamplePosition * CanonicalVoxelSize;
''',
    '''        Result.LocalPosition =
            ChunkMinimum +
            ApplyTransitionTransform(
                LocalSamplePosition,
                SelfSubdivisions,
                TransitionFaces
            ) * CanonicalVoxelSize;
'''
)

# Add true transition cells before the mesher namespace closes.
transition_code = r'''

    struct FTransitionPoint
    {
        FCubusDensitySample Sample;
        FVector LocalGeometryPosition = FVector::ZeroVector;
        FVector GlobalSamplePosition = FVector::ZeroVector;
        FVector Gradient = FVector::ZeroVector;
    };

    FInterpolatedVertex InterpolateTransitionEdge(
        const FTransitionPoint& PointA,
        const FTransitionPoint& PointB,
        const FVector& ChunkMinimum,
        const float CanonicalVoxelSize,
        const float IsoLevel
    )
    {
        const float DensityDelta = PointB.Sample.Density - PointA.Sample.Density;
        const float Alpha = FMath::IsNearlyZero(DensityDelta)
            ? 0.5f
            : FMath::Clamp(
                (IsoLevel - PointA.Sample.Density) / DensityDelta,
                0.0f,
                1.0f
            );

        const bool bAIsSolid = PointA.Sample.IsSolid(IsoLevel);
        FInterpolatedVertex Result;
        Result.LocalPosition = ChunkMinimum +
            FMath::Lerp(PointA.LocalGeometryPosition, PointB.LocalGeometryPosition, Alpha) * CanonicalVoxelSize;
        Result.GlobalSamplePosition = FMath::Lerp(PointA.GlobalSamplePosition, PointB.GlobalSamplePosition, Alpha);
        Result.Normal = (-FMath::Lerp(PointA.Gradient, PointB.Gradient, Alpha)).GetSafeNormal();
        Result.MaterialId = ClampDensityMaterialId(
            bAIsSolid ? PointA.Sample.MaterialId : PointB.Sample.MaterialId
        );
        SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

        if (Result.Normal.IsNearlyZero())
        {
            const FVector SolidToEmpty = bAIsSolid
                ? PointB.LocalGeometryPosition - PointA.LocalGeometryPosition
                : PointA.LocalGeometryPosition - PointB.LocalGeometryPosition;
            Result.Normal = SolidToEmpty.GetSafeNormal();
        }
        if (Result.Normal.IsNearlyZero())
        {
            Result.Normal = FVector::UpVector;
        }
        return Result;
    }

    void BuildTransitionCells(
        const ICubusDensityField& DensityField,
        const FIntVector& ChunkCoordinate,
        const float CanonicalVoxelSize,
        const int32 SelfSubdivisions,
        const float IsoLevel,
        const FCubusDensityTransitionFaces& TransitionFaces,
        FCubusMeshData& UnifiedMesh,
        int32& InOutGeneratedTriangleCount
    )
    {
        using namespace CubusTransvoxelTables;

        const int32 Subdivisions = FCubusDensityLod::NormalizeSubdivisions(SelfSubdivisions);
        const float CoarseSpacing = 1.0f / static_cast<float>(Subdivisions);
        const float FineSpacing = CoarseSpacing * 0.5f;
        const float TransitionThickness = CoarseSpacing * 0.5f;
        const int32 FaceCellCount = Cubus::ChunkSize * Subdivisions;
        const FVector ChunkGlobalOrigin = ToVector(ChunkCoordinate * Cubus::ChunkSize);
        const float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;
        const FVector ChunkMinimum(-ChunkWorldSize * 0.5f, -ChunkWorldSize * 0.5f, -ChunkWorldSize * 0.5f);

        /*
         * Official Transvoxel transition-point numbering around the high-res
         * face is perimeter-first plus centre:
         *
         *     6 -- 5 -- 4
         *     |    |    |
         *     7 -- 8 -- 3
         *     |    |    |
         *     0 -- 1 -- 2
         *
         * Low-resolution duplicate points are 9=0, A=2, B=6, C=4 and are
         * displaced inward geometrically while retaining the coarse-corner
         * scalar values. This is what lets the transition topology match both
         * the fine boundary contour and the transformed coarse regular mesh.
         */
        static constexpr int32 HighPointGrid[9][2] =
        {
            {0, 0}, {1, 0}, {2, 0},
            {2, 1}, {2, 2}, {1, 2},
            {0, 2}, {0, 1}, {1, 1}
        };
        static constexpr int32 LowPointHighIndex[4] = { 0, 2, 6, 4 };

        for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
        {
            const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
            if (!TransitionFaces.HasFinerNeighbour(Face, Subdivisions))
            {
                continue;
            }

            const FTransitionFaceBasis Basis = GetTransitionFaceBasis(Face);
            for (int32 VCell = 0; VCell < FaceCellCount; ++VCell)
            {
                for (int32 UCell = 0; UCell < FaceCellCount; ++UCell)
                {
                    const float U0 = static_cast<float>(UCell) * CoarseSpacing;
                    const float V0 = static_cast<float>(VCell) * CoarseSpacing;
                    FTransitionPoint Points[13];
                    int32 CaseIndex = 0;

                    for (int32 PointIndex = 0; PointIndex < 9; ++PointIndex)
                    {
                        const float U = U0 + static_cast<float>(HighPointGrid[PointIndex][0]) * FineSpacing;
                        const float V = V0 + static_cast<float>(HighPointGrid[PointIndex][1]) * FineSpacing;
                        FTransitionPoint& Point = Points[PointIndex];
                        Point.LocalGeometryPosition = Basis.BoundaryOrigin + Basis.U * U + Basis.V * V;
                        Point.GlobalSamplePosition = ChunkGlobalOrigin + Point.LocalGeometryPosition;
                        Point.Sample = SampleConsistentField(DensityField, Point.GlobalSamplePosition);
                        Point.Gradient = SampleFieldGradient(DensityField, Point.GlobalSamplePosition, FineSpacing);
                        if (Point.Sample.IsSolid(IsoLevel))
                        {
                            CaseIndex |= 1 << PointIndex;
                        }
                    }

                    if (CaseIndex == 0 || CaseIndex == 511)
                    {
                        continue;
                    }

                    for (int32 LowIndex = 0; LowIndex < 4; ++LowIndex)
                    {
                        const int32 HighIndex = LowPointHighIndex[LowIndex];
                        Points[9 + LowIndex] = Points[HighIndex];
                        Points[9 + LowIndex].LocalGeometryPosition += Basis.Inward * TransitionThickness;
                    }

                    const uint8 RawClass = TransitionCellClass[CaseIndex];
                    const bool bReverseWinding = (RawClass & 0x80u) != 0;
                    const FTransitionCellData& CellData = TransitionCellData[RawClass & 0x7Fu];
                    const int32 VertexCount = CellData.GetVertexCount();
                    FInterpolatedVertex CellVertices[12];

                    for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
                    {
                        const uint16 VertexData = TransitionVertexData[CaseIndex][VertexIndex];
                        const uint8 EndpointCode = static_cast<uint8>(VertexData & 0x00FFu);
                        const int32 PointA = (EndpointCode >> 4) & 0x0F;
                        const int32 PointB = EndpointCode & 0x0F;
                        if (PointA >= 13 || PointB >= 13)
                        {
                            ensureMsgf(false, TEXT("Invalid Transvoxel endpoint %d-%d for case %d"), PointA, PointB, CaseIndex);
                            continue;
                        }
                        CellVertices[VertexIndex] = InterpolateTransitionEdge(
                            Points[PointA],
                            Points[PointB],
                            ChunkMinimum,
                            CanonicalVoxelSize,
                            IsoLevel
                        );
                    }

                    const int32 TriangleCount = CellData.GetTriangleCount();
                    for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
                    {
                        int32 A = CellData.VertexIndex[TriangleIndex * 3 + 0];
                        int32 B = CellData.VertexIndex[TriangleIndex * 3 + 1];
                        int32 C = CellData.VertexIndex[TriangleIndex * 3 + 2];
                        if (A >= VertexCount || B >= VertexCount || C >= VertexCount)
                        {
                            ensureMsgf(false, TEXT("Invalid Transvoxel triangle indices for case %d"), CaseIndex);
                            continue;
                        }
                        if (bReverseWinding)
                        {
                            Swap(B, C);
                        }

                        if (AddTriangle(UnifiedMesh, CellVertices[A], CellVertices[B], CellVertices[C]))
                        {
                            ++InOutGeneratedTriangleCount;
                        }
                    }
                }
            }
        }
    }
'''
replace_once(
    mesher_cpp,
    '''        return true;
    }
}

void FCubusDensityMesher::BuildChunk(
''',
    '''        return true;
    }
''' + transition_code + '''
}

void FCubusDensityMesher::BuildChunk(
'''
)

# Extend BuildChunk signature.
replace_once(
    mesher_cpp,
    '''    int32& OutGeneratedTriangleCount,
    const ICubusDensityField* SurfaceMaterialField
)
''',
    '''    int32& OutGeneratedTriangleCount,
    const ICubusDensityField* SurfaceMaterialField,
    const ICubusDensityField* TransitionField,
    const FCubusDensityTransitionFaces& TransitionFaces
)
'''
)
# Ordinary edge call: canonical chunk is subdivision 1.
replace_once(
    mesher_cpp,
    '''                                    VoxelSize,
                                    IsoLevel
                                );''',
    '''                                    VoxelSize,
                                    IsoLevel,
                                    1,
                                    TransitionFaces
                                );'''
)
# Add transition cells before the BuildChunk empty-mesh cleanup.
replace_once(
    mesher_cpp,
    '''    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(
            UnifiedDensityMaterialKey
        );
    }
}

void FCubusDensityMesher::BuildAdaptiveChunk(
''',
    '''    if (TransitionField != nullptr)
    {
        BuildTransitionCells(
            *TransitionField,
            DensityBuffer.GetChunkCoordinate(),
            VoxelSize,
            1,
            IsoLevel,
            TransitionFaces,
            UnifiedMesh,
            OutGeneratedTriangleCount
        );
    }

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(
            UnifiedDensityMaterialKey
        );
    }
}

void FCubusDensityMesher::BuildAdaptiveChunk(
'''
)

# Extend adaptive signature and its subdivision-1 forwarding path.
replace_once(
    mesher_cpp,
    '''    TMap<int32, FCubusMeshData>& OutMaterialMeshes,
    int32& OutGeneratedTriangleCount
)
''',
    '''    TMap<int32, FCubusMeshData>& OutMaterialMeshes,
    int32& OutGeneratedTriangleCount,
    const FCubusDensityTransitionFaces& TransitionFaces
)
'''
)
replace_once(
    mesher_cpp,
    '''            OutMaterialMeshes,
            OutGeneratedTriangleCount
        );
''',
    '''            OutMaterialMeshes,
            OutGeneratedTriangleCount,
            nullptr,
            &DensityField,
            TransitionFaces
        );
'''
)
# Adaptive edge call gets own subdivision and transition faces.
replace_once(
    mesher_cpp,
    '''                                                CanonicalVoxelSize,
                                                IsoLevel
                                            );''',
    '''                                                CanonicalVoxelSize,
                                                IsoLevel,
                                                Subdivisions,
                                                TransitionFaces
                                            );'''
)
# Add adaptive transition cells before final cleanup. This is the last cleanup
# block in the file, so use rfind-style anchored replacement via regex.
regex_replace_once(
    mesher_cpp,
    r'''\n    if \(UnifiedMesh\.IsEmpty\(\)\)\n    \{\n        OutMaterialMeshes\.Remove\(UnifiedDensityMaterialKey\);\n    \}\n\}\s*$''',
    '''
    BuildTransitionCells(
        DensityField,
        ChunkCoordinate,
        CanonicalVoxelSize,
        Subdivisions,
        IsoLevel,
        TransitionFaces,
        UnifiedMesh,
        OutGeneratedTriangleCount
    );

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
    }
}
'''
)

# ---------------------------------------------------------------------------
# Replace the old positional-only mixed-boundary test with boundary-segment
# equality tests. A watertight seam must have exactly the same contour segments
# on both chunk meshes, for both 1<->2 and 2<->4 transitions on all six faces.
# ---------------------------------------------------------------------------
tests = 'Source/Orakai/CubusCore/Tests/CubusDensityMesherTests.cpp'
# Add a continuous sphere field after FWavyHeightField.
replace_once(
    tests,
    '''    int32 CountVertices(
''',
    '''    class FContinuousSphereField final : public ICubusDensityField
    {
    public:
        FContinuousSphereField(const FVector& InCentre, const float InRadius)
            : Centre(InCentre), Radius(InRadius)
        {
        }

        virtual FCubusDensitySample Sample(const FIntVector& Coordinate) const override
        {
            return SampleContinuous(FVector(Coordinate.X, Coordinate.Y, Coordinate.Z));
        }

        virtual FCubusDensitySample SampleContinuous(const FVector& Coordinate) const override
        {
            FCubusDensitySample Result;
            Result.Density = Radius - static_cast<float>(FVector::Distance(Coordinate, Centre));
            Result.MaterialId = Result.Density > 0.0f ? 3 : 0;
            return Result;
        }

    private:
        FVector Centre = FVector::ZeroVector;
        float Radius = 10.0f;
    };

    int32 CountVertices(
'''
)
# Add boundary segment helper before namespace close (identified before first test).
replace_once(
    tests,
    '''    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityHorizontalPlaneTest,
''',
    '''    }

    FString QuantizedVertexKey(const FVector& Position)
    {
        return FString::Printf(
            TEXT("%d,%d,%d"),
            FMath::RoundToInt(Position.X * 10000.0),
            FMath::RoundToInt(Position.Y * 10000.0),
            FMath::RoundToInt(Position.Z * 10000.0)
        );
    }

    void AddBoundarySegments(
        const TMap<int32, FCubusMeshData>& MaterialMeshes,
        const FIntVector& ChunkCoordinate,
        const int32 Axis,
        const double BoundaryPosition,
        TSet<FString>& OutSegments
    )
    {
        const FVector ChunkWorldOrigin(
            static_cast<double>(ChunkCoordinate.X * Cubus::ChunkSize),
            static_cast<double>(ChunkCoordinate.Y * Cubus::ChunkSize),
            static_cast<double>(ChunkCoordinate.Z * Cubus::ChunkSize)
        );

        for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
        {
            const FCubusMeshData& Mesh = Pair.Value;
            for (int32 TriangleIndex = 0; TriangleIndex + 2 < Mesh.Triangles.Num(); TriangleIndex += 3)
            {
                const FVector Positions[3] =
                {
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 0]],
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 1]],
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 2]]
                };
                static constexpr int32 EdgePairs[3][2] = { {0,1}, {1,2}, {2,0} };
                for (const int32* Edge : EdgePairs)
                {
                    const FVector& A = Positions[Edge[0]];
                    const FVector& B = Positions[Edge[1]];
                    if (!FMath::IsNearlyEqual(A[Axis], BoundaryPosition, 0.0001) ||
                        !FMath::IsNearlyEqual(B[Axis], BoundaryPosition, 0.0001))
                    {
                        continue;
                    }
                    FString KeyA = QuantizedVertexKey(A);
                    FString KeyB = QuantizedVertexKey(B);
                    if (KeyA == KeyB)
                    {
                        continue;
                    }
                    if (KeyB < KeyA)
                    {
                        Swap(KeyA, KeyB);
                    }
                    OutSegments.Add(KeyA + TEXT("|") + KeyB);
                }
            }
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityHorizontalPlaneTest,
'''
)

# Replace old mixed test through endif.
regex_replace_once(
    tests,
    r'''IMPLEMENT_SIMPLE_AUTOMATION_TEST\(\n    FCubusMixedDensityLodBoundaryTest,.*?\n#endif''',
    '''IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusMixedDensityLodBoundaryTest,
    "Orakai.Cubus.Density.LOD.WatertightTransitions",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusMixedDensityLodBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace CubusDensityMesherTests;

    const int32 CoarseSubdivisions[] = { 1, 2 };
    for (const int32 CoarseSubdivisionsValue : CoarseSubdivisions)
    {
        const int32 FineSubdivisions = CoarseSubdivisionsValue * 2;
        for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
        {
            const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
            const FIntVector FineChunkCoordinate = FCubusDensityTransitionFaces::GetOffset(Face);
            const int32 Axis = FaceIndex / 2;
            const bool bPositive = (FaceIndex & 1) != 0;
            const double BoundaryPosition = bPositive ? 16.0 : -16.0;

            FVector SphereCentre = FVector::ZeroVector;
            SphereCentre[Axis] = BoundaryPosition;
            const FContinuousSphereField Field(SphereCentre, 10.0f);

            FCubusDensityTransitionFaces CoarseFaces;
            CoarseFaces.Set(Face, FineSubdivisions);

            TMap<int32, FCubusMeshData> CoarseMeshes;
            TMap<int32, FCubusMeshData> FineMeshes;
            int32 CoarseTriangleCount = 0;
            int32 FineTriangleCount = 0;

            FCubusDensityMesher::BuildAdaptiveChunk(
                Field,
                FIntVector::ZeroValue,
                1.0f,
                CoarseSubdivisionsValue,
                0.0f,
                CoarseMeshes,
                CoarseTriangleCount,
                CoarseFaces
            );
            FCubusDensityMesher::BuildAdaptiveChunk(
                Field,
                FineChunkCoordinate,
                1.0f,
                FineSubdivisions,
                0.0f,
                FineMeshes,
                FineTriangleCount
            );

            const FString Context = FString::Printf(
                TEXT("%dx->%dx face %d"),
                CoarseSubdivisionsValue,
                FineSubdivisions,
                FaceIndex
            );
            TestTrue(*FString::Printf(TEXT("%s coarse mesh intersects transition"), *Context), CoarseTriangleCount > 0);
            TestTrue(*FString::Printf(TEXT("%s fine mesh intersects transition"), *Context), FineTriangleCount > 0);

            TSet<FString> CoarseSegments;
            TSet<FString> FineSegments;
            AddBoundarySegments(
                CoarseMeshes,
                FIntVector::ZeroValue,
                Axis,
                BoundaryPosition,
                CoarseSegments
            );
            AddBoundarySegments(
                FineMeshes,
                FineChunkCoordinate,
                Axis,
                BoundaryPosition,
                FineSegments
            );

            TestTrue(*FString::Printf(TEXT("%s emits shared-boundary contour segments"), *Context), CoarseSegments.Num() > 0);
            bool bSegmentsMatch = CoarseSegments.Num() == FineSegments.Num();
            if (bSegmentsMatch)
            {
                for (const FString& Segment : CoarseSegments)
                {
                    if (!FineSegments.Contains(Segment))
                    {
                        bSegmentsMatch = false;
                        break;
                    }
                }
            }
            TestTrue(*FString::Printf(TEXT("%s boundary segments are position/topology identical"), *Context), bSegmentsMatch);
        }
    }

    return true;
}

#endif'''
)

# ---------------------------------------------------------------------------
# Static source guards. These do not replace UBT/automation, but make the
# migration fail rather than silently committing a partial seam implementation.
# ---------------------------------------------------------------------------
checks = {
    lod_path: ['FCubusDensityTransitionFaces', 'HasFinerNeighbour', 'GetSignature'],
    mesher_h: ['TransitionField', 'TransitionFaces'],
    mesher_cpp: ['BuildTransitionCells(', 'TransitionCellClass[CaseIndex]', 'ApplyTransitionTransform(', 'SampleConsistentField('],
    voxel_h: ['FCubusDensityTransitionFaces TransitionFaces;'],
    voxel_cpp: ['Input.TransitionFaces', '&EditedDensityField'],
    world_h: ['TransitionSignature', 'BuildDensityTransitionFaces'],
    world_cpp: ['bTopologyStillCurrent', 'Distance == 1', 'ChangedCoordinates'],
    tests: ['WatertightTransitions', 'AddBoundarySegments', 'CoarseFaces.Set'],
    'Source/Orakai/CubusCore/Meshing/CubusTransvoxelTables.h': ['TransitionCellClass[512]', 'TransitionCellData[56]', 'TransitionVertexData[512][12]', 'MIT License'],
}
for path, tokens in checks.items():
    text = read(path)
    for token in tokens:
        if token not in text:
            raise RuntimeError(f'{path}: missing guard token {token!r}')

# The old blanket boundary interpolation is specifically forbidden now.
if 'SampleCanonicalBoundary' in read(mesher_cpp):
    raise RuntimeError('obsolete canonical boundary interpolation still present')
