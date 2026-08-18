#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

namespace CubusDensityWindingTests
{
    class FPlaneDensityField final : public ICubusDensityField
    {
    public:
        virtual FCubusDensitySample Sample(
            const FIntVector& GlobalSampleCoordinate
        ) const override
        {
            FCubusDensitySample Result;
            Result.Density =
                10.25f -
                static_cast<float>(GlobalSampleCoordinate.Z);
            Result.MaterialId = 1;
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityUnrealWindingTest,
    "Orakai.Cubus.Density.Meshing.UnrealFrontFaceWinding",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityUnrealWindingTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    const CubusDensityWindingTests::FPlaneDensityField Field;

    FCubusDensitySamplingBuffer Buffer;
    Buffer.Build(
        FIntVector::ZeroValue,
        Field
    );

    TMap<int32, FCubusMeshData> MaterialMeshes;
    int32 TriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        Buffer,
        1.0f,
        0.0f,
        MaterialMeshes,
        TriangleCount
    );

    TestTrue(
        TEXT("The winding test generated density triangles"),
        TriangleCount > 0
    );

    for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
    {
        const FCubusMeshData& Mesh = Pair.Value;

        for (
            int32 TriangleIndex = 0;
            TriangleIndex + 2 < Mesh.Triangles.Num();
            TriangleIndex += 3
        )
        {
            const int32 IndexA = Mesh.Triangles[TriangleIndex];
            const int32 IndexB = Mesh.Triangles[TriangleIndex + 1];
            const int32 IndexC = Mesh.Triangles[TriangleIndex + 2];

            if (
                !Mesh.Vertices.IsValidIndex(IndexA) ||
                !Mesh.Vertices.IsValidIndex(IndexB) ||
                !Mesh.Vertices.IsValidIndex(IndexC) ||
                !Mesh.Normals.IsValidIndex(IndexA) ||
                !Mesh.Normals.IsValidIndex(IndexB) ||
                !Mesh.Normals.IsValidIndex(IndexC)
            )
            {
                AddError(TEXT("Density winding test found an invalid mesh index."));
                return false;
            }

            const FVector CrossNormal =
                FVector::CrossProduct(
                    Mesh.Vertices[IndexB] - Mesh.Vertices[IndexA],
                    Mesh.Vertices[IndexC] - Mesh.Vertices[IndexA]
                ).GetSafeNormal();

            const FVector StoredNormal =
                (
                    Mesh.Normals[IndexA] +
                    Mesh.Normals[IndexB] +
                    Mesh.Normals[IndexC]
                ).GetSafeNormal();

            TestTrue(
                TEXT("Density triangle winding matches Cubus/Unreal front-face order"),
                FVector::DotProduct(
                    CrossNormal,
                    StoredNormal
                ) < 0.0
            );
        }
    }

    return true;
}

#endif
