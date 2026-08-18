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

    class FContinuousPlaneField final : public ICubusDensityField
    {
    public:
        explicit FContinuousPlaneField(const float InPlaneHeight)
            : PlaneHeight(InPlaneHeight)
        {
        }

        virtual FCubusDensitySample Sample(
            const FIntVector& GlobalSampleCoordinate
        ) const override
        {
            return SampleContinuous(
                FVector(
                    GlobalSampleCoordinate.X,
                    GlobalSampleCoordinate.Y,
                    GlobalSampleCoordinate.Z
                )
            );
        }

        virtual FCubusDensitySample SampleContinuous(
            const FVector& GlobalSampleCoordinate
        ) const override
        {
            FCubusDensitySample Result;
            Result.Density =
                PlaneHeight -
                static_cast<float>(GlobalSampleCoordinate.Z);
            Result.MaterialId = Result.Density > 0.0f ? 1 : 0;
            return Result;
        }

    private:
        float PlaneHeight = 0.0f;
    };

    class FWavyHeightField final : public ICubusDensityField
    {
    public:
        static float HeightAt(const float WorldY)
        {
            return
                12.0f +
                FMath::Sin(WorldY * 0.37f) * 2.5f;
        }

        virtual FCubusDensitySample Sample(
            const FIntVector& GlobalSampleCoordinate
        ) const override
        {
            FCubusDensitySample Result;
            Result.Density =
                HeightAt(
                    static_cast<float>(GlobalSampleCoordinate.Y)
                ) -
                static_cast<float>(GlobalSampleCoordinate.Z);
            Result.MaterialId = Result.Density > 0.0f ? 3 : 0;
            return Result;
        }

        virtual FCubusDensitySample SampleContinuous(
            const FVector& GlobalSampleCoordinate
        ) const override
        {
            FCubusDensitySample Result;
            Result.Density =
                HeightAt(
                    static_cast<float>(GlobalSampleCoordinate.Y)
                ) -
                static_cast<float>(GlobalSampleCoordinate.Z);
            Result.MaterialId = Result.Density > 0.0f ? 3 : 0;
            return Result;
        }
    };

    class FContinuousSphereField final : public ICubusDensityField
    {
    public:
        FContinuousSphereField(const FVector& InCentre, const float InRadius)
            : Centre(InCentre), Radius(InRadius)
        {
        }

        virtual FCubusDensitySample Sample(const FIntVector& Coordinate) const override
        {
            return SampleContinuous(FVector(Coordinate.X, Coordinate.Y, Coordinate.Z));
        }

        virtual FCubusDensitySample SampleContinuous(const FVector& Coordinate) const override
        {
            FCubusDensitySample Result;
            Result.Density = Radius - static_cast<float>(FVector::Distance(Coordinate, Centre));
            Result.MaterialId = Result.Density > 0.0f ? 3 : 0;
            return Result;
        }

    private:
        FVector Centre = FVector::ZeroVector;
        float Radius = 10.0f;
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

    FString QuantizedVertexKey(const FVector& Position)
    {
        return FString::Printf(
            TEXT("%d,%d,%d"),
            FMath::RoundToInt(Position.X * 10000.0),
            FMath::RoundToInt(Position.Y * 10000.0),
            FMath::RoundToInt(Position.Z * 10000.0)
        );
    }

    void AddBoundarySegments(
        const TMap<int32, FCubusMeshData>& MaterialMeshes,
        const FIntVector& ChunkCoordinate,
        const int32 Axis,
        const double BoundaryPosition,
        TSet<FString>& OutSegments
    )
    {
        const FVector ChunkWorldOrigin(
            static_cast<double>(ChunkCoordinate.X * Cubus::ChunkSize),
            static_cast<double>(ChunkCoordinate.Y * Cubus::ChunkSize),
            static_cast<double>(ChunkCoordinate.Z * Cubus::ChunkSize)
        );

        for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
        {
            const FCubusMeshData& Mesh = Pair.Value;
            for (int32 TriangleIndex = 0; TriangleIndex + 2 < Mesh.Triangles.Num(); TriangleIndex += 3)
            {
                const FVector Positions[3] =
                {
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 0]],
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 1]],
                    ChunkWorldOrigin + Mesh.Vertices[Mesh.Triangles[TriangleIndex + 2]]
                };
                static constexpr int32 EdgePairs[3][2] = { {0,1}, {1,2}, {2,0} };
                for (const int32* Edge : EdgePairs)
                {
                    const FVector& A = Positions[Edge[0]];
                    const FVector& B = Positions[Edge[1]];
                    if (!FMath::IsNearlyEqual(A[Axis], BoundaryPosition, 0.0001) ||
                        !FMath::IsNearlyEqual(B[Axis], BoundaryPosition, 0.0001))
                    {
                        continue;
                    }
                    FString KeyA = QuantizedVertexKey(A);
                    FString KeyB = QuantizedVertexKey(B);
                    if (KeyA == KeyB)
                    {
                        continue;
                    }
                    if (KeyB < KeyA)
                    {
                        Swap(KeyA, KeyB);
                    }
                    OutSegments.Add(KeyA + TEXT("|") + KeyB);
                }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusAdaptiveDensityResolutionTest,
    "Orakai.Cubus.Density.LOD.AdaptiveResolution",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusAdaptiveDensityResolutionTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    using namespace CubusDensityMesherTests;

    constexpr float PlaneHeight = 10.375f;
    constexpr int32 Subdivisions = 4;
    const FContinuousPlaneField Field(PlaneHeight);

    TMap<int32, FCubusMeshData> Meshes;
    int32 TriangleCount = 0;

    FCubusDensityMesher::BuildAdaptiveChunk(
        Field,
        FIntVector::ZeroValue,
        100.0f,
        Subdivisions,
        0.0f,
        Meshes,
        TriangleCount
    );

    TestEqual(
        TEXT("A 25 cm plane emits two triangles per refined XY cell"),
        TriangleCount,
        Cubus::ChunkArea *
            Subdivisions *
            Subdivisions *
            2
    );

    const double ExpectedLocalZ =
        (
            static_cast<double>(PlaneHeight) -
            static_cast<double>(Cubus::ChunkSize) * 0.5
        ) * 100.0;

    for (const TPair<int32, FCubusMeshData>& Pair : Meshes)
    {
        for (const FVector& Vertex : Pair.Value.Vertices)
        {
            TestTrue(
                TEXT("Adaptive vertices retain the fixed 32 metre chunk extent"),
                Vertex.X >= -1600.0001 &&
                Vertex.X <= 1600.0001 &&
                Vertex.Y >= -1600.0001 &&
                Vertex.Y <= 1600.0001
            );

            TestTrue(
                TEXT("Adaptive interpolation resolves fractional canonical heights"),
                FMath::IsNearlyEqual(
                    Vertex.Z,
                    ExpectedLocalZ,
                    0.001
                )
            );
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusMixedDensityLodBoundaryTest,
    "Orakai.Cubus.Density.LOD.WatertightTransitions",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusMixedDensityLodBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace CubusDensityMesherTests;

    const int32 CoarseSubdivisions[] = { 1, 2 };
    for (const int32 CoarseSubdivisionsValue : CoarseSubdivisions)
    {
        const int32 FineSubdivisions = CoarseSubdivisionsValue * 2;
        for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
        {
            const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
            const FIntVector FineChunkCoordinate = FCubusDensityTransitionFaces::GetOffset(Face);
            const int32 Axis = FaceIndex / 2;
            const bool bPositive = (FaceIndex & 1) != 0;
            const double BoundaryPosition = bPositive ? 16.0 : -16.0;

            FVector SphereCentre = FVector::ZeroVector;
            SphereCentre[Axis] = BoundaryPosition;
            const FContinuousSphereField Field(SphereCentre, 10.0f);

            FCubusDensityTransitionFaces CoarseFaces;
            CoarseFaces.Set(Face, FineSubdivisions);

            TMap<int32, FCubusMeshData> CoarseMeshes;
            TMap<int32, FCubusMeshData> FineMeshes;
            int32 CoarseTriangleCount = 0;
            int32 FineTriangleCount = 0;

            FCubusDensityMesher::BuildAdaptiveChunk(
                Field,
                FIntVector::ZeroValue,
                1.0f,
                CoarseSubdivisionsValue,
                0.0f,
                CoarseMeshes,
                CoarseTriangleCount,
                CoarseFaces
            );
            FCubusDensityMesher::BuildAdaptiveChunk(
                Field,
                FineChunkCoordinate,
                1.0f,
                FineSubdivisions,
                0.0f,
                FineMeshes,
                FineTriangleCount
            );

            const FString Context = FString::Printf(
                TEXT("%dx->%dx face %d"),
                CoarseSubdivisionsValue,
                FineSubdivisions,
                FaceIndex
            );
            TestTrue(*FString::Printf(TEXT("%s coarse mesh intersects transition"), *Context), CoarseTriangleCount > 0);
            TestTrue(*FString::Printf(TEXT("%s fine mesh intersects transition"), *Context), FineTriangleCount > 0);

            TSet<FString> CoarseSegments;
            TSet<FString> FineSegments;
            AddBoundarySegments(
                CoarseMeshes,
                FIntVector::ZeroValue,
                Axis,
                BoundaryPosition,
                CoarseSegments
            );
            AddBoundarySegments(
                FineMeshes,
                FineChunkCoordinate,
                Axis,
                BoundaryPosition,
                FineSegments
            );

            TestTrue(*FString::Printf(TEXT("%s emits shared-boundary contour segments"), *Context), CoarseSegments.Num() > 0);
            bool bSegmentsMatch = CoarseSegments.Num() == FineSegments.Num();
            if (bSegmentsMatch)
            {
                for (const FString& Segment : CoarseSegments)
                {
                    if (!FineSegments.Contains(Segment))
                    {
                        bSegmentsMatch = false;
                        break;
                    }
                }
            }
            TestTrue(*FString::Printf(TEXT("%s boundary segments are position/topology identical"), *Context), bSegmentsMatch);
        }
    }

    return true;
}

#endif
