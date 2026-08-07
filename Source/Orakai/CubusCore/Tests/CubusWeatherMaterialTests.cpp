#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Weather/CubusWeatherMaterialUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusWeatherAmountNormalizationTest,
    "Orakai.Cubus.Weather.Materials.AmountNormalization",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusWeatherAmountNormalizationTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    TestTrue(
        TEXT("Unit weather values pass through"),
        FMath::IsNearlyEqual(
            FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(0.65f),
            0.65f
        )
    );
    TestTrue(
        TEXT("Ten-point weather values normalize"),
        FMath::IsNearlyEqual(
            FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(6.5f),
            0.65f
        )
    );
    TestTrue(
        TEXT("Percentage weather values normalize"),
        FMath::IsNearlyEqual(
            FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(65.0f),
            0.65f
        )
    );
    TestEqual(
        TEXT("Negative weather values clamp to dry"),
        FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(-4.0f),
        0.0f
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusWeatherWetnessLifecycleTest,
    "Orakai.Cubus.Weather.Materials.WetnessLifecycle",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusWeatherWetnessLifecycleTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    FCubusWeatherMaterialSample Rain;
    Rain.bHasRainIntensity = true;
    Rain.RainIntensity = 0.5f;

    const float Wet = FCubusWeatherMaterialUtilities::AdvanceWetness(
        0.0f,
        Rain,
        2.0f,
        0.2f,
        0.05f
    );
    TestTrue(
        TEXT("Rain accumulates wetness according to intensity"),
        FMath::IsNearlyEqual(Wet, 0.2f)
    );

    FCubusWeatherMaterialSample Dry;
    Dry.bHasRainIntensity = true;
    Dry.RainIntensity = 0.0f;

    const float Dried = FCubusWeatherMaterialUtilities::AdvanceWetness(
        Wet,
        Dry,
        2.0f,
        0.2f,
        0.05f
    );
    TestTrue(
        TEXT("Wet terrain dries after rainfall stops"),
        FMath::IsNearlyEqual(Dried, 0.1f)
    );

    FCubusWeatherMaterialSample AuthoritativeWetness;
    AuthoritativeWetness.bHasSurfaceWetness = true;
    AuthoritativeWetness.SurfaceWetness = 0.8f;

    const float Driven = FCubusWeatherMaterialUtilities::AdvanceWetness(
        0.2f,
        AuthoritativeWetness,
        1.0f,
        0.3f,
        0.05f
    );
    TestTrue(
        TEXT("Published surface wetness takes priority over rainfall integration"),
        FMath::IsNearlyEqual(Driven, 0.5f)
    );

    return true;
}

#endif
