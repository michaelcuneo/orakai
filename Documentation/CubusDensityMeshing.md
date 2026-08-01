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
- `UCubusDensityMeshComponent` is a Blueprint-spawnable procedural mesh
  component that renders density geometry for an `ACubusVoxelVolumeActor`.

## Using it on a chunk

1. Create a Blueprint subclass of `ACubusVoxelVolumeActor`.
2. Add a `Cubus Density Mesh Component` and attach it to the chunk root at a
   zero relative transform.
3. Assign the same `UCubusMaterialRegistry` used by the block world.
4. Set the block world's `ChunkActorClass` to the Blueprint subclass.
5. Leave the original block mesh visible to compare both renderers, or hide the
   root block mesh when testing density-only rendering.

At runtime the density component defers its automatic build by one tick. This
allows the block world to finish configuring and generating the chunk after
`SpawnActor` returns.

## Coordinate convention

The block adapter treats each block-cell centre as a density sample. Its sample
offset is `(0.5, 0.5, 0.5)` voxel units, so the zero crossing between a solid
cell and an empty cell lands on the existing block boundary.

A density chunk owns Marching Cubes cells with lower sample coordinates
`0..31`. Those cells need corner samples `0..32`. Smooth normals additionally
need a one-sample halo, producing the buffered local range `-1..33`.

All lookups use global voxel coordinates. Adjacent chunks therefore calculate
identical positions on their shared boundary.

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
