#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusLandmarkField.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

namespace CubusLandmarkFieldTests
{
    int32 WholeChunkOffset(const int32 VoxelOffset)
    {
        return (VoxelOffset / Cubus::ChunkSize) * Cubus::ChunkSize;
    }

    int32 FindHighestSolidWorldZ(
        const FCubusBlockChunkData& Chunk,
        const int32 LocalX,
        const int32 LocalY
    )
    {
        for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
        {
            const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                LocalZ
            );

            if (Voxel != nullptr && Voxel->IsSolid())
            {
                return Chunk.GetChunkCoordinate().Z * Cubus::ChunkSize + LocalZ;
            }
        }

        return INDEX_NONE;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusLandmarkDeterminismTest,
    "Orakai.Cubus.Generation.LandmarkDeterminism",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusLandmarkDeterminismTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusLandmarkFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.Seed = 198734;
    Settings.CellSizeVoxels = 96.0f;
    Settings.SpawnChance = 1.0f;
    Settings.MinimumRadiusVoxels = 22.0f;
    Settings.MaximumRadiusVoxels = 34.0f;
    Settings.MinimumHeightVoxels = 12.0f;
    Settings.MaximumHeightVoxels = 24.0f;

    FCubusLandmarkSample StrongestSample;
    FVector2D StrongestCoordinate = FVector2D::ZeroVector;

