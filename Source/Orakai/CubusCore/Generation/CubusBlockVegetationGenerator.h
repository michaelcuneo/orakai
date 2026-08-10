#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusLandmarkField.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;
class FCubusTerrainDensityField;

struct FCubusGenerationSeeds;
struct FCubusVegetationInstance;
struct FCubusVegetationRegion;
struct FCubusBiomeSample;

struct FCubusVegetationGenerationSettings
{
    bool bUseConfiguredBiomes = false;

    float ForestGroveCoverage = 1.0f;
    float ForestTreeDensity = 0.0f;
    float ForestBroadleafFraction = 0.72f;

    float WetlandTreeDensity = 0.0f;
    float WetlandReedDensity = 0.0f;

    float RockyAlpineDensity = 0.0f;

    float PlainsTreeDensity = 0.0f;
    float PlainsShrubFraction = 0.0f;
    float PlainsGroundCoverDensity = 0.0f;

    float FallbackTreeDensity = 0.012f;

    FCubusBiomeFieldSettings BiomeSettings;
    FCubusLandmarkFieldSettings LandmarkSettings;
};

/**
 * Generates deterministic vegetation placement data without rendering meshes.
 */
class ORAKAI_API FCubusBlockVegetationGenerator
{
public:
    static FCubusVegetationGenerationSettings
    CaptureGenerationSettings(
        const UCubusGeologyProfile* GeologyProfile,
        const FCubusGenerationSeeds& GenerationSeeds
    );

    static void Generate(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile,
        const FCubusTerrainDensityField* DensityField = nullptr,
        bool bGenerateWater = false,
        int32 WaterLevel = 0
    );

    /**
     * Generates deterministic tree placements directly from world-space
     * terrain columns.
     *
     * This path does not require a chunk actor or FCubusBlockChunkData and is
     * intended for long-range vegetation streaming.
     */
    static void GenerateTreesForRegion(
        const FCubusVegetationRegion& Region,
        const FCubusGenerationSeeds& GenerationSeeds,
        const FCubusVegetationGenerationSettings& GenerationSettings,
        const FCubusTerrainDensityField& DensityField,
        TArray<FCubusVegetationInstance>& OutTrees
    );

    /**
     * Generates a deterministic reduced-density tree representation for
     * long-range vegetation.
     *
     * Unlike GenerateTreesForRegion(), this does not visit every terrain column.
     * Each SampleStrideVoxels square contributes at most one representative tree.
     */
    static void GenerateFarTreesForRegion(
        const FCubusVegetationRegion& Region,
        const FCubusGenerationSeeds& GenerationSeeds,
        const FCubusVegetationGenerationSettings& GenerationSettings,
        const FCubusTerrainDensityField& DensityField,
        int32 SampleStrideVoxels,
        float DensityScale,
        TArray<FCubusVegetationInstance>& OutTrees
    );

private:
    static bool IsSpacedTreeCandidate(
        int32 WorldX,
        int32 WorldY,
        int32 Seed,
        float TargetDensity
    );

    static uint32 HashWorldColumn(
        int32 WorldX,
        int32 WorldY,
        int32 Salt
    );

    static float HashToUnitFloat(
        uint32 Hash
    );

    struct FColumnSelection
    {
        int32 TypeId = 0;
        int32 BiomeMask = 0;
        float Density = 0.0f;
        float ActivePlacementRoll = 0.0f;
    };

    static FColumnSelection ResolveColumnSelection(
        int32 WorldX,
        int32 WorldY,
        int32 VegetationSeed,
        const FCubusBiomeSample& BiomeSample,
        const FCubusVegetationGenerationSettings& Settings
    );

    static float ResolveMaximumSlopeDegrees(
        int32 TypeId
    );

    static bool IsTreeType(
        int32 TypeId
    );

    static float SampleDensitySlopeDegrees(
        int32 WorldX,
        int32 WorldY,
        const FCubusTerrainDensityField& DensityField
    );
};