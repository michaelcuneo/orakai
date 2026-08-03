# Cubus density meshing

Cubus supports block, density and hybrid rendering from the streamed voxel chunk
class. Density uses classic Marching Cubes over a continuous scalar field.

## Runtime authority

Orakai does not create terrain chunks in editor mode. `ACubusBlockWorldActor`
streams them after Play begins using its configured `ChunkActorClass`:

```text
ACubusBlockWorldActor
    |
    | ChunkActorClass = BP_CubusVoxelPCGChunk
    v
SpawnChunkAtCoordinate()
    |
    v
BP_CubusVoxelPCGChunk runtime instance
```

The Blocks / Density / Hybrid setting is authored on
`BP_CubusVoxelPCGChunk`'s inherited class defaults. The world actor deliberately
has no second render-mode property.

To select density:

1. Open `BP_CubusVoxelPCGChunk`.
2. Open Class Defaults.
3. Under `Cubus | Rendering`, set the inherited render mode to `Density`.
4. Compile and save the Blueprint.
5. Confirm the world's `ChunkActorClass` is `BP_CubusVoxelPCGChunk`.
6. Press Play so runtime streaming creates the chunks.

The world's read-only render-mode helper resolves the configured chunk class
default object. It exists so a streamed chunk can consistently obtain the mode
authored on `BP_CubusVoxelPCGChunk`; it is not a user-facing world switch.

## Authoritative render component

Every runtime chunk has one authoritative terrain renderer: the inherited root
`UProceduralMeshComponent` named `ProceduralMesh`.

- `Blocks` submits block-mesher sections to the root mesh.
- `Density` submits Marching Cubes sections to the root mesh.
- `Hybrid` appends both groups to the root mesh.

Using the existing root is required because Orakai already uses it for:

- collision and visibility traces,
- voxel hit resolution,
- player spawn placement,
- streamed chunk mobility and teardown,
- materials,
- and near-field ray-tracing proxies.

## Legacy `DensityMesh` compatibility object

An earlier implementation briefly added a separate native component named
`DensityMesh`. `BP_CubusVoxelPCGChunk` serialized a `BodySetup_0` object beneath
that component. Removing the class and default subobject afterwards caused:

```text
CreateExport: Failed to load Outer for resource 'BodySetup_0'
```

The branch now restores an inert `UCubusDensityMeshComponent` and a hidden
`DensityMesh` default subobject on `ACubusPCGVoxelVolumeActor` solely so that old
Blueprint exports can load. It does not render, collide, tick or participate in
density generation. Real density terrain is still submitted to the root
`ProceduralMesh`.

After the corrected build loads successfully, compile and save
`BP_CubusVoxelPCGChunk` once so Unreal normalizes the asset against the current
native class layout. No actor or component needs to be manually added or
removed.

## Scalar-field pipeline

```text
BP_CubusVoxelPCGChunk render mode = Density
        |
        v
ACubusVoxelVolumeActor::RebuildVolume
        |
        v
FCubusTerrainDensityField
        |
        v
FCubusDensityEditField (generated field + sparse edit snapshot)
        |
        v
player-distance density LOD
        |
        +-- 100 cm: 35 x 35 x 35 halo buffer
        |
        +-- 50/25/10 cm: sparse adaptive surface sampling
        |
        v
FCubusDensityMesher
        |
        v
root ProceduralMesh sections
```

`FCubusTerrainDensityField` evaluates:

```text
terrain density = continuous surface sample Z - global sample Z
```

Fractional height is preserved, so the surface can cross a voxel edge anywhere
rather than being locked to a block staircase.

The field also uses:

- the same deterministic terrain-domain offset as block generation,
- the owning world's generation seed,
- continuous seeded river lowering,
- and geology-profile cave density.

Where caves are enabled:

```text
final density = min(terrain density, cave density)
```

## Coordinate convention

A chunk owns voxel coordinates `0..31` on each axis. Geometry is centred around
the chunk actor, so density sample `0` is at the local chunk minimum and sample
`32` is at the local chunk maximum.

Marching Cubes owns cells whose lower sample coordinate is `0..31`. Their
corners require samples `0..32`, and central-difference normals require the halo
`-1..33`, producing a `35 x 35 x 35` temporary buffer.

