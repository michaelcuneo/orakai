from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one anchor, found {count}: {old[:140]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')

world_h = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.h'
world_cpp = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
density_h = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h'
density_cpp = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp'

# ---------------------------------------------------------------------------
# Streaming startup: support-first, resolution-aware async builds.
# ---------------------------------------------------------------------------
replace_once(
    world_h,
    '''\tUE::Tasks::TTask<FCubusDensityMeshBuildResult> Task;\n};\n''',
    '''\tUE::Tasks::TTask<FCubusDensityMeshBuildResult> Task;\n\n\t// Resolution captured by this worker. If LOD changes while the task is\n\t// running, the result is discarded instead of publishing stale geometry.\n\tint32 SubdivisionsPerVoxel = 1;\n};\n'''
)

replace_once(
    world_cpp,
    '''\tStreamingTerrainSettings.BaseHeight = static_cast<float>(TerrainBaseHeight);\n''',
    '''\tStreamingTerrainSettings.BaseHeight = static_cast<float>(TerrainBaseHeight);\n\tStreamingTerrainSettings.VoxelSizeCm = GeneratedVoxelSize;\n'''
)

replace_once(
    world_cpp,
    '''\tif (bHasTrackedChunk)\n\t{\n\t\tconst int32 Distance = FCubusDensityLod::ChunkDistance(ChunkCoordinate, LastTrackedChunk);\n\n\t\tif (Distance <= DensityNearChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityNearSampleSpacing;\n\t\t}\n\t\telse if (Distance <= DensityMiddleChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityMiddleSampleSpacing;\n\t\t}\n\t}\n\n\treturn FCubusDensityLod::ResolveSubdivisionsForSpacing(GeneratedVoxelSize, TargetSpacing);\n''',
    '''\tif (bHasTrackedChunk)\n\t{\n\t\tconst int32 Distance = FCubusDensityLod::ChunkDistance(ChunkCoordinate, LastTrackedChunk);\n\n\t\tif (Distance <= DensityNearChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityNearSampleSpacing;\n\t\t}\n\t\telse if (Distance <= DensityMiddleChunkRadius)\n\t\t{\n\t\t\tTargetSpacing = DensityMiddleSampleSpacing;\n\t\t}\n\n\t\t/*\n\t\t * Bootstrap collision must never wait for the expensive visual LOD.\n\t\t * The canonical 80 cm support mesh is enough to place the pawn safely;\n\t\t * once released, UpdateDensityLods() schedules the normal 20 cm near\n\t\t * mesh asynchronously. Generated density remains the same field.\n\t\t */\n\t\tif (bPawnHeldForStreaming && ChunkCoordinate == LastTrackedChunk)\n\t\t{\n\t\t\tTargetSpacing = GeneratedVoxelSize;\n\t\t}\n\t}\n\n\treturn FCubusDensityLod::ResolveSubdivisionsForSpacing(GeneratedVoxelSize, TargetSpacing);\n'''
)

replace_once(
    world_cpp,
    '''\t\tif (ChunkActor->ConfigureDensityResolution(ResolveDensitySubdivisions(Entry.Key)))\n\t\t{\n\t\t\tQueueChunkForRebuild(Entry.Key);\n\t\t}\n''',
    '''\t\tif (ChunkActor->ConfigureDensityResolution(ResolveDensitySubdivisions(Entry.Key)))\n\t\t{\n\t\t\t/*\n\t\t\t * Streamed density LOD changes use the existing worker pipeline.\n\t\t\t * The old path pushed them into the synchronous dirty queue, which\n\t\t\t * could freeze the game thread immediately after spawn.\n\t\t\t */\n\t\t\tif (bEnableRuntimeStreaming && ChunkActor->GetEffectiveRenderMode() == ECubusVoxelRenderMode::Density)\n\t\t\t{\n\t\t\t\tStreamingChunksReady.Remove(Entry.Key);\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\tQueueChunkForRebuild(Entry.Key);\n\t\t\t}\n\t\t}\n'''
)

replace_once(
    world_cpp,
    '''\tint32 QueuedCount = 0;\n\n\tfor (const FIntVector& Coordinate : RequiredChunkCoordinates)\n\t{\n''',
    '''\tint32 QueuedCount = 0;\n\n\t/*\n\t * TSet iteration order is deliberately unspecified. Startup previously\n\t * handed the only density worker to an arbitrary neighbour while the pawn\n\t * waited. Always schedule the support chunk first, then expand outward.\n\t */\n\tTArray<FIntVector> BuildCandidates = RequiredChunkCoordinates.Array();\n\tconst FIntVector PriorityCentre = LastTrackedChunk;\n\tBuildCandidates.Sort([PriorityCentre](const FIntVector& A, const FIntVector& B)\n\t{\n\t\tconst int32 DistanceA = FCubusDensityLod::ChunkDistance(A, PriorityCentre);\n\t\tconst int32 DistanceB = FCubusDensityLod::ChunkDistance(B, PriorityCentre);\n\t\tif (DistanceA != DistanceB)\n\t\t{\n\t\t\treturn DistanceA < DistanceB;\n\t\t}\n\t\tconst int32 VerticalA = FMath::Abs(A.Z - PriorityCentre.Z);\n\t\tconst int32 VerticalB = FMath::Abs(B.Z - PriorityCentre.Z);\n\t\tif (VerticalA != VerticalB)\n\t\t{\n\t\t\treturn VerticalA < VerticalB;\n\t\t}\n\t\tif (A.Y != B.Y)\n\t\t{\n\t\t\treturn A.Y < B.Y;\n\t\t}\n\t\treturn A.X < B.X;\n\t});\n\n\tfor (const FIntVector& Coordinate : BuildCandidates)\n\t{\n'''
)

replace_once(
    world_cpp,
    '''\t\tBuild.Coordinate = Coordinate;\n\t\tBuild.Chunk\t\t = Chunk;\n\n\t\tBuild.Task = UE::Tasks::Launch(TEXT("CubusStreamingDensityMesh"),\n''',
    '''\t\tBuild.Coordinate = Coordinate;\n\t\tBuild.Chunk\t\t = Chunk;\n\t\tBuild.SubdivisionsPerVoxel = BuildInput.SubdivisionsPerVoxel;\n\n\t\tBuild.Task = UE::Tasks::Launch(TEXT("CubusStreamingDensityMesh"),\n'''
)

replace_once(
    world_cpp,
    '''void ACubusBlockWorldActor::ProcessInitialStreaming()\n{\n\tif (bInitialSpawnAreaReady)\n\t{\n\t\treturn;\n\t}\n\n\tif (!AreInitialChunksReady())\n\t{\n\t\treturn;\n\t}\n\n\tbInitialSpawnAreaReady = true;\n\n\tUE_LOG(LogTemp, Display, TEXT("Cubus initial spawn area ready: %d chunks"), InitialRequiredCoordinates.Num());\n\n\tUpdateRuntimeStreaming(true);\n}\n''',
    '''void ACubusBlockWorldActor::ProcessInitialStreaming()\n{\n\tif (bInitialSpawnAreaReady)\n\t{\n\t\treturn;\n\t}\n\n\tconst bool bHasSupportCoordinate =\n\t\tLastTrackedChunk.X != MAX_int32 &&\n\t\tLastTrackedChunk.Y != MAX_int32 &&\n\t\tLastTrackedChunk.Z != MAX_int32;\n\tif (!bHasSupportCoordinate || !IsInitialChunkReady(LastTrackedChunk))\n\t{\n\t\treturn;\n\t}\n\n\t/*\n\t * Spawn readiness means the support surface is safe, not that every\n\t * surrounding visual chunk has finished. The rest continues streaming\n\t * after the pawn is released.\n\t */\n\tbInitialSpawnAreaReady = true;\n\n\tUE_LOG(LogTemp, Display,\n\t\tTEXT("Cubus spawn support ready at (%d, %d, %d); surrounding chunks continue streaming"),\n\t\tLastTrackedChunk.X, LastTrackedChunk.Y, LastTrackedChunk.Z);\n\n\tUpdateRuntimeStreaming(true);\n}\n'''
)

replace_once(
    world_cpp,
    '''\t\tconst bool bStillRequired = RequiredChunkCoordinates.Contains(Build.Coordinate);\n\n\t\tif (IsValid(Chunk) && bStillRequired)\n\t\t{\n\t\t\tif (Chunk->BuildStagedVolumeFromDensityMesh(Result))\n\t\t\t{\n\t\t\t\tChunk->CommitStagedVolume();\n\n\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t}\n\t\t}\n''',
    '''\t\tconst bool bStillRequired = RequiredChunkCoordinates.Contains(Build.Coordinate);\n\t\tconst bool bResolutionStillCurrent =\n\t\t\tIsValid(Chunk) &&\n\t\t\tChunk->GetDensitySubdivisionsPerVoxel() == Build.SubdivisionsPerVoxel;\n\n\t\tif (IsValid(Chunk) && bStillRequired && bResolutionStillCurrent)\n\t\t{\n\t\t\tif (Chunk->BuildStagedVolumeFromDensityMesh(Result))\n\t\t\t{\n\t\t\t\tChunk->CommitStagedVolume();\n\n\t\t\t\tStreamingChunksReady.Add(Build.Coordinate);\n\t\t\t}\n\t\t}\n'''
)

# Spawn trace follows the generated support altitude, not the pawn's old Z.
replace_once(
    world_cpp,
    '''\tconst float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * FMath::Max(1.0f, GeneratedVoxelSize);\n\n\tconst FVector TraceStart(HeldPawnLocation.X, HeldPawnLocation.Y, HeldPawnLocation.Z + ChunkWorldSize * 4.0f);\n\n\tconst FVector TraceEnd(HeldPawnLocation.X, HeldPawnLocation.Y, HeldPawnLocation.Z - ChunkWorldSize * 8.0f);\n''',
    '''\tconst float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * FMath::Max(1.0f, GeneratedVoxelSize);\n\n\t/*\n\t * Physical terrain may be hundreds or thousands of metres above/below the\n\t * level-authored pawn. Trace through the support chunk we actually built,\n\t * rather than a small fixed window around the pawn's obsolete Z.\n\t */\n\tconst ACubusVoxelVolumeActor* SupportChunk = FindChunk(LastTrackedChunk);\n\tconst double SupportCentreZ = IsValid(SupportChunk)\n\t\t? SupportChunk->GetActorLocation().Z\n\t\t: static_cast<double>(LastTrackedChunk.Z) * static_cast<double>(ChunkWorldSize);\n\tconst FVector TraceStart(\n\t\tHeldPawnLocation.X,\n\t\tHeldPawnLocation.Y,\n\t\tSupportCentreZ + static_cast<double>(ChunkWorldSize));\n\tconst FVector TraceEnd(\n\t\tHeldPawnLocation.X,\n\t\tHeldPawnLocation.Y,\n\t\tSupportCentreZ - static_cast<double>(ChunkWorldSize));\n'''
)

replace_once(
    world_cpp,
    '''\t\tconst FIntVector HeldChunkCoordinate = WorldLocationToChunkCoordinate(HeldPawnLocation);\n''',
    '''\t\t// Recovery follows the deterministic terrain support coordinate. The\n\t\t// held pawn's original Z is not meaningful in a kilometre-relief world.\n\t\tconst FIntVector HeldChunkCoordinate = LastTrackedChunk;\n'''
)

# ---------------------------------------------------------------------------
# Density sampling: fine geometry, canonical-rate ecology/material lookup.
# ---------------------------------------------------------------------------
replace_once(
    density_h,
    '''    struct FColumnData\n    {\n        float SurfaceVoxelHeight = 0.0f;\n        float SurfaceSampleZ = 1.0f;\n        float Slope = 0.0f;\n        FVector2D Gradient = FVector2D::ZeroVector;\n        float RockHardness = 0.5f;\n        float Fracture = 0.0f;\n        float StrataTilt = 0.0f;\n        float RockExposure = 0.0f;\n        FCubusTerrainFormSample FormSample;\n        FCubusBiomeSample BiomeSample;\n        int32 SurfaceMaterialId = 1;\n    };\n''',
    '''    struct FColumnData\n    {\n        float SurfaceVoxelHeight = 0.0f;\n        float SurfaceSampleZ = 1.0f;\n        float Slope = 0.0f;\n        FVector2D Gradient = FVector2D::ZeroVector;\n        float RockHardness = 0.5f;\n        float Fracture = 0.0f;\n        float StrataTilt = 0.0f;\n        float RockExposure = 0.0f;\n        FCubusTerrainFormSample FormSample;\n    };\n\n    struct FSurfaceMaterialData\n    {\n        FCubusBiomeSample BiomeSample;\n        int32 SurfaceMaterialId = 1;\n    };\n'''
)

replace_once(
    density_h,
    '''    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;\n    mutable TMap<FIntPoint, FColumnData> ColumnCache;\n    mutable TMap<FIntPoint, FCubusBiomeClimateContext> BiomeClimateCache;\n''',
    '''    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;\n    mutable TMap<FIntPoint, FColumnData> ColumnCache;\n    // Ecology/materials do not need the 20 cm geometry lattice. One canonical\n    // material classification per XY cell is sufficient and avoids thousands\n    // of expensive climate/community evaluations per adaptive chunk.\n    mutable TMap<FIntPoint, FSurfaceMaterialData> SurfaceMaterialCache;\n    mutable TMap<FIntPoint, FCubusBiomeClimateContext> BiomeClimateCache;\n'''
)

replace_once(
    density_h,
    '''    const FSurfaceData& GetCachedSurfaceData(float WorldX, float WorldY) const;\n    float GetCachedSurfaceVoxelHeight(float WorldX, float WorldY) const;\n    const FColumnData& GetColumnData(float WorldX, float WorldY) const;\n''',
    '''    const FSurfaceData& GetCachedSurfaceData(float WorldX, float WorldY) const;\n    float GetCachedSurfaceVoxelHeight(float WorldX, float WorldY) const;\n    const FColumnData& GetColumnData(float WorldX, float WorldY) const;\n    const FSurfaceMaterialData& GetCachedSurfaceMaterialData(float WorldX, float WorldY) const;\n'''
)

replace_once(
    density_cpp,
    '''\tSurfaceCache.Reserve(1600);\n\tColumnCache.Reserve(1600);\n\tBiomeClimateCache.Reserve(96);\n''',
    '''\tSurfaceCache.Reserve(1600);\n\tColumnCache.Reserve(1600);\n\tSurfaceMaterialCache.Reserve(1600);\n\tBiomeClimateCache.Reserve(96);\n'''
)

replace_once(
    density_cpp,
    '''FCubusBiomeSample FCubusTerrainDensityField::SampleSurfaceBiome(const float WorldX, const float WorldY) const\n{\n\treturn GetColumnData(WorldX + 0.5f, WorldY + 0.5f).BiomeSample;\n}\n''',
    '''FCubusBiomeSample FCubusTerrainDensityField::SampleSurfaceBiome(const float WorldX, const float WorldY) const\n{\n\treturn GetCachedSurfaceMaterialData(WorldX + 0.5f, WorldY + 0.5f).BiomeSample;\n}\n'''
)

old_material_block = '''\n\tFCubusBiomeTerrainContext BiomeTerrainContext;\n\tBiomeTerrainContext.Drainage = Column.FormSample.Drainage;\n\tBiomeTerrainContext.RockExposure = Column.RockExposure;\n\tBiomeTerrainContext.MountainCore = Column.FormSample.MountainCore;\n\tBiomeTerrainContext.FoothillWeight = Column.FormSample.FoothillWeight;\n\tBiomeTerrainContext.Ridge = Column.FormSample.Ridge;\n\tBiomeTerrainContext.Gradient = Column.Gradient;\n\tBiomeTerrainContext.bHasTerrainFormSample = true;\n\tBiomeTerrainContext.TerrainFormSample = Column.FormSample;\n\tBiomeTerrainContext.bHasHydrologySample = Surface.bHasHydrologySample;\n\tBiomeTerrainContext.HydrologySample = Surface.HydrologySample;\n\tBiomeTerrainContext.bHasSubstrateSample = true;\n\tBiomeTerrainContext.SubstrateHardness = Column.RockHardness;\n\tBiomeTerrainContext.FractureDensity = Column.Fracture;\n\tBiomeTerrainContext.bHasTopographicClimateSample = true;\n\tBiomeTerrainContext.TopographicClimateSample = GetInterpolatedBiomeTopographicClimate(WorldX, WorldY);\n\n\tconst FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);\n\tColumn.BiomeSample = FCubusBiomeField::Sample(\n\t\tWorldX,\n\t\tWorldY,\n\t\tColumn.SurfaceVoxelHeight,\n\t\tColumn.Slope,\n\t\tSettings.BiomeSettings,\n\t\tBiomeTerrainContext,\n\t\t&Climate\n\t);\n\n\tconst FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);\n\tif (LandmarkSample.IsInside())\n\t{\n\t\tColumn.SurfaceMaterialId = FMath::Max(1, Settings.LandmarkSettings.SurfaceMaterialId);\n\t}\n\telse\n\t{\n\t\tColumn.SurfaceMaterialId = Column.Slope >= Settings.RockSlopeThreshold\n\t\t\t? Settings.RockMaterialId\n\t\t\t: (Settings.BiomeSettings.bEnabled ? Column.BiomeSample.SurfaceMaterialId : Settings.SurfaceMaterialId);\n\n\t\t/*\n\t\t * Snow is an alpine altitude band. Climate may describe the ecology,\n\t\t * but it is never allowed to put snow on low ground. The structural\n\t\t * floor scales with the authored mountain amplitude so legacy worlds\n\t\t * carrying the old 34 m default cannot accidentally become snowy\n\t\t * lowlands after the mountain vertical scale is increased.\n\t\t */\n\t\tconst float StructuralSnowFloor =\n\t\t\tSettings.BaseHeight + FMath::Max(64.0f, Settings.RidgeAmplitude * 4.0f);\n\t\tconst float ConfiguredSnowLine = Settings.BiomeSettings.bEnabled\n\t\t\t? Settings.BiomeSnowMinimumHeight\n\t\t\t: Settings.SnowMinimumHeight;\n\t\tconst float SnowLine = FMath::Max(ConfiguredSnowLine, StructuralSnowFloor);\n\t\tconst float SnowRetention = 1.0f - SmoothStep(\n\t\t\tSettings.RockSlopeThreshold * 0.95f,\n\t\t\tFMath::Max(Settings.RockSlopeThreshold * 1.70f, Settings.RockSlopeThreshold + 0.01f),\n\t\t\tColumn.Slope\n\t\t);\n\n\t\tif (Column.SurfaceVoxelHeight >= SnowLine && SnowRetention >= 0.30f)\n\t\t{\n\t\t\tColumn.SurfaceMaterialId = Settings.BiomeSettings.bEnabled\n\t\t\t\t? Settings.BiomeSnowMaterialId\n\t\t\t\t: Settings.SnowMaterialId;\n\t\t}\n\t}\n'''
replace_once(density_cpp, old_material_block, '\n')

# Insert canonical-rate material/ecology cache after geometry column cache.
anchor = '''\tColumnCache.Add(Key, Column);\n\treturn ColumnCache.FindChecked(Key);\n}\n\nconst FCubusBiomeClimateContext& FCubusTerrainDensityField::GetCachedBiomeClimateCell(\n'''
material_fn = '''\tColumnCache.Add(Key, Column);\n\treturn ColumnCache.FindChecked(Key);\n}\n\nconst FCubusTerrainDensityField::FSurfaceMaterialData& FCubusTerrainDensityField::GetCachedSurfaceMaterialData(\n\tconst float WorldSampleX,\n\tconst float WorldSampleY\n) const\n{\n\t/*\n\t * Material/ecology classification is intentionally canonical-rate. Fine\n\t * 20 cm density samples still evaluate the true continuous geometry, but\n\t * neighbouring samples share the same 80 cm ecological decision. Biome\n\t * geography varies at metre-to-kilometre scales, so no meaningful visual\n\t * information is lost by refusing to classify it 16x per canonical cell.\n\t */\n\tconst FIntPoint Key(FMath::RoundToInt(WorldSampleX), FMath::RoundToInt(WorldSampleY));\n\tif (const FSurfaceMaterialData* Existing = SurfaceMaterialCache.Find(Key))\n\t{\n\t\treturn *Existing;\n\t}\n\n\tconst float CanonicalSampleX = static_cast<float>(Key.X);\n\tconst float CanonicalSampleY = static_cast<float>(Key.Y);\n\tconst FSurfaceData Surface = GetCachedSurfaceData(CanonicalSampleX, CanonicalSampleY);\n\tconst FColumnData& Column = GetColumnData(CanonicalSampleX, CanonicalSampleY);\n\tconst float WorldX = CanonicalSampleX - 0.5f;\n\tconst float WorldY = CanonicalSampleY - 0.5f;\n\tconst float TerrainX = WorldX + static_cast<float>(Settings.TerrainOffsetX);\n\tconst float TerrainY = WorldY + static_cast<float>(Settings.TerrainOffsetY);\n\n\tFCubusBiomeTerrainContext BiomeTerrainContext;\n\tBiomeTerrainContext.Drainage = Column.FormSample.Drainage;\n\tBiomeTerrainContext.RockExposure = Column.RockExposure;\n\tBiomeTerrainContext.MountainCore = Column.FormSample.MountainCore;\n\tBiomeTerrainContext.FoothillWeight = Column.FormSample.FoothillWeight;\n\tBiomeTerrainContext.Ridge = Column.FormSample.Ridge;\n\tBiomeTerrainContext.Gradient = Column.Gradient;\n\tBiomeTerrainContext.bHasTerrainFormSample = true;\n\tBiomeTerrainContext.TerrainFormSample = Column.FormSample;\n\tBiomeTerrainContext.bHasHydrologySample = Surface.bHasHydrologySample;\n\tBiomeTerrainContext.HydrologySample = Surface.HydrologySample;\n\tBiomeTerrainContext.bHasSubstrateSample = true;\n\tBiomeTerrainContext.SubstrateHardness = Column.RockHardness;\n\tBiomeTerrainContext.FractureDensity = Column.Fracture;\n\tBiomeTerrainContext.bHasTopographicClimateSample = true;\n\tBiomeTerrainContext.TopographicClimateSample = GetInterpolatedBiomeTopographicClimate(WorldX, WorldY);\n\n\tFSurfaceMaterialData MaterialData;\n\tconst FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);\n\tMaterialData.BiomeSample = FCubusBiomeField::Sample(\n\t\tWorldX,\n\t\tWorldY,\n\t\tColumn.SurfaceVoxelHeight,\n\t\tColumn.Slope,\n\t\tSettings.BiomeSettings,\n\t\tBiomeTerrainContext,\n\t\t&Climate\n\t);\n\n\tconst FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);\n\tif (LandmarkSample.IsInside())\n\t{\n\t\tMaterialData.SurfaceMaterialId = FMath::Max(1, Settings.LandmarkSettings.SurfaceMaterialId);\n\t}\n\telse\n\t{\n\t\tMaterialData.SurfaceMaterialId = Column.Slope >= Settings.RockSlopeThreshold\n\t\t\t? Settings.RockMaterialId\n\t\t\t: (Settings.BiomeSettings.bEnabled ? MaterialData.BiomeSample.SurfaceMaterialId : Settings.SurfaceMaterialId);\n\n\t\tconst float StructuralSnowFloor =\n\t\t\tSettings.BaseHeight + FMath::Max(64.0f, Settings.RidgeAmplitude * 4.0f);\n\t\tconst float ConfiguredSnowLine = Settings.BiomeSettings.bEnabled\n\t\t\t? Settings.BiomeSnowMinimumHeight\n\t\t\t: Settings.SnowMinimumHeight;\n\t\tconst float SnowLine = FMath::Max(ConfiguredSnowLine, StructuralSnowFloor);\n\t\tconst float SnowRetention = 1.0f - SmoothStep(\n\t\t\tSettings.RockSlopeThreshold * 0.95f,\n\t\t\tFMath::Max(Settings.RockSlopeThreshold * 1.70f, Settings.RockSlopeThreshold + 0.01f),\n\t\t\tColumn.Slope\n\t\t);\n\n\t\tif (Column.SurfaceVoxelHeight >= SnowLine && SnowRetention >= 0.30f)\n\t\t{\n\t\t\tMaterialData.SurfaceMaterialId = Settings.BiomeSettings.bEnabled\n\t\t\t\t? Settings.BiomeSnowMaterialId\n\t\t\t\t: Settings.SnowMaterialId;\n\t\t}\n\t}\n\n\tSurfaceMaterialCache.Add(Key, MaterialData);\n\treturn SurfaceMaterialCache.FindChecked(Key);\n}\n\nconst FCubusBiomeClimateContext& FCubusTerrainDensityField::GetCachedBiomeClimateCell(\n'''
replace_once(density_cpp, anchor, material_fn)

replace_once(
    density_cpp,
    '''\tif (DepthBelowSurface <= Settings.SurfaceMaterialDepth)\n\t{\n\t\tResult.MaterialId = Column.SurfaceMaterialId;\n\t}\n''',
    '''\tif (DepthBelowSurface <= Settings.SurfaceMaterialDepth)\n\t{\n\t\tResult.MaterialId = GetCachedSurfaceMaterialData(\n\t\t\tstatic_cast<float>(GlobalSampleCoordinate.X),\n\t\t\tstatic_cast<float>(GlobalSampleCoordinate.Y)\n\t\t).SurfaceMaterialId;\n\t}\n'''
)

# Guards against accidental regression.
w = Path(world_cpp).read_text(encoding='utf-8')
wh = Path(world_h).read_text(encoding='utf-8')
d = Path(density_cpp).read_text(encoding='utf-8')
dh = Path(density_h).read_text(encoding='utf-8')
for token in [
    'TargetSpacing = GeneratedVoxelSize;',
    'Cubus spawn support ready',
    'BuildCandidates.Sort',
    'bResolutionStillCurrent',
    'SupportCentreZ',
    'StreamingChunksReady.Remove(Entry.Key);',
]:
    if token not in w:
        raise RuntimeError(f'missing streaming fix token: {token}')
if 'SubdivisionsPerVoxel = 1;' not in wh:
    raise RuntimeError('stream build resolution token missing')
for token in ['SurfaceMaterialCache', 'GetCachedSurfaceMaterialData']:
    if token not in d or token not in dh:
        raise RuntimeError(f'missing density material cache token: {token}')
if 'Column.BiomeSample' in d or 'Column.SurfaceMaterialId' in d:
    raise RuntimeError('fine geometry column still owns ecology/material classification')
