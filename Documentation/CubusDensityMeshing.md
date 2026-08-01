# Cubus density meshing

Cubus has a smooth density-rendering path that runs beside the existing block
mesher. It uses classic Marching Cubes and keeps the scalar-field source
separate from geometry extraction.

## Components

- `ICubusDensityField` is the read-only scalar-field interface.
- `FCubusTerrainDensityField` evaluates the configured terrain function as a
  continuous scalar field. It preserves fractional height instead of rounding
  terrain into occupied block cells before meshing.
- `FCubusBlockDensityField` remains as a diagnostic and future hybrid adapter
  over the current block voxel store.
- `FCubusDensitySamplingBuffer` caches the 35 x 35 x 35 sample region required
  to mesh a 32 x 32 x 32 chunk with central-difference normals.
- `FCubusDensityMesher` extracts per-material mesh sections with interpolated
  vertices, gradient normals, world-stable planar UVs, tangents, and Cubus face
  selectors in vertex alpha.
- `UCubusDensityMeshComponent` is the procedural mesh component used by every
  `ACubusVoxelVolumeActor` for density geometry.

## Selecting the world render mode

Select the `ACubusBlockWorldActor` in the level and use:

`Cubus > Rendering > Voxel Render Mode`

The available modes are:

- `Blocks` builds only the existing hard-edged block mesh.
- `Density` builds only the Marching Cubes density mesh.
- `Hybrid (Blocks + Density)` builds both representations for comparison and
  future mixed-terrain work.

Changing the dropdown in the editor rebuilds the registered chunks. New
streamed chunks read the setting from their owning world whenever they build,
so no special chunk Blueprint or manually added density component is required.

After updating an existing level to the native terrain-density implementation,
run `Regenerate Terrain` once so every existing chunk receives the current
world terrain settings before it rebuilds.

At runtime the same switch is available through:

```cpp
BlockWorld->SetVoxelRenderMode(
    ECubusVoxelRenderMode::Density,
    true
);
```

The second argument controls whether currently registered chunks rebuild
immediately.

In `Hybrid` mode the block mesh currently owns collision to avoid submitting
the same surface twice. In `Density` mode collision is generated from the
density mesh.

## Why the first density pass still looked blocky

The first pass used `FCubusBlockDensityField`, which converted each generated
block into one of two scalar values:

```text
solid block = +1
empty block = -1
```

Marching Cubes interpolated between those values and produced smooth normals,
but the underlying surface was still the already-quantized block staircase.
It could round block corners, but it could not recover the fractional terrain
height that had already been discarded.

The native path now samples:

```text
Density = ContinuousSurfaceSampleZ - GlobalSampleZ
```

The terrain noise result remains a float. The isosurface can therefore cross an
edge at any fractional position rather than only halfway between an occupied
block and an empty block.

## Coordinate convention

Generated block terrain describes the top occupied cell by integer height `H`.
That cell ends at density sample plane `H + 1`. The native field keeps this
`+1` alignment but leaves `H` fractional, so changing between Blocks and
Density does not vertically shift the terrain.

A density chunk owns Marching Cubes cells with lower sample coordinates
`0..31`. Those cells need corner samples `0..32`. Smooth normals additionally
need a one-sample halo, producing the buffered local range `-1..33`.

All native terrain lookups use global voxel coordinates. Adjacent chunks
therefore calculate identical density values and surface positions along their
shared boundary without depending on neighbouring chunk data being loaded.

## Current scope

This remains an early density milestone rather than the final hybrid data
model.

- Meshing is synchronous.
- Vertices are emitted per triangle and are not yet edge-cached across cells.
- Material selection is currently one dominant material per triangle.
- Native terrain density currently represents the continuous height terrain.
  Caves, overhangs, sparse sculpting edits, and blocks embedded in density still
  need the composite three-dimensional field.
- The block-backed density adapter is still available by disabling
  `bUseNativeTerrainDensity` on the density mesh component for diagnostics.
- Liquid density remains a separate surface path.

Automation coverage includes a full-chunk horizontal plane, a sphere crossing a
chunk boundary, and a fractional-height plane that verifies Marching Cubes does
not snap the native surface back onto a block boundary.
