#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusDensityEditField.h"

namespace CubusDensityEditFieldTests
{
    class FConstantField final : public ICubusDensityField
    {
    public:
        virtual FCubusDensitySample Sample(
            const FIntVector& GlobalSampleCoordinate
        ) const override
        {
            (void)GlobalSampleCoordinate;

            FCubusDensitySample Result;
            Result.Density = 1.0f;
            Result.MaterialId = 2;
            return Result;
        }

        virtual FVector GetSampleOffsetInVoxels() const override
        {
            return FVector(0.25, 0.5, 0.75);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusSparseDensityEditFieldTest,
    "Orakai.Cubus.Density.Editing.SparseOverlay",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusSparseDensityEditFieldTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    const CubusDensityEditFieldTests::FConstantField GeneratedField;
    FCubusDensityEditMap Edits;

    FCubusDensityEdit RemovedEdit;
    RemovedEdit.DensityDelta = -2.0f;
    Edits.Add(FIntVector(1, 2, 3), RemovedEdit);

    FCubusDensityEdit AddedEdit;
    AddedEdit.DensityDelta = 2.0f;
    AddedEdit.MaterialId = 7;
    Edits.Add(FIntVector(-1, -2, -3), AddedEdit);

    const FCubusDensityEditField EditedField(
        GeneratedField,
        Edits
    );

    const FCubusDensitySample Untouched =
        EditedField.Sample(FIntVector::ZeroValue);

    TestEqual(
        TEXT("An unedited sample preserves generated density"),
        Untouched.Density,
        1.0f
    );
    TestEqual(
        TEXT("An unedited sample preserves generated material"),
        Untouched.MaterialId,
        2
    );

    const FCubusDensitySample Removed =
        EditedField.Sample(FIntVector(1, 2, 3));

    TestTrue(
        TEXT("A negative edit removes generated solid terrain"),
        !Removed.IsSolid()
    );
    TestEqual(
        TEXT("Removed terrain has no solid material"),
        Removed.MaterialId,
        0
    );

    const FCubusDensitySample Added =
        EditedField.Sample(FIntVector(-1, -2, -3));

    TestEqual(
        TEXT("A positive edit accumulates with generated density"),
        Added.Density,
        3.0f
    );
    TestEqual(
        TEXT("Added terrain uses the authored material"),
        Added.MaterialId,
        7
    );

    TestTrue(
        TEXT("The edit adapter preserves the generated sample offset"),
        EditedField.GetSampleOffsetInVoxels().Equals(
            GeneratedField.GetSampleOffsetInVoxels()
        )
    );

    return true;
}

#endif
