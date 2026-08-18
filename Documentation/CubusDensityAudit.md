# Cubus density implementation audit

## Runtime architecture confirmed

Orakai does not author terrain chunks in the editor. The runtime path is:

```text
ACubusBlockWorldActor
    |
    | ChunkActorClass = BP_CubusVoxelPCGChunk
    v
SpawnChunkAtCoordinate()
    |
    v
ACubusPCGVoxelVolumeActor instance
    |
    | inherited Voxel Render Mode authored on the Blueprint CDO
    v
GenerateTerrainData()
    |
    v
RebuildVolume()
    |
    +-- Blocks  -> FCubusBlockMesher
    +-- Density -> FCubusTerrainDensityField -> FCubusDensityMesher
    +-- Hybrid  -> both
    |
    v
root UProceduralMeshComponent
```

`BP_CubusVoxelPCGChunk` is the render-mode authority. The world actor owns no
separate Blocks / Density / Hybrid property. Its `GetVoxelRenderMode()` helper
only reads the configured `ChunkActorClass` default object so streamed chunks
resolve the Blueprint-authored value consistently.

## Scope audited

The audit followed every Orakai path that creates, modifies, renders or consumes
runtime chunk terrain:

- world startup and runtime streaming,
- `ChunkActorClass` resolution and `SpawnActor`,
- `BP_CubusVoxelPCGChunk`'s native parent path,
- chunk ownership, registration and removal,
- generation seed initialization,
- local block-cache lookup and regeneration,
- base terrain, regions, valleys and mountains,
- geology, rivers and caves,
- block and density field construction,
- coarse halo and sparse adaptive density sampling,
- Marching Cubes extraction,
- root procedural-mesh section submission,
- material assignment and collision cooking,
- voxel hit resolution,
- player spawn placement,
- vegetation's block-data dependencies,
- ray-tracing proxy generation,
- persistence value types,
- project plugin configuration,
- and density automation coverage.

Third-party plugin implementation internals were not changed.

## Root causes found

### 1. The chunk Blueprint mode was ignored

The earlier implementation added a second render-mode property to
`ACubusBlockWorldActor`. Runtime chunks then asked the world for the mode, which
meant the value selected on `BP_CubusVoxelPCGChunk` was not authoritative.

**Correction:** the world-level property and setter were removed. The world's
read-only resolver obtains the mode from the `ChunkActorClass` CDO, which is
`BP_CubusVoxelPCGChunk` in the runtime setup.

### 2. `BP_CubusVoxelPCGChunk` could not load cleanly

An earlier density implementation added a native default subobject named
`DensityMesh`, derived from `UCubusDensityMeshComponent`. The Blueprint saved an
inner `BodySetup_0` export whose Outer was that component. Removing the class and
subobject later produced:

```text
CreateExport: Failed to load Outer for resource 'BodySetup_0'
```

When the chunk Blueprint cannot load correctly, `ChunkActorClass` cannot
reliably provide its Density class default during streaming.

**Correction:** an inert serialization-compatibility
`UCubusDensityMeshComponent` class has been restored. The PCG chunk constructor
also recreates a hidden default subobject with the exact historical name
`DensityMesh`. It never renders or collides; all real terrain still uses the root
`ProceduralMesh`.

### 3. Density originally used the wrong render component

The first implementation rendered density through a second child procedural
mesh. Orakai's existing systems treat the chunk root procedural mesh as terrain:

- collision and visibility traces,
- voxel hit resolution,
- runtime mobility,
- teardown,
- and ray-tracing proxy generation.

**Correction:** Blocks, Density and Hybrid now submit sections to the existing
root `ProceduralMesh`.

### 4. The first scalar field was still block occupancy

The first pass converted each block to `+1` or `-1`. Marching Cubes could round
that staircase but could not recover continuous terrain.

**Correction:** ordinary Density mode evaluates a native field directly:

```text
terrain density = continuous surface sample Z - global sample Z
```

### 5. Native density initially ignored deterministic seed domains

The block generator shifts its terrain sampling domain with the world's derived
terrain seed. The initial native field did not.

**Correction:** density uses the same whole-chunk terrain offset as block
generation. The streamed PCG chunk now also applies the owning world's generation
seeds explicitly before cache lookup or generation, rather than relying only on
a global actor-spawn delegate.

### 6. Native density initially ignored geology shape changes

The initial continuous field omitted river lowering and caves.

**Correction:** it now applies the geology profile's seeded river field and 3D
cave field. Cave subtraction uses:

```text
final density = min(terrain density, cave density)
```

### 7. Mutable voxel access left occupancy state stale

Block generation writes through mutable voxel pointers after `Chunk.Clear()`.
The cached occupied/empty state was not invalidated, so systems could treat a
populated chunk as empty.

**Correction:** mutable voxel access invalidates the occupancy cache.

## Current runtime diagnostics

Each streamed PCG chunk logs its actual class and selected mode before generation:

```text
Cubus streamed chunk class=BP_CubusVoxelPCGChunk_C ... renderMode=1
```

Render-mode values are:

```text
0 = Blocks
1 = Density
2 = Hybrid
```

After rebuilding, the chunk logs:

```text
Cubus chunk (...) built mode=1 rootSections=...
blockSections=0 densitySections=... densityTriangles=...
```

For a chunk intersecting the terrain surface, Density mode must report:

```text
mode=1
blockSections=0
densitySections>0
densityTriangles>0
```

A completely solid or completely empty vertical chunk can legitimately produce
zero density sections.

## Separate editor-load warning

The project previously enabled the experimental `ProceduralVegetationEditor`
plugin. Its sample material `MA_UI_Element_Inst` references a package under
`/Quixel_Utilities`, but that mount point is unavailable in the current engine
installation.

No Orakai C++ source references that editor plugin, and Orakai already owns its
runtime vegetation path. The branch therefore removes
`ProceduralVegetationEditor` from `Orakai.uproject`, preventing the unrelated
sample asset and missing Quixel package from loading.

This change is independent of voxel density.

## Remaining density work

The following are not yet part of the native scalar field:

- SpaceTimeDB density deltas,
- discrete blocks embedded in density,
- liquid surfaces,
- complete biome/strata/ore material parity,
- density-native vegetation queries,
- revisioned asynchronous meshing,
- shared edge vertices,
- full block/density transition cells.

Density mesh LOD is now player-centred and independent of the canonical block
grid. Fine chunks use fractional field sampling while retaining the same 32 m
chunk bounds, generation domain and persisted edit coordinates. Mixed density
LOD boundaries are locked to the canonical lattice; this is separate from the
still-unimplemented transition between block geometry and density geometry.

Sparse player density edits now wrap the generated field before halo sampling.
They survive chunk unload/reload for the lifetime of the world actor, but are
not yet persisted across sessions. Block edits remain a separate operation on
`FCubusBlockChunkData`, preserving the engine's explicit dual representation.

## Validation boundary

The branch has source-level tests for plane extraction, cross-chunk seams,
adaptive and mixed-LOD boundaries, fractional surfaces, continuous edit
interpolation, seeded-domain parity, rivers, caves and occupancy-cache
invalidation. The authoritative integration test remains an UnrealBuildTool
compile followed by PIE with streamed `BP_CubusVoxelPCGChunk` instances.
