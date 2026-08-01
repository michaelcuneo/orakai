#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

namespace CubusDensityMesherTests
{
    class FFunctionalDensityField final : public ICubusDensityField
    {
    public:
        using FSampleFunction =
            TFunction<FCubusDensitySample(const FIntVector&)>;

        explicit FFunctionalDensityField(
            FSampleFunction InSampleFunction,
            const FVector& InSampleOffset = FVector::ZeroVector
        )
            : SampleFunction(MoveTemp(InSampleFunction))
            , SampleOffset(InSampleOffset)
        {
        }

        virtual FCubusDensitySample Sample(
            const FIntVector& GlobalSampleCoordinate
        ) const override
        {
            return SampleFunction(GlobalSampleCoordinate);
        }

        virtual FVector GetSampleOffsetInVoxels() const override
        {
            return SampleOffset;
        }

    private:
        FSampleFunction SampleFunction;
        FVector SampleOffset = FVector::ZeroVector;
    };

    int32 CountVertices(
        const TMap<int32, FCubusMeshData>& MaterialMeshes
    )
    {
        int32 VertexCount = 0;

        for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
        {
            VertexCount += Pair.Value.GetVertexCount();
        }

        return VertexCount;
    }

    void AddBoundaryVertices(
        const TMap<int32, FCubusMeshData>& MaterialMeshes,
        const FIntVector& ChunkCoordinate,
        const int32 Axis,
        const double BoundaryPosition,
        TSet<FIntVector>& OutQuantizedVertices
    )
    {
        const FVector ChunkWorldOrigin(
            static_cast<double>(
                ChunkCoordinate.X *
                Cubus::ChunkSize
            ),
            static_cast<double>(
                ChunkCoordinate.Y *
                Cubus::ChunkSize
            ),
            static_cast<double>(
                ChunkCoordinate.Z *
                Cubus::ChunkSize
            )
        );

        for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
        {
            for (const FVector& LocalVertex : Pair.Value.Vertices)
            {
                const FVector WorldVertex =
                    ChunkWorldOrigin +
                    LocalVertex;

                if (
                    !FMath::IsNearlyEqual(
                        WorldVertex[Axis],
                        BoundaryPosition,
                        0.0001
                    )
                )
                {
                    continue;
                }

                OutQuantizedVertices.Add(
                    FIntVector(
                        FMath::RoundToInt(
                            WorldVertex.X * 10000.0
                        ),
                        FMath::RoundToInt(
                            WorldVertex.Y * 10000.0
                        ),
                        FMath::RoundToInt(
                            WorldVertex.Z * 10000.0
                        )
                    )
                );
            }
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityHorizontalPlaneTest,
    "Orakai.Cubus.Density.HorizontalPlane",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityHorizontalPlaneTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    using namespace CubusDensityMesherTests;

    constexpr float PlaneHeight = 10.25f;

    const FFunctionalDensityField Field(
        [](const FIntVector& Coordinate)
        {
            FCubusDensitySample Sample;
            Sample.Density =
                PlaneHeight -
                static_cast<float>(Coordinate.Z);
            Sample.MaterialId = 1;
            return Sample;
        }
    );

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

    TestEqual(
        TEXT("A plane crossing every XY cell emits two triangles per cell"),
        TriangleCount,
        Cubus::ChunkArea * 2
    );

    TestEqual(
        TEXT("The plane mesh has three emitted vertices per triangle"),
        CountVertices(Meshes),
        TriangleCount * 3
    );

    const double ExpectedLocalZ =
        static_cast<double>(PlaneHeight) -
        static_cast<double>(Cubus::ChunkSize) *
        0.5;

    for (const TPair<int32, FCubusMeshData>& Pair : Meshes)
    {
        for (const FVector& Vertex : Pair.Value.Vertices)
        {
            TestTrue(
                TEXT("Every plane vertex lies on the interpolated isosurface"),
                FMath::IsNearlyEqual(
                    Vertex.Z,
                    ExpectedLocalZ,
                    0.0001
                )
            );
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityChunkSeamTest,
    "Orakai.Cubus.Density.ChunkSeam",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityChunkSeamTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    using namespace CubusDensityMesherTests;

    const FVector SphereCentre(32.0, 16.0, 16.0);
    constexpr double SphereRadius = 18.0;

    const FFunctionalDensityField Field(
        [SphereCentre](const FIntVector& Coordinate)
        {
            const FVector Position(
                static_cast<double>(Coordinate.X),
                static_cast<double>(Coordinate.Y),
                static_cast<double>(Coordinate.Z)
            );

            FCubusDensitySample Sample;
            Sample.Density =
                static_cast<float>(
                    SphereRadius -
                    FVector::Distance(
                        Position,
                        SphereCentre
                    )
                );
            Sample.MaterialId = 2;
            return Sample;
        }
    );

    FCubusDensitySamplingBuffer LeftBuffer;
    FCubusDensitySamplingBuffer RightBuffer;

    LeftBuffer.Build(FIntVector(0, 0, 0), Field);
    RightBuffer.Build(FIntVector(1, 0, 0), Field);

    TMap<int32, FCubusMeshData> LeftMeshes;
    TMap<int32, FCubusMeshData> RightMeshes;
    int32 LeftTriangleCount = 0;
    int32 RightTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        LeftBuffer,
        1.0f,
        0.0f,
        LeftMeshes,
        LeftTriangleCount
    );

    FCubusDensityMesher::BuildChunk(
        RightBuffer,
        1.0f,
        0.0f,
        RightMeshes,
        RightTriangleCount
    );

    TestTrue(
        TEXT("The sphere intersects the left chunk"),
        LeftTriangleCount > 0
    );

    TestTrue(
        TEXT("The sphere intersects the right chunk"),
        RightTriangleCount > 0
    );

    TSet<FIntVector> LeftBoundaryVertices;
    TSet<FIntVector> RightBoundaryVertices;

    AddBoundaryVertices(
        LeftMeshes,
        FIntVector(0, 0, 0),
        0,
        16.0,
        LeftBoundaryVertices
    );

    AddBoundaryVertices(
        RightMeshes,
        FIntVector(1, 0, 0),
        0,
        16.0,
        RightBoundaryVertices
    );

    TestTrue(
        TEXT("The test sphere creates vertices on the shared chunk boundary"),
        LeftBoundaryVertices.Num() > 0
    );

    TestEqual(
        TEXT("Both chunks calculate the same shared-boundary vertex count"),
        LeftBoundaryVertices.Num(),
        RightBoundaryVertices.Num()
    );

    bool bBoundarySetsMatch =
        LeftBoundaryVertices.Num() ==
        RightBoundaryVertices.Num();

    if (bBoundarySetsMatch)
    {
        for (const FIntVector& Vertex : LeftBoundaryVertices)
        {
            if (!RightBoundaryVertices.Contains(Vertex))
            {
                bBoundarySetsMatch = false;
                break;
            }
        }
    }

    TestTrue(
        TEXT("Shared-boundary vertices are position-identical across chunks"),
        bBoundarySetsMatch
    );

    return true;
}

#endif
