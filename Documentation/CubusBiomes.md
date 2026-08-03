# Cubus natural biomes

Cubus now derives terrain, surface materials and vegetation from shared,
deterministic geography instead of treating a biome as a single noise value
painted onto an already-generated surface.

## Generation order

1. `FCubusTerrainForm` creates domain-warped land masses, plains, rolling
   country, kilometre-scale mountain ranges and erosion-smoothed drainage
   valleys. Mountain ranges follow connected warped plate boundaries, with
   broad foothills, several major ridges and varying peaks along the spine.
2. `FCubusBiomeField` samples moisture, temperature, elevation, slope and the
   shared waterway field.
3. Block and density terrain select the same plains, forest, rocky and wetland
   surface materials.
4. Density terrain transitions from surface material to soil and then deep
   rock. Block terrain continues to use the geology profile's strata.
5. Vegetation uses the same biome sample. Trees are selected from a jittered
   spacing grid and habitat strength, while ground cover follows moisture.

The field is data-driven at the material boundary: biome material IDs are not
limited to the original first five materials, and density vertices preserve
the selected IDs through the unified density material payload.

`Biome Definitions` may contain any number of client-authored biomes. Each
definition selects a broad vegetation archetype while supplying its own
surface material, moisture/temperature targets, elevation range, slope limit
and priority. Leaving the array empty preserves the original four-biome setup.

## Enabling it

On the geology profile assigned to `CubusBlockWorldActor`:

- enable `Generate Biomes`;
- enable `Generate Rivers` for waterways and wetlands;
- assign distinct plains, forest, rocky and wetland surface material IDs;
- tune the existing forest, plains, rocky and wetland vegetation densities.

The existing terrain shape controls remain authoritative. Their meaning is
hierarchical: continent settings control regional relief, hills control
rolling country, ridge amplitude controls range height and ridge frequency
controls local crests within each range. The range footprint itself is derived
at one tenth of the region frequency, preventing mountain country from fitting
inside one or two chunks. Valley settings control the main drainage and
tributary network.

Generated chunk cache version 9 invalidates the old uniform-detail baseline.
Persistent player edits remain separate and are migrated forward, then
reapplied after the new baseline loads.

Local terrain relief is deliberately heterogeneous. Broad soil undulation is
present even in plains, while rolling and mountain country receive patchy
broken ground, warped metre-scale relief and intermittent erosion rills.
Floodplains suppress these layers so waterways remain readable instead of
becoming uniformly noisy.
