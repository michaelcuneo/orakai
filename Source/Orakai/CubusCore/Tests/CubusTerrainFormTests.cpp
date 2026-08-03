#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainForm.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusNaturalTerrainFormTest,
    "Orakai.Cubus.Generation.NaturalTerrainForm",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusNaturalTerrainFormTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainFormSettings Settings;
    float MaximumAdjacentHeightDelta = 0.0f;
    bool bFoundPlain = false;
    bool bFoundMountain = false;
    bool bFoundStrongDrainage = false;

    for (int32 Y = -384; Y <= 384; Y += 12)
    {
        for (int32 X = -384; X <= 384; X += 12)
        {
            const FCubusTerrainFormSample Sample = FCubusTerrainForm::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );
            const FCubusTerrainFormSample Repeated = FCubusTerrainForm::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );
            const FCubusTerrainFormSample Adjacent = FCubusTerrainForm::Sample(
                static_cast<float>(X + 1),
                static_cast<float>(Y),
                Settings
            );

            TestTrue(TEXT("Terrain height is finite"), FMath::IsFinite(Sample.Height));
            TestEqual(TEXT("Terrain sampling is deterministic"), Sample.Height, Repeated.Height);
            TestTrue(
                TEXT("Terrain region weights are normalized"),
                FMath::IsNearlyEqual(
                    Sample.PlainsWeight + Sample.RollingWeight + Sample.MountainWeight,
                    1.0f,
                    0.001f
                )
            );

            MaximumAdjacentHeightDelta = FMath::Max(
                MaximumAdjacentHeightDelta,
                FMath::Abs(Sample.Height - Adjacent.Height)
            );
            bFoundPlain |= Sample.PlainsWeight > 0.65f;
            bFoundMountain |= Sample.MountainWeight > 0.65f;
            bFoundStrongDrainage |= Sample.Drainage > 0.75f;
        }
    }

    TestTrue(TEXT("The sampled world contains coherent plains"), bFoundPlain);
    TestTrue(TEXT("The sampled world contains mountain regions"), bFoundMountain);
    TestTrue(TEXT("The sampled world contains drainage corridors"), bFoundStrongDrainage);
    TestTrue(
        TEXT("Neighbouring terrain columns remain continuous"),
        MaximumAdjacentHeightDelta < 8.0f
    );
    return true;
}

#endif
