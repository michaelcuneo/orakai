# Cubus density meshing

Cubus now has a smooth density-rendering path that runs beside the existing
block mesher. The first implementation uses classic Marching Cubes and keeps
the scalar-field source separate from the mesher.

## Components

- `ICubusDensityField` is the read-only scalar-field interface.
- `FCubusBlockDensityField` adapts the current block voxel store into scalar
  samples, so existing generated terrain can be rendered immediately.
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
the same transitional surface twice. In `Density` mode collision is generated
from the density mesh.

## Coordinate convention

The block adapter treats each block-cell centre as a density sample. Its sample
offset is `(0.5, 0.5, 0.5)` voxel units, so the zero crossing between a solid
cell and an empty cell lands on the existing block boundary.

A density chunk owns Marching Cubes cells with lower sample coordinates
`0..31`. Those cells need corner samples `0..32`. Smooth normals additionally
need a one-sample halo, producing the buffered local range `-1..33`.

All lookups use global voxel coordinates. Adjacent chunks therefore calculate
identical positions on their shared boundary when the required source chunks
are loaded.

## Current scope

This is the first density milestone rather than the final hybrid data model.

- Meshing is synchronous.
- Vertices are emitted per triangle and are not yet edge-cached across cells.
- Material selection is currently one dominant material per triangle.
- The block adapter reads loaded chunks. A later world-level density provider
  will combine procedural density, sparse SpaceTimeDB edits, and material
  layers without depending on block chunk availability.
- Liquid density is optional through `bTreatWaterAsEmpty`; a dedicated liquid
  surface path is still separate work.

Automation coverage includes a full-chunk horizontal plane and a sphere that
crosses a chunk boundary. The seam test verifies that both chunks generate the
same quantized boundary vertex set.
