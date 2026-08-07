#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Meshing/CubusMarchingCubesTables.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusMarchingCubesTableTest,
    "Orakai.Cubus.Density.MarchingCubes.AllTableCases",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusMarchingCubesTableTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    for (int32 CaseIndex = 0; CaseIndex < 256; ++CaseIndex)
    {
        bool bReachedTerminator = false;
        int32 EdgeCount = 0;

        for (int32 EntryIndex = 0; EntryIndex < 16; ++EntryIndex)
        {
            const int8 EdgeIndex =
                CubusMarchingCubesTables::GetTriangleEdge(
                    CaseIndex,
                    EntryIndex
                );

            if (EdgeIndex < 0)
            {
                bReachedTerminator = true;
                continue;
            }

            TestFalse(
                FString::Printf(
                    TEXT("Case %d has an edge after its terminator"),
                    CaseIndex
                ),
                bReachedTerminator
            );

            TestTrue(
                FString::Printf(
                    TEXT("Case %d entry %d references edge %d"),
                    CaseIndex,
                    EntryIndex,
                    static_cast<int32>(EdgeIndex)
                ),
                EdgeIndex >= 0 &&
                EdgeIndex < 12
            );

            ++EdgeCount;
        }

        TestEqual(
            FString::Printf(
                TEXT("Case %d contains complete triangle triplets"),
                CaseIndex
            ),
            EdgeCount % 3,
            0
        );
    }

    return true;
}

#endif