All field samples use canonical global voxel coordinates. One canonical unit is
still one block voxel (`GeneratedVoxelSize`, normally 100 cm). Adaptive density
samples use fractional canonical coordinates, so 25 cm density sampling occurs
at `0.25`-voxel intervals without shrinking chunks, mountains, rivers, caves,
vegetation or saved edits.

Adjacent streamed chunks receive the same scalar values without depending on
neighbour chunk data. Fine chunks lock their boundary samples to interpolation
of the canonical one-metre lattice. This keeps their boundary contour on the
same coarse segments used by a neighbouring lower-LOD chunk instead of opening
cracks when the two resolutions meet.

## Density LOD

`ACubusBlockWorldActor` selects density resolution from three player-centred
tiers. Defaults for a 100 cm canonical voxel are:

| Tier | Chunk radius | Sample spacing | Subdivisions |
|---|---:|---:|---:|
| Near | 1 | 25 cm | 4 |
| Middle | 3 | 50 cm | 2 |
| Far | remaining view | 100 cm | 1 |

The supported subdivisions are `1`, `2`, `4` and `10`. Setting the near sample
spacing to 10 cm enables the 10x tier, but it is intentionally not the default:
even sparse surface refinement produces considerably more triangles and field
queries at that resolution.

The actor remeshes an existing density or hybrid chunk only when player motion
moves it into another tier. Block-mode chunks ignore density LOD. Chunk actor
locations and extents always remain `32 * GeneratedVoxelSize`, so streaming and
persistence continue to address the same world volume.

## Runtime editing

`UCubusVoxelEditLibrary` exposes separate Blueprint nodes for the two terrain
representations:

- `Remove Cubus Block Brush From Hit`
- `Add Cubus Block Brush From Hit`
- `Remove Cubus Density From Hit`
- `Add Cubus Density From Hit`

Brush radii are measured in canonical voxel/sample coordinates. Block brushes batch all
writes by chunk, save each touched chunk once, then queue the touched chunks and
their face neighbours for remeshing. Density brushes accumulate sparse scalar
deltas and queue a `3 x 3 x 3` chunk halo because central-difference normals can
depend on diagonal samples.

Density edits are retained by the world actor while chunks stream out and back
in. A rebuilding chunk receives an immutable, chunk-local snapshot containing
the canonical edit lattice plus its normal halo. Fine density samples
trilinearly interpolate those deltas, so changing LOD does not move or discard
an edit.

## Runtime diagnostics

Before generation, each streamed PCG chunk logs:

```text
Cubus streamed chunk class=BP_CubusVoxelPCGChunk_C coordinate=(X, Y, Z) renderMode=1
```

After rebuilding:

```text
Cubus chunk (X, Y, Z) built mode=1 densityStep=25.0cm rootSections=3 blockSections=0 densitySections=3 densityTriangles=...
```

Mode values are:

```text
0 = Blocks
1 = Density
2 = Hybrid
```

A chunk intersecting the terrain surface must report:

```text
renderMode=1
blockSections=0
densitySections>0
densityTriangles>0
```

A vertical chunk completely above or below the isosurface can legitimately
report zero density sections.

## Current limitations

The native density path does not yet include:

- SpaceTimeDB density deltas,
- discrete blocks embedded in density,
- liquid surfaces,
- complete biome, strata and ore material parity,
- density-native vegetation placement,
- asynchronous revisioned meshing,
- chunk-wide edge-vertex reuse,
- or block/density transition cells.

Block and density edits remain deliberately separate: block brushes modify
`FCubusBlockChunkData`, while density brushes modify the scalar-field overlay.

## Regression coverage

Automation tests cover:

- a full-chunk horizontal plane,
- a sphere crossing a chunk boundary,
- adaptive 25 cm extraction inside a fixed 32 metre chunk,
- mixed-resolution canonical boundary locking,
- fractional surface placement,
- deterministic seeded-domain displacement,
- block-generator/native-density height parity,
- continuous river lowering,
- three-dimensional cave carving and surface clearance,
- sparse add/remove density overlay, continuous edit interpolation and material overrides,
- and mutable block-chunk occupancy invalidation.
