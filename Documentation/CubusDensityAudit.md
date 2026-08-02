# Cubus density implementation audit

## Scope

The density audit traced the complete terrain path through:

- `ACubusBlockWorldActor` construction, editor properties and streaming,
- chunk registration, generation, removal and transient state,
- `ACubusVoxelVolumeActor` data and render rebuilds,
- `ACubusPCGVoxelVolumeActor` cache loading,
- deterministic generation seeds,
- base terrain, regions, valleys and mountains,
- geology, rivers, caves, strata, ores and biomes,
- block and density meshers,
- procedural-mesh section and material submission,
- collision setup and asynchronous cooking,
- voxel hit resolution and edits,
- spawn placement,
- vegetation dependencies,
- ray-tracing proxy generation,
- local chunk cache metadata,
- persistence value types,
- project/plugin configuration,
- and automation coverage.

Third-party plugin internals were not modified; the audit followed every Orakai
system that consumes or produces terrain state.

## Critical faults corrected

### 1. Density used a separate child terrain mesh

The initial implementation rendered density through a second child
`UProceduralMeshComponent`. Existing Orakai systems use the chunk root
procedural mesh as terrain, including collision, hit resolution, mobility,
teardown and ray-tracing proxies.

**Correction:** Blocks, Density and Hybrid now submit sections to the existing
root `ProceduralMesh`.

### 2. Render-mode changes rebuilt stale chunk configuration

Changing `VoxelRenderMode` previously called `RebuildAllChunks()` without first
reapplying the world's seed, terrain settings, geology profile or material
registry. Level-authored chunks could therefore rebuild with constructor
defaults.

**Correction:** `SetVoxelRenderMode` refreshes the registry, reconstructs the
transient generated-chunk array, reapplies all world configuration and rebuilds
each root terrain mesh.

### 3. Native density ignored the world seed

Block terrain moves its sampling domain using the deterministic terrain seed.
The first native density field sampled unshifted coordinates, so Density and
Blocks represented different worlds.

**Correction:** Native density uses the same whole-chunk terrain-domain offset
calculated from `FCubusGenerationSeeds`.

### 4. Native density ignored geology shape operations

The first native field represented only the base height function. River
lowering and caves remained block-only operations.

**Correction:** Native density applies continuous seeded river lowering and a
three-dimensional cave scalar field with the geology profile's bounds,
frequencies, threshold and surface clearance.

### 5. Chunk occupancy caching could remain permanently stale

Terrain generators mutate voxels through mutable pointers after `Chunk.Clear()`.
The occupancy cache remained marked as known-empty, causing populated chunks to
be treated as empty by spawn, vegetation and ray-tracing systems.

**Correction:** Mutable voxel access invalidates the cached occupancy state.

### 6. Editor-reloaded chunks were absent from `GeneratedChunks`

`GeneratedChunks` is transient. A saved level could contain owned chunks while
the array used by clear/regenerate operations was empty.

**Correction:** Render-mode synchronization reconstructs the array from the
coordinate registry.

## Verified architecture after correction

```text
ACubusBlockWorldActor.VoxelRenderMode
        |
        v
world configuration synchronization
        |
        v
ACubusVoxelVolumeActor.RebuildVolume
        |
        +-- Blocks  --> FCubusBlockMesher -----------+
        |                                             |
        +-- Density --> FCubusTerrainDensityField     |
        |              -> sampling buffer             |
        |              -> FCubusDensityMesher --------+--> root ProceduralMesh
        |                                             |
        +-- Hybrid --> both section groups -----------+
```

The root component remains authoritative for rendering, material slots,
collision and downstream terrain consumers.

## Remaining high-priority gaps

### Density edits are not implemented

Block edits mutate `FCubusBlockChunkData`. The native field does not yet consume
sparse scalar-density edits, so adding or removing a block does not sculpt
Density mode.

Required next layer:

```text
procedural terrain density
+ procedural cave density
+ sparse SpaceTimeDB density edits
+ discrete block occupancy/overrides
```

### Liquid surfaces are absent

The density mesher currently produces solid terrain only. Ocean, lake and river
water require a separate liquid field/section path.

### Material parity is incomplete

The density field selects surface, rock, snow and subsurface materials. Biome
surface overrides, strata and ores are still generated in block data rather
than continuous material fields.

### Vegetation and spawn fallback use block data

Vegetation placement and one spawn fallback calculate the surface from
`FCubusBlockChunkData`. They remain close to, but not exactly on, the continuous
surface.

### Cache identity is incomplete

The local block cache is keyed by world seed and generation version, not a hash
of all terrain/geology parameters. Changing amplitudes or geology settings can
reuse stale block-derived data until the generation version or cache is reset.
Native density geometry itself is evaluated directly and does not use that
cache.

### Terrain-shape code is duplicated

The continuous density field currently mirrors the block terrain formula. A
shared deterministic terrain sampler should become the single source of truth
for both block occupancy and density sampling.

### Meshing remains synchronous

A density chunk samples `35^3` values and builds geometry on the calling thread.
Runtime streaming needs revisioned asynchronous jobs before increasing view
distance or density complexity.

### Vertex reuse and LOD are not implemented

Marching Cubes currently emits triangle-local vertices. Chunk-wide edge caches,
LOD meshes and transition cells are future performance work.

## Test coverage added

- classic horizontal plane extraction,
- sphere seam across a chunk boundary,
- fractional native terrain surface,
- deterministic seeded-domain displacement,
- continuous river lowering,
- three-dimensional cave carving and surface clearance,
- and mutable chunk occupancy-cache invalidation.

## Validation boundary

The code has been statically audited against the Orakai source and Unreal Engine
5.8 API documentation. A full UnrealBuildTool compile, editor load, PIE run and
automation-test pass must still be performed in the actual Orakai workspace.
That build is the authoritative validation for engine integration.
