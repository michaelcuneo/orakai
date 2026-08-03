# Cubus Biome Landmarks

Cubus generates sparse weathered-stone mesas as the first recognisable
landmark type. The landmark field is deterministic from the world seed,
continuous across chunk borders, and shared by block and density terrain.

Landmarks are configured on the active `Cubus Geology Profile` under
`Cubus | Geology | Landmarks`:

- `Generate Landmarks` enables the field.
- `Landmark Cell Size Voxels` controls average spacing.
- `Landmark Spawn Chance` controls how many eligible cells contain a mesa.
- Minimum/maximum radius and height control silhouette scale.
- Plateau radius, terrace steps and terrace strength control the weathered
  mesa shape.
- `Landmark Surface Material Id` controls the exposed surface material.

Landmarks suppress generated vegetation inside their footprint so the form
remains readable. Rivers may still cross them, and cave generation uses the
raised landmark surface when applying surface-clearance rules.

Changing these settings changes the generated baseline. Generation version 6
invalidates older generated chunk caches. The local backend loads the newest
older delta store for the same world seed, reapplies the player edits over the
new baseline, and writes a migrated version-six store without deleting the
older save.

Run the focused automation coverage with:

```text
Automation RunTests Orakai.Cubus.Generation.Landmark
```
