#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBlockChunkMutableOccupancyTest,
    "Orakai.Cubus.Chunks.MutableOccupancyInvalidation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusBlockChunkMutableOccupancyTest::RunTest(
    const FString& Parameters
)
{
    FCubusBlockChunkData Chunk;

    TestTrue(
        TEXT("A cleared chunk starts empty"),
        !Chunk.HasAnyOccupiedVoxel()
    );

    FCubusBlockVoxel* MutableVoxel =
        Chunk.GetVoxel(3, 4, 5);

    TestNotNull(
        TEXT("Mutable voxel access succeeds"),
        MutableVoxel
    );

    if (MutableVoxel == nullptr)
    {
        return false;
    }

    MutableVoxel->MaterialId = 7;

    TestTrue(
        TEXT("Direct generator-style mutation invalidates the empty cache"),
        Chunk.HasAnyOccupiedVoxel()
    );

    MutableVoxel = Chunk.GetVoxel(3, 4, 5);

    if (MutableVoxel == nullptr)
    {
        return false;
    }

    MutableVoxel->MaterialId = 0;

    TestTrue(
        TEXT("Direct removal invalidates the occupied cache"),
        !Chunk.HasAnyOccupiedVoxel()
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusVegetationTombstoneApplicationTest,
    "Orakai.Cubus.Chunks.VegetationTombstoneApplication",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusVegetationTombstoneApplicationTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    FCubusBlockChunkData Chunk(FIntVector(-1, 2, 0));
    FCubusVegetationInstance Tree;
    Tree.WorldVoxel = FIntVector(-3, 70, 8);
    Tree.TypeId = 3;
    Chunk.AddOrReplaceVegetationInstance(Tree);

    TestEqual(TEXT("Tree placement is present"), Chunk.GetVegetationInstances().Num(), 1);
    TestTrue(
        TEXT("A persisted tombstone removes the deterministic tree"),
        Chunk.RemoveVegetationAtWorldVoxel(Tree.WorldVoxel)
    );
    TestEqual(TEXT("Removed tree stays absent from chunk data"), Chunk.GetVegetationInstances().Num(), 0);
    return true;
}

#endif
