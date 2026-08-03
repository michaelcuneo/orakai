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
- Plateau radius, aspect-ratio range and outline irregularity control the main
  mass and broken rim.
- Shoulder strength adds lower secondary rock masses and saddles instead of a
  single central peak.
- Gully strength cuts seeded erosion channels into the sloped faces.
- Terrace steps and terrace strength control the exposed rock benches.
- `Landmark Surface Material Id` controls the exposed surface material.

Landmarks suppress generated vegetation inside their footprint so the form
remains readable. Rivers may still cross them, and cave generation uses the
raised landmark surface when applying surface-clearance rules.

Changing these settings changes the generated baseline. Generation version 7
invalidated the radial version-six landmark caches; the current version-eight
baseline retains the weathered landmark and adds kilometre-scale mountain
ranges. The local backend loads the newest older delta store for the same world
seed, reapplies the player edits over the new baseline, and writes a migrated
store without deleting the older save.

Run the focused automation coverage with:

```text
Automation RunTests Orakai.Cubus.Generation.Landmark
```
