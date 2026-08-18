#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Rendering/CubusDensityMaterialKey.h"

namespace CubusDensityMaterialBlendTests
{
    class FBiomeBoundaryField final : public ICubusDensityField
    {
    public:
        virtual FCubusDensitySample Sample(
            const FIntVector& Coordinate
        ) const override
        {
            FCubusDensitySample Sample;
            Sample.Density =
                10.25f -
                static_cast<float>(Coordinate.Z);
            Sample.MaterialId =
                Coordinate.X < 16
                    ? 2
                    : 3;
            return Sample;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityMaterialBlendTest,
    "Orakai.Cubus.Density.MaterialBlend",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityMaterialBlendTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    using namespace CubusDensityMaterialBlendTests;

    FBiomeBoundaryField Field;
    FCubusDensitySamplingBuffer Buffer;
    Buffer.Build(FIntVector::ZeroValue, Field);

    TMap<int32, FCubusMeshData> Meshes;
    int32 TriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        Buffer,
        1.0f,
        0.0f,
        Meshes,
        TriangleCount
    );

    const int32 BlendKey =
        FCubusDensityMaterialKey::Make(2, 3);

    const FCubusMeshData* BlendMesh =
        Meshes.Find(BlendKey);

    TestNotNull(
        TEXT("A section is emitted for the biome material pair"),
        BlendMesh
    );

    if (BlendMesh == nullptr)
    {
        return false;
    }

    bool bFoundPrimaryWeight = false;
    bool bFoundSecondaryWeight = false;

    for (const FLinearColor& Color : BlendMesh->VertexColors)
    {
        bFoundPrimaryWeight |= FMath::IsNearlyZero(Color.A);
        bFoundSecondaryWeight |= FMath::IsNearlyEqual(Color.A, 1.0f);
    }

    TestTrue(
        TEXT("The pair section contains primary material vertices"),
        bFoundPrimaryWeight
    );
    TestTrue(
        TEXT("The pair section contains secondary material vertices"),
        bFoundSecondaryWeight
    );

    int32 PrimaryMaterialId = 0;
    int32 SecondaryMaterialId = 0;

    TestTrue(
        TEXT("The density pair key decodes"),
        FCubusDensityMaterialKey::Decode(
            BlendKey,
            PrimaryMaterialId,
            SecondaryMaterialId
        )
    );
    TestEqual(
        TEXT("The primary material ID is preserved"),
        PrimaryMaterialId,
        2
    );
    TestEqual(
        TEXT("The secondary material ID is preserved"),
        SecondaryMaterialId,
        3
    );

    return true;
}

#endif
