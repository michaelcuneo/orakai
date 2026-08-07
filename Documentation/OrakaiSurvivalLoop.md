# First survival interaction

The first complete local gameplay loop is deliberately small:

1. Aim at a generated broadleaf or conifer tree.
2. Left click to harvest it and receive wood.
3. Aim at terrain and right click to place one wood block.
4. Stop PIE or quit the game.
5. Start the same world seed again. The removed tree, placed block and remaining
   wood count are restored.

`AOrakaiCharacter` exposes `DoHarvestTree`, `DoPlaceWoodBlock` and
`GetWoodCount` to Blueprint. The source-only defaults bind harvest to left mouse
and placement to right mouse. Disable `bEnableSurvivalInteraction` if a
Blueprint input layer will own those actions instead.

`WoodBlockMaterialId` defaults to `6`. The active `UCubusMaterialRegistry`
should register material ID 6 as the project's wood material.

Local deltas are written to:

`Saved/Orakai/Worlds/world_<seed>_v<generation>.delta`

The file contains player-authored block edits, density edits, foliage
tombstones and inventory counts. It does not contain generated chunks.
`CubusChunkStore` remains a disposable generated-data cache.

In Density render mode, placed blocks are rendered through a sparse block edit
overlay. This allows hard-edged constructions to coexist with smooth density
terrain without block-meshing the generated world.
