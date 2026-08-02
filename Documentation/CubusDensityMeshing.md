# Cubus density meshing

Cubus supports block, density and hybrid rendering from one voxel world. The
density path uses classic Marching Cubes over a continuous scalar field.

## Authoritative render component

Every `ACubusVoxelVolumeActor` has one authoritative
`UProceduralMeshComponent`: its root `ProceduralMesh`.

All render modes submit sections to that same component:

- `Blocks` submits block-mesher sections.
- `Density` submits Marching Cubes sections.
- `Hybrid` submits block sections followed by density sections.

This is intentional. Existing Cubus systems already treat the root procedural
mesh as terrain:

- collision and visibility traces,
- voxel hit resolution,
- player spawn placement,
- streamed chunk mobility and teardown,
- material assignment,
- and near-field ray-tracing proxy generation.

Density is therefore not a child component, separate actor, or Blueprint-added
component.

## Scalar-field pipeline

- `ICubusDensityField` is the read-only scalar-field interface.
- `FCubusTerrainDensityField` evaluates seeded terrain directly as continuous
  density.
- `FCubusBlockDensityField` is retained as a future block/density transition
  adapter, but is not used for ordinary density terrain.
- `FCubusDensitySamplingBuffer` caches the `35 x 35 x 35` sample region required
  for one `32 x 32 x 32` mesh-cell chunk and central-difference normals.
- `FCubusDensityMesher` extracts per-material Marching Cubes sections with
  interpolated positions, gradient normals, stable UV projection, tangents and
  Cubus face selectors.

The native terrain field uses:

```text
terrain density = continuous surface sample Z - global sample Z
```

The height remains fractional, so the zero crossing can occur anywhere along a
voxel edge instead of being locked to a block staircase.

Where geology enables caves, the field combines terrain and cave density:

```text
final density = min(terrain density, cave density)
```

The field also applies the same deterministic terrain-domain offset used by the
block generator and continuous river lowering using the geology profile's river
seed and parameters.

## Selecting the world render mode

Select `ACubusBlockWorldActor` and use:

```text
Cubus > Rendering > Voxel Render Mode
```

The choices are:

- `Blocks`
- `Density`
- `Hybrid (Blocks + Density)`

Changing the property synchronizes registered chunks with the world's current:

- generation seed,
- terrain settings,
- geology profile,
- material registry,
- and render mode,

then rebuilds their root terrain meshes.

At runtime:

```cpp
BlockWorld->SetVoxelRenderMode(
    ECubusVoxelRenderMode::Density,
    true
);
```

The second argument determines whether loaded chunks rebuild immediately.

## Coordinate convention

A chunk owns voxel coordinates `0..31` on each axis. Geometry is centred around
the chunk actor, so density sample `0` is placed at the local chunk minimum and
sample `32` at the local chunk maximum.

A block surface whose top occupied voxel has integer height `H` ends at sample
plane `H + 1`. Density preserves that vertical alignment while retaining the
fractional terrain height.

A density chunk owns Marching Cubes cells with lower sample coordinates
`0..31`. Cell corners require samples `0..32`, while central-difference normals
require the halo `-1..33`.

All field samples use global voxel coordinates. Shared chunk boundaries
therefore receive the same scalar values without requiring neighbouring chunks
to be loaded.

## Diagnostics

Each chunk records:

- `Last Built Render Mode`
- `Generated Block Section Count`
- `Generated Density Section Count`
- `Generated Density Triangle Count`
- total root-mesh sections, vertices and triangles
- build time

A completed build also logs one line in this form:

```text
Cubus chunk (X, Y, Z) built mode=1 rootSections=3 blockSections=0 densitySections=3 ...
```

Render-mode values are:

```text
0 = Blocks
1 = Density
2 = Hybrid
```

A density chunk can legitimately have zero sections when the entire chunk is
inside or outside the field. A world needs vertically adjacent chunks around
the terrain surface; runtime streaming already loads a vertical range, while a
manually generated editor grid must use suitable `Grid Origin` and
`Grid Dimensions Z` values.

## Audited limitations

The following are not yet represented by the native density field:

- sparse player density edits,
- SpaceTimeDB density deltas,
- discrete blocks embedded in or replacing density,
- liquid surfaces,
- biome-specific surface-material overrides,
- strata and ore material layers,
- and a dedicated block/density transition mesher.

Block voxel edits currently modify `FCubusBlockChunkData`; Density mode does not
yet translate those edits into scalar-field changes. Vegetation and some spawn
fallback logic also still derive from block chunk data, so they approximate the
continuous surface until those systems are moved to world density queries.

Meshing is synchronous, vertices are emitted per triangle rather than through a
chunk-wide edge cache, and material selection is one dominant material per
triangle.

## Regression coverage

Automation tests cover:

- a full-chunk horizontal plane,
- a sphere crossing a chunk boundary,
- fractional surface placement,
- deterministic seeded terrain domains,
- continuous river lowering,
- and three-dimensional cave carving with surface clearance.