    for (int32 Y = -192; Y <= 192; Y += 2)
    {
        for (int32 X = -192; X <= 192; X += 2)
        {
            const FCubusLandmarkSample Sample = FCubusLandmarkField::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );

            if (Sample.HeightOffset > StrongestSample.HeightOffset)
            {
                StrongestSample = Sample;
                StrongestCoordinate = FVector2D(
                    static_cast<float>(X),
                    static_cast<float>(Y)
                );
            }
        }
    }

    TestTrue(TEXT("The sampled world contains a landmark"), StrongestSample.IsInside());
    TestTrue(
        TEXT("The landmark has a recognisable vertical contribution"),
        StrongestSample.HeightOffset >= Settings.MinimumHeightVoxels * 0.8f
    );

    const FCubusLandmarkSample Repeated = FCubusLandmarkField::Sample(
        StrongestCoordinate.X,
        StrongestCoordinate.Y,
        Settings
    );
    TestEqual(
        TEXT("Landmark height is deterministic"),
        Repeated.HeightOffset,
        StrongestSample.HeightOffset
    );
    TestEqual(
        TEXT("Landmark identity is deterministic"),
        Repeated.CellCoordinate,
        StrongestSample.CellCoordinate
    );

    const FCubusLandmarkSample Adjacent = FCubusLandmarkField::Sample(
        StrongestCoordinate.X + 0.01f,
        StrongestCoordinate.Y,
        Settings
    );
    TestTrue(
        TEXT("Landmark sampling remains continuous across arbitrary chunk boundaries"),
        FMath::Abs(Adjacent.HeightOffset - StrongestSample.HeightOffset) < 1.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusLandmarkBlockDensityParityTest,
    "Orakai.Cubus.Generation.LandmarkBlockDensityParity",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusLandmarkBlockDensityParityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FCubusGenerationSeeds Seeds =
        FCubusGenerationSeeds::FromWorldSeed(9081726354ll);
    UCubusGeologyProfile* Profile = NewObject<UCubusGeologyProfile>();
    Profile->bGenerateLandmarks = true;
    Profile->LandmarkCellSizeVoxels = 64.0f;
    Profile->LandmarkSpawnChance = 1.0f;
    Profile->LandmarkMinimumRadiusVoxels = 20.0f;
    Profile->LandmarkMaximumRadiusVoxels = 26.0f;
    Profile->LandmarkMinimumHeightVoxels = 8.0f;
    Profile->LandmarkMaximumHeightVoxels = 12.0f;
    Profile->LandmarkTerraceStrength = 0.25f;
    Profile->bGenerateRivers = false;
    Profile->bGenerateBiomes = false;
    Profile->bGenerateCaves = false;

    const int32 TerrainOffsetX =
        CubusLandmarkFieldTests::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetX(Seeds.Terrain)
        );
    const int32 TerrainOffsetY =
        CubusLandmarkFieldTests::WholeChunkOffset(
            FCubusGenerationSeeds::DomainOffsetY(Seeds.Terrain)
        );
    const FCubusLandmarkFieldSettings LandmarkSettings =
        FCubusLandmarkField::MakeSettings(Profile, Seeds.Terrain);

    FIntPoint LandmarkColumn = FIntPoint::ZeroValue;
    float StrongestHeight = 0.0f;

    for (int32 WorldY = -96; WorldY <= 96; ++WorldY)
    {
        for (int32 WorldX = -96; WorldX <= 96; ++WorldX)
        {
            const FCubusLandmarkSample Sample = FCubusLandmarkField::Sample(
                static_cast<float>(WorldX + TerrainOffsetX),
                static_cast<float>(WorldY + TerrainOffsetY),
                LandmarkSettings
            );

            if (Sample.HeightOffset > StrongestHeight)
            {
                StrongestHeight = Sample.HeightOffset;
                LandmarkColumn = FIntPoint(WorldX, WorldY);
            }
        }
    }

    TestTrue(TEXT("A parity-test landmark was found"), StrongestHeight > 1.0f);

    const int32 ChunkX = FMath::FloorToInt(
        static_cast<float>(LandmarkColumn.X) /
        static_cast<float>(Cubus::ChunkSize)
    );
    const int32 ChunkY = FMath::FloorToInt(
        static_cast<float>(LandmarkColumn.Y) /
        static_cast<float>(Cubus::ChunkSize)
    );
    FCubusBlockChunkData BlockChunk(FIntVector(ChunkX, ChunkY, 0));
    BlockChunk.SetGenerationSeeds(Seeds);

    FCubusBlockTerrainGenerator::GenerateHeightTerrain(
        BlockChunk,
        0,
        0.0f,
        0.003f,
        0.0f,
        0.015f,
        0.0f,
        0.08f,
        0.0f,
        0.012f,
        0.0f,
        0.006f,
        0.08f,
        0.22f,
        24.0f,
        0.004f,
        0.0025f,
        -0.25f,
        0.18f,
        0.30f,
        0.20f,
        1,
        2,
        3,
        4,
        1000.0f,
        1000,
        false,
        0,
        5,
        Profile
    );

    FCubusTerrainDensitySettings DensitySettings;
    DensitySettings.BaseHeight = 0.0f;
    DensitySettings.ContinentAmplitude = 0.0f;
    DensitySettings.HillAmplitude = 0.0f;
    DensitySettings.DetailAmplitude = 0.0f;
    DensitySettings.RidgeAmplitude = 0.0f;
    DensitySettings.ValleyDepth = 0.0f;
    DensitySettings.TerrainOffsetX = TerrainOffsetX;
    DensitySettings.TerrainOffsetY = TerrainOffsetY;
    DensitySettings.LandmarkSettings = LandmarkSettings;

    const FCubusTerrainDensityField DensityField(DensitySettings);
    const int32 LocalX = LandmarkColumn.X - ChunkX * Cubus::ChunkSize;
    const int32 LocalY = LandmarkColumn.Y - ChunkY * Cubus::ChunkSize;
    const int32 BlockSurfaceWorldZ =
        CubusLandmarkFieldTests::FindHighestSolidWorldZ(
            BlockChunk,
            LocalX,
            LocalY
        );
    const int32 DensitySurfaceWorldZ = FMath::RoundToInt(
        DensityField.SampleSurfaceVoxelHeight(
            static_cast<float>(LandmarkColumn.X),
            static_cast<float>(LandmarkColumn.Y)
        )
    );

    TestEqual(
        TEXT("Block and density terrain agree on landmark height"),
        DensitySurfaceWorldZ,
        BlockSurfaceWorldZ
    );
    return true;
}

#endif
