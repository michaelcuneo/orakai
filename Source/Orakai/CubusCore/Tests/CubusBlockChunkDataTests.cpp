#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusBlockVoxel.h"

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

#endif
