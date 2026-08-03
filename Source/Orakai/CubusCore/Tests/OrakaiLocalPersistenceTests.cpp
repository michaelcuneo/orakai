#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Persistence/OrakaiLocalPersistenceBackend.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"

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

        FOrakaiWorldObjectRecord TreeTombstone;
        TreeTombstone.ObjectId =
            OrakaiPersistence::MakeGeneratedWorldObjectId(
                TestWorldSeed,
                TEXT("Tree"),
                FIntVector(-34, 97, 9)
            );
        TreeTombstone.TypeId = TEXT("Tree");
        TreeTombstone.ChunkCoordinate = FIntVector(-2, 3, 0);
        TreeTombstone.bGenerated = true;
        TreeTombstone.bDestroyed = true;
        Writer.RecordWorldObject(TreeTombstone);

        FOrakaiWorldObjectRecord PlacedObject;
        PlacedObject.ObjectId = OrakaiPersistence::MakePlacedWorldObjectId();
        PlacedObject.TypeId = TEXT("Campfire");
        PlacedObject.ChunkCoordinate = FIntVector(4, -1, 2);
        PlacedObject.Transform = FTransform(
            FRotator(0.0, 45.0, 0.0),
            FVector(14000.0, -2500.0, 7000.0),
            FVector(1.25)
        );
        PlacedObject.Payload = TEXT("fuel=3");
        Writer.RecordWorldObject(PlacedObject);

        // Moving a persistent object must update its spatial index instead of
        // leaving it visible in both the old and new chunk.
        PlacedObject.ChunkCoordinate = FIntVector(5, -1, 2);
        Writer.RecordWorldObject(PlacedObject);

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

    TArray<FOrakaiWorldObjectRecord> TreeRecords;
    Reader.GetWorldObjectsForChunk(FIntVector(-2, 3, 0), TreeRecords);
    TestEqual(TEXT("One generated object tombstone reloads"), TreeRecords.Num(), 1);
    if (TreeRecords.Num() == 1)
    {
        TestTrue(TEXT("Generated object flag reloads"), TreeRecords[0].bGenerated);
        TestTrue(TEXT("Destroyed object flag reloads"), TreeRecords[0].bDestroyed);
        TestEqual(TEXT("Generated object type reloads"), TreeRecords[0].TypeId, FName(TEXT("Tree")));
    }

    TArray<FOrakaiWorldObjectRecord> OldChunkRecords;
    Reader.GetWorldObjectsForChunk(FIntVector(4, -1, 2), OldChunkRecords);
    TestEqual(TEXT("Moved object leaves its old chunk index"), OldChunkRecords.Num(), 0);

    TArray<FOrakaiWorldObjectRecord> PlacedRecords;
    Reader.GetWorldObjectsForChunk(FIntVector(5, -1, 2), PlacedRecords);
    TestEqual(TEXT("One placed object reloads in its new chunk"), PlacedRecords.Num(), 1);
    if (PlacedRecords.Num() == 1)
    {
        TestFalse(TEXT("Placed object is not generated"), PlacedRecords[0].bGenerated);
        TestFalse(TEXT("Placed object is alive"), PlacedRecords[0].bDestroyed);
        TestEqual(TEXT("Placed object payload reloads"), PlacedRecords[0].Payload, FString(TEXT("fuel=3")));
        TestTrue(
            TEXT("Placed object transform reloads"),
            PlacedRecords[0].Transform.Equals(
                FTransform(
                    FRotator(0.0, 45.0, 0.0),
                    FVector(14000.0, -2500.0, 7000.0),
                    FVector(1.25)
                )
            )
        );
    }

    Reader.Disconnect();
    IFileManager::Get().Delete(*TestPath, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrakaiGeneratedWorldObjectIdTest,
    "Orakai.Cubus.Persistence.GeneratedWorldObjectIds",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FOrakaiGeneratedWorldObjectIdTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    const FString First = OrakaiPersistence::MakeGeneratedWorldObjectId(
        99123,
        TEXT("Tree"),
        FIntVector(-2, 7, 19)
    );
    const FString Repeated = OrakaiPersistence::MakeGeneratedWorldObjectId(
        99123,
        TEXT("Tree"),
        FIntVector(-2, 7, 19)
    );
    const FString OtherSeed = OrakaiPersistence::MakeGeneratedWorldObjectId(
        99124,
        TEXT("Tree"),
        FIntVector(-2, 7, 19)
    );
    const FString OtherType = OrakaiPersistence::MakeGeneratedWorldObjectId(
        99123,
        TEXT("Rock"),
        FIntVector(-2, 7, 19)
    );
    const FString OtherCoordinate =
        OrakaiPersistence::MakeGeneratedWorldObjectId(
            99123,
            TEXT("Tree"),
            FIntVector(-2, 7, 20)
        );

    TestEqual(TEXT("Generated IDs repeat exactly"), First, Repeated);
    TestTrue(TEXT("World seed participates in generated ID"), First != OtherSeed);
    TestTrue(TEXT("Object type participates in generated ID"), First != OtherType);
    TestTrue(
        TEXT("Stable coordinate participates in generated ID"),
        First != OtherCoordinate
    );

    const FString PlacedA = OrakaiPersistence::MakePlacedWorldObjectId();
    const FString PlacedB = OrakaiPersistence::MakePlacedWorldObjectId();
    TestTrue(TEXT("Placed object IDs are unique"), PlacedA != PlacedB);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrakaiLegacyLocalDeltaCompatibilityTest,
    "Orakai.Cubus.Persistence.LegacyLocalDeltaCompatibility",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FOrakaiLegacyLocalDeltaCompatibilityTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    constexpr uint32 LegacyMagic = 0x4F52444C;
    constexpr uint32 LegacyVersion = 1;
    constexpr int64 TestWorldSeed = -9917113;
    constexpr uint32 TestGenerationVersion = 12345u;
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

    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    uint32 StoredMagic = LegacyMagic;
    uint32 StoredVersion = LegacyVersion;
    int64 StoredSeed = TestWorldSeed;
    uint32 StoredGenerationVersion = TestGenerationVersion;
    Writer << StoredMagic;
    Writer << StoredVersion;
    Writer << StoredSeed;
    Writer << StoredGenerationVersion;

    bool bHasPlayerCoordinate = false;
    FVector PlayerLocation = FVector::ZeroVector;
    float PlayerYaw = 0.0f;
    float PlayerPitch = 0.0f;
    Writer << bHasPlayerCoordinate;
    Writer << PlayerLocation;
    Writer << PlayerYaw;
    Writer << PlayerPitch;

    int32 EmptyCount = 0;
    Writer << EmptyCount; // voxel edits
    Writer << EmptyCount; // density edits
    Writer << EmptyCount; // foliage edits

    int32 InventoryCount = 1;
    FString ItemId = TEXT("Wood");
    int32 Quantity = 7;
    Writer << InventoryCount;
    Writer << ItemId;
    Writer << Quantity;
    Writer.Close();

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TestPath), true);
    TestTrue(
        TEXT("Legacy version-one delta fixture writes"),
        FFileHelper::SaveArrayToFile(Bytes, *TestPath)
    );

    FOrakaiLocalPersistenceBackend Reader;
    Reader.Connect();
    Reader.SetWorldConfig(TestWorldSeed, TestGenerationVersion);
    TestEqual(
        TEXT("Version-one inventory survives the version-two reader"),
        Reader.GetInventoryQuantity(TEXT("Wood")),
        7
    );

    TArray<FOrakaiWorldObjectRecord> WorldObjects;
    Reader.GetWorldObjectsForChunk(FIntVector::ZeroValue, WorldObjects);
    TestEqual(
        TEXT("Legacy saves begin with no world-object deltas"),
        WorldObjects.Num(),
        0
    );

    Reader.Disconnect();
    IFileManager::Get().Delete(*TestPath, false, true);
    return true;
}

#endif
