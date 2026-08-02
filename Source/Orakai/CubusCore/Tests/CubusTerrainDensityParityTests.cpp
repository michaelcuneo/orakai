#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

namespace CubusTerrainDensityParityTests
{
    int32 WholeChunkOffset(const int32 VoxelOffset)
    {
        return
            (VoxelOffset / Cubus::ChunkSize) *
            Cubus::ChunkSize;
    }

    int32 FindHighestSolidWorldZ(
        const FCubusBlockChunkData& Chunk,
        const int32 LocalX,
        const int32 LocalY
    )
    {
        for (
            int32 LocalZ = Cubus::ChunkSize - 1;
            LocalZ >= 0;
            --LocalZ
        )
        {
            const FCubusBlockVoxel* Voxel =
                Chunk.GetVoxel(
                    LocalX,
                    LocalY,
                    LocalZ
                );

            if (
                Voxel != nullptr &&
                Voxel->IsSolid()
            )
            {
                return
                    Chunk.GetChunkCoordinate().Z *
                        Cubus::ChunkSize +
                    LocalZ;
            }
        }

        return INDEX_NONE;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityBlockParityTest,
    "Orakai.Cubus.Density.NativeTerrain.BlockGeneratorParity",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityBlockParityTest::RunTest(
    const FString& Parameters
)
{
    constexpr int32 BaseHeight = 8;
    constexpr float ContinentAmplitude = 2.0f;
    constexpr float ContinentFrequency = 0.003f;
    constexpr float HillAmplitude = 1.5f;
    constexpr float HillFrequency = 0.015f;
    constexpr float DetailAmplitude = 0.5f;
    constexpr float DetailFrequency = 0.08f;
    constexpr float RidgeAmplitude = 1.0f;
    constexpr float RidgeFrequency = 0.012f;
    constexpr float ValleyDepth = 1.0f;
    constexpr float ValleyFrequency = 0.006f;
    constexpr float ValleyWidth = 0.08f;
    constexpr float ValleyFalloff = 0.22f;
    constexpr float ValleyWarpAmplitude = 24.0f;
    constexpr float ValleyWarpFrequency = 0.004f;
    constexpr float RegionFrequency = 0.0025f;
    constexpr float PlainsThreshold = -0.25f;
    constexpr float PlainsBlend = 0.18f;
    constexpr float MountainThreshold = 0.30f;
    constexpr float MountainBlend = 0.20f;

    const FCubusGenerationSeeds Seeds =
        FCubusGenerationSeeds::FromWorldSeed(734921);

    FCubusBlockChunkData BlockChunk(
        FIntVector::ZeroValue
    );
    BlockChunk.SetGenerationSeeds(Seeds);

    FCubusBlockTerrainGenerator::GenerateHeightTerrain(
        BlockChunk,
        BaseHeight,
        ContinentAmplitude,
        ContinentFrequency,
        HillAmplitude,
        HillFrequency,
        DetailAmplitude,
        DetailFrequency,
        RidgeAmplitude,
        RidgeFrequency,
        ValleyDepth,
        ValleyFrequency,
        ValleyWidth,
        ValleyFalloff,
        ValleyWarpAmplitude,
        ValleyWarpFrequency,
        RegionFrequency,
        PlainsThreshold,
        PlainsBlend,
        MountainThreshold,
        MountainBlend,
        1,
        2,
        3,
        4,
        1000.0f,
        1000,
        false,
        0,
        5
    );

    FCubusTerrainDensitySettings DensitySettings;
    DensitySettings.BaseHeight =
        static_cast<float>(BaseHeight);
    DensitySettings.ContinentAmplitude =
        ContinentAmplitude;
    DensitySettings.ContinentFrequency =
        ContinentFrequency;
    DensitySettings.HillAmplitude =
        HillAmplitude;
    DensitySettings.HillFrequency =
        HillFrequency;
    DensitySettings.DetailAmplitude =
        DetailAmplitude;
    DensitySettings.DetailFrequency =
        DetailFrequency;
    DensitySettings.RidgeAmplitude =
        RidgeAmplitude;
    DensitySettings.RidgeFrequency =
        RidgeFrequency;
    DensitySettings.ValleyDepth =
        ValleyDepth;
    DensitySettings.ValleyFrequency =
        ValleyFrequency;
    DensitySettings.ValleyWidth =
        ValleyWidth;
    DensitySettings.ValleyFalloff =
        ValleyFalloff;
    DensitySettings.ValleyWarpAmplitude =
        ValleyWarpAmplitude;
    DensitySettings.ValleyWarpFrequency =
        ValleyWarpFrequency;
    DensitySettings.RegionFrequency =
        RegionFrequency;
    DensitySettings.PlainsThreshold =
        PlainsThreshold;
    DensitySettings.PlainsBlend =
        PlainsBlend;
    DensitySettings.MountainThreshold =
        MountainThreshold;
    DensitySettings.MountainBlend =
        MountainBlend;

    DensitySettings.TerrainOffsetX =
        CubusTerrainDensityParityTests::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetX(
                Seeds.Terrain
            )
        );

    DensitySettings.TerrainOffsetY =
        CubusTerrainDensityParityTests::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetY(
                Seeds.Terrain
            )
        );

    const FCubusTerrainDensityField DensityField(
        DensitySettings
    );

    const FIntPoint TestColumns[] =
    {
        FIntPoint(0, 0),
        FIntPoint(3, 11),
        FIntPoint(15, 16),
        FIntPoint(24, 7),
        FIntPoint(31, 31)
    };

    for (const FIntPoint& Column : TestColumns)
    {
        const int32 BlockSurfaceWorldZ =
            CubusTerrainDensityParityTests::FindHighestSolidWorldZ(
                BlockChunk,
                Column.X,
                Column.Y
            );

        const int32 DensityRoundedSurfaceWorldZ =
            FMath::RoundToInt(
                DensityField.SampleSurfaceVoxelHeight(
                    static_cast<float>(Column.X),
                    static_cast<float>(Column.Y)
                )
            );

        TestEqual(
            FString::Printf(
                TEXT("Block and density terrain agree at column (%d, %d)"),
                Column.X,
                Column.Y
            ),
            DensityRoundedSurfaceWorldZ,
            BlockSurfaceWorldZ
        );
    }

    return true;
}

#endif
