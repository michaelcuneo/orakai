#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Persistence/OrakaiLocalPersistenceBackend.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrakaiLocalDeltaRoundTripTest,
    "Orakai.Cubus.Persistence.LocalDeltaRoundTrip",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FOrakaiLocalDeltaRoundTripTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    constexpr int64 TestWorldSeed = -771293401;
    constexpr uint32 TestGenerationVersion = 987654u;
    const FString TestPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Orakai"),
        TEXT("Worlds"),
        FString::Printf(
            TEXT("world_%lld_v%u.delta"),
            static_cast<long long>(TestWorldSeed),
            TestGenerationVersion
        )
    );

    IFileManager::Get().Delete(*TestPath, false, true);

    {
        FOrakaiLocalPersistenceBackend Writer;
        Writer.Connect();
        Writer.SetWorldConfig(TestWorldSeed, TestGenerationVersion);

        FOrakaiVoxelEdit VoxelEdit;
        VoxelEdit.ChunkCoordinate = FIntVector(-2, 3, 1);
        VoxelEdit.LocalCoordinate = FIntVector(31, 0, 7);
        VoxelEdit.MaterialId = 6;
        Writer.RecordVoxelEdit(VoxelEdit);

        FOrakaiDensityEdit DensityEdit;
        DensityEdit.WorldSample = FIntVector(-33, 96, 8);
        DensityEdit.DensityDelta = -2.5f;
        DensityEdit.MaterialId = 0;
        Writer.RecordDensityEdit(DensityEdit);

        FOrakaiFoliageEdit FoliageEdit;
        FoliageEdit.WorldVoxel = FIntVector(-34, 97, 9);
        FoliageEdit.bRemoved = true;
        Writer.RecordFoliageEdit(FoliageEdit);

        Writer.SetInventoryQuantity(TEXT("Wood"), 3);
        Writer.Disconnect();
    }

    FOrakaiLocalPersistenceBackend Reader;
    Reader.Connect();
    Reader.SetWorldConfig(TestWorldSeed, TestGenerationVersion);

    TArray<FOrakaiVoxelEdit> VoxelEdits;
    Reader.GetVoxelEditsForChunk(FIntVector(-2, 3, 1), VoxelEdits);
    TestEqual(TEXT("One block delta reloads"), VoxelEdits.Num(), 1);
    if (VoxelEdits.Num() == 1)
    {
        TestEqual(TEXT("Block material reloads"), VoxelEdits[0].MaterialId, 6);
    }

    TArray<FOrakaiDensityEdit> DensityEdits;
    Reader.GetDensityEdits(DensityEdits);
    TestEqual(TEXT("One density delta reloads"), DensityEdits.Num(), 1);
    if (DensityEdits.Num() == 1)
    {
        TestEqual(TEXT("Density amount reloads"), DensityEdits[0].DensityDelta, -2.5f);
    }

    TArray<FOrakaiFoliageEdit> FoliageEdits;
    Reader.GetFoliageEditsForChunk(FIntVector(-2, 3, 0), FoliageEdits);
    TestEqual(TEXT("One tree tombstone reloads"), FoliageEdits.Num(), 1);
    if (FoliageEdits.Num() == 1)
    {
        TestTrue(TEXT("Reloaded foliage edit is a tombstone"), FoliageEdits[0].bRemoved);
    }

    TestEqual(TEXT("Wood inventory reloads"), Reader.GetInventoryQuantity(TEXT("Wood")), 3);

    Reader.Disconnect();
    IFileManager::Get().Delete(*TestPath, false, true);
    return true;
}

#endif
