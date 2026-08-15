#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Storage/CubusDensityChunkStore.h"

namespace
{
    class FCubusDensityCacheTestField final : public ICubusDensityField
    {
    public:
        virtual FCubusDensitySample Sample(const FIntVector& Coordinate) const override
        {
  FCubusDensitySample Sample;
  Sample.Density = static_cast<float>(Coordinate.Z) - 17.25f + static_cast<float>(Coordinate.X & 3) * 0.03125f;
  Sample.MaterialId = ((Coordinate.X + Coordinate.Y + Coordinate.Z) & 1) == 0 ? 3 : 7;
  return Sample;
        }

        virtual FVector GetSampleOffsetInVoxels() const override
        {
  return FVector(0.25, 0.50, 0.75);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityChunkStoreRoundTripTest,
    "Orakai.Cubus.Storage.DensityChunkRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusDensityChunkStoreRoundTripTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FIntVector Coordinate(21791, -19007, 413);
    FCubusDensityChunkStoreContext Context;
    Context.WorldSeed = 0x123456789LL;
    Context.GenerationVersion = 0x7ffffff0u;
    Context.VoxelSize = 100.0f;
    Context.SubdivisionsPerVoxel = 1;

    FCubusDensityChunkStore::DeleteBuffer(Coordinate, Context);

    FCubusDensityCacheTestField Field;
    FCubusDensitySamplingBuffer Original;
    Original.Build(Coordinate, Field);

    TestTrue(TEXT("Density baseline saves to disk"), FCubusDensityChunkStore::SaveBuffer(Original, Context));
    TestTrue(TEXT("Density baseline file exists"), FCubusDensityChunkStore::HasBuffer(Coordinate, Context));

    FCubusDensitySamplingBuffer Loaded;
    TestTrue(TEXT("Density baseline loads from disk"), FCubusDensityChunkStore::LoadBuffer(Coordinate, Context, Loaded));
    TestTrue(TEXT("Loaded density baseline is built"), Loaded.IsBuilt());
    TestEqual(TEXT("Loaded density baseline keeps its chunk coordinate"), Loaded.GetChunkCoordinate(), Coordinate);
    TestTrue(TEXT("Loaded density baseline keeps sample offset"), Loaded.GetSampleOffsetInVoxels().Equals(Original.GetSampleOffsetInVoxels(), KINDA_SMALL_NUMBER));

    const TConstArrayView<FCubusDensitySample> OriginalSamples = Original.GetSamples();
    const TConstArrayView<FCubusDensitySample> LoadedSamples = Loaded.GetSamples();
    TestEqual(TEXT("Loaded density baseline keeps sample count"), LoadedSamples.Num(), OriginalSamples.Num());

    bool bSamplesMatch = LoadedSamples.Num() == OriginalSamples.Num();
    for (int32 Index = 0; bSamplesMatch && Index < OriginalSamples.Num(); ++Index)
    {
        bSamplesMatch =
  OriginalSamples[Index].Density == LoadedSamples[Index].Density &&
  OriginalSamples[Index].MaterialId == LoadedSamples[Index].MaterialId;
    }
    TestTrue(TEXT("Density baseline round-trip is exact"), bSamplesMatch);

    FCubusDensityChunkStoreContext WrongVoxelSize = Context;
    WrongVoxelSize.VoxelSize = 50.0f;
    FCubusDensitySamplingBuffer WrongLoad;
    TestFalse(TEXT("Different density resolution cannot reuse the cache"),
        FCubusDensityChunkStore::LoadBuffer(Coordinate, WrongVoxelSize, WrongLoad));

    TestTrue(TEXT("Density baseline cache can be deleted"), FCubusDensityChunkStore::DeleteBuffer(Coordinate, Context));
    return true;
}

#endif
