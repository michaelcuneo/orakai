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
35 x 35 x 35 halo sampling buffer
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

All field samples use global voxel coordinates. Adjacent streamed chunks receive
the same scalar values on their shared boundary without depending on neighbour
chunk data.

## Runtime diagnostics

Before generation, each streamed PCG chunk logs:

```text
Cubus streamed chunk class=BP_CubusVoxelPCGChunk_C coordinate=(X, Y, Z) renderMode=1
```

After rebuilding:

```text
Cubus chunk (X, Y, Z) built mode=1 rootSections=3 blockSections=0 densitySections=3 densityTriangles=...
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

- sparse player density edits,
- SpaceTimeDB density deltas,
- discrete blocks embedded in density,
- liquid surfaces,
- complete biome, strata and ore material parity,
- density-native vegetation placement,
- asynchronous revisioned meshing,
- chunk-wide edge-vertex reuse,
- LOD meshes,
- or block/density transition cells.

Block edits currently modify `FCubusBlockChunkData`; they do not yet sculpt the
native scalar field.

## Regression coverage

Automation tests cover:

- a full-chunk horizontal plane,
- a sphere crossing a chunk boundary,
- fractional surface placement,
- deterministic seeded-domain displacement,
- block-generator/native-density height parity,
- continuous river lowering,
- three-dimensional cave carving and surface clearance,
- and mutable block-chunk occupancy invalidation.
