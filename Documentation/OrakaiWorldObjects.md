# Orakai persistent world objects

World objects are the non-terrain half of the generated-world delta model.
Voxel and density edits remain owned by the terrain system. Interactive actors
such as rocks, pickups and constructions use `FOrakaiWorldObjectRecord` and
`AOrakaiWorldObjectActor` instead.

## Identity rules

- Generated objects use `MakeGeneratedWorldObjectId(WorldSeed, TypeId,
  StableCoordinate)`. The same seed and generator anchor always produce the
  same ID, independent of actor spawn order.
- Player-placed objects use a unique `placed:<guid>` ID.
- Generated objects are not saved while unchanged. Destroying one writes a
  tombstone.
- Player-placed objects store type, transform, owning chunk and a small payload.
  Destroying one removes its authored record because it has no generated
  baseline.

Tree harvesting continues to remove the matching instance through the existing
foliage edit path. It now also stores a deterministic `Tree` world-object
tombstone for future object replication, loot and regrowth state. The foliage
renderer, Megaplant assets, wind bridge and weather behaviour are unchanged.

## Actor workflow

Create a Blueprint derived from `AOrakaiWorldObjectActor` for a pickup, rock or
construction.

- Generated actor: call `Initialize Generated Object` after spawning. Do not
  save it until its state differs from the generated baseline.
- Player construction: call `Initialize Placed Object`; this assigns an ID and
  immediately stores the full record.
- After an authoritative transform or payload change, call `Persist World
  Object`.
- When destroyed, call `Destroy Persistent World Object`.

The actor enables movement and property replication. Persistence stays behind
`IOrakaiPersistenceBackend`; the built-in local backend implements it now, and
the SpacetimeDB backend can map the same records to a table without changing
gameplay code.

## Local store compatibility

The delta file format is version 2. Version 1 survival-loop saves still load;
their world-object collection is simply empty until new object deltas are
recorded.
