#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"

void FCubusBlockTerrainGenerator::GenerateHeightTerrain(
    FCubusBlockChunkData& Chunk,
    const int32 BaseHeight,
    const float ContinentAmplitude,
    const float ContinentFrequency,
    const