#include "CubusCore/Meshing/CubusBlockMesher.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

namespace CubusBlockMesher
{
    FCubusBlockVoxel MakeEmptyVoxel()
    {
        return FCubusBlockVoxel();
    }

    bool IsRenderableSolid(
        const FCubusBlockVoxel* Voxel,
        const UCubusMaterialRegistry* MaterialRegistry
    )
    {
        if (
            Voxel == nullptr ||
            Voxel->IsEmpty() ||
            Voxel->IsWater()
        )
        {
            return false;
        }

        return
            MaterialRegistry == nullptr ||
            MaterialRegistry->IsRenderableSolid(Voxel->MaterialId);
    }

    FCubusBlockVoxel ResolveBlockSample(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const FIntVector& LocalCoordinate
    )
    {
        const FCubusBlockVoxel* DirectVoxel =
            Neighborhood.GetVoxel(
                LocalCoordinate.X,
                LocalCoordinate.Y,
                LocalCoordinate.Z
            );

        if (DirectVoxel != nullptr)
        {
            return IsRenderableSolid(DirectVoxel, MaterialRegistry)
                ? *DirectVoxel
                : MakeEmptyVoxel();
        }

        // The legacy block neighbourhood stores only six face neighbours.
        // Marching Cubes also asks for edge/corner samples such as
        // (ChunkSize, ChunkSize, Z). Reconstruct those samples from every
        // available axis projection instead of treating them as air, which
        // previously carved holes where chunks met.
        TArray<const FCubusBlockVoxel*, TInlineAllocator<7>> Candidates;

        const int32 ClampedX = FMath::Clamp(
            LocalCoordinate.X,
            0,
            Cubus::ChunkSize - 1
        );
        const int32 ClampedY = FMath::Clamp(
            LocalCoordinate.Y,
            0,
            Cubus::ChunkSize - 1
        );
        const int32 ClampedZ = FMath::Clamp(
            LocalCoordinate.Z,
            0,
            Cubus::ChunkSize - 1
        );

        Candidates.Add(
            Neighborhood.GetVoxel(
                LocalCoordinate.X,
                ClampedY,
                ClampedZ
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                ClampedX,
                LocalCoordinate.Y,
                ClampedZ
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                ClampedX,
                ClampedY,
                LocalCoordinate.Z
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                LocalCoordinate.X,
                LocalCoordinate.Y,
                ClampedZ
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                LocalCoordinate.X,
                ClampedY,
                LocalCoordinate.Z
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                ClampedX,
                LocalCoordinate.Y,
                LocalCoordinate.Z
            )
        );
        Candidates.Add(
            Neighborhood.GetVoxel(
                ClampedX,
                ClampedY,
                ClampedZ
            )
        );

        TMap<int32, int32> SolidVotesByMaterial;
        int32 SolidVoteCount = 0;
        int32 EmptyVoteCount = 0;

        for (const FCubusBlockVoxel* Candidate : Candidates)
        {
            if (IsRenderableSolid(Candidate, MaterialRegistry))
            {
                ++SolidVoteCount;
                ++SolidVotesByMaterial.FindOrAdd(Candidate->MaterialId);
            }
            else if (Candidate != nullptr)
            {
                ++EmptyVoteCount;
            }
        }

        // Bias ties toward solid. A tiny amount of hidden overlap is safer
        // than a visible crack between independently generated chunks.
        if (SolidVoteCount <= 0 || SolidVoteCount < EmptyVoteCount)
        {
            return MakeEmptyVoxel();
        }

        int32 DominantMaterialId = 1;
        int32 DominantVotes = -1;

        for (const TPair<int32, int32>& Pair : SolidVotesByMaterial)
        {
            if (Pair.Value > DominantVotes)
            {
                DominantMaterialId = Pair.Key;
                DominantVotes = Pair.Value;
            }
        }

        FCubusBlockVoxel Result;
        Result.MaterialId = DominantMaterialId;
        return Result;
    }

    int32 DecodeLowMaterialId(const double PackedValue)
    {
        const int64 Packed = FMath::RoundToInt64(PackedValue);
        return static_cast<int32>(
            Packed % FCubusDensityMesher::MaterialIdPackingBase
        );
    }

    int32 DecodeHighMaterialId(const double PackedValue)
    {
        const int64 Packed = FMath::RoundToInt64(PackedValue);
        return static_cast<int32>(
            Packed / FCubusDensityMesher::MaterialIdPackingBase
        );
    }

    int32 ResolveTriangleMaterial(
        const FCubusMeshData& Source,
        const int32 SourceIndex0,
        const int32 SourceIndex1,
        const int32 SourceIndex2
    )
    {
        if (!Source.UV0.IsValidIndex(SourceIndex0))
        {
            return 1;
        }

        const FVector2D PackedPalette = Source.UV0[SourceIndex0];
        const int32 MaterialIds[4] =
        {
            DecodeLowMaterialId(PackedPalette.X),
            DecodeHighMaterialId(PackedPalette.X),
            DecodeLowMaterialId(PackedPalette.Y),
            DecodeHighMaterialId(PackedPalette.Y)
        };

        float WeightTotals[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const int32 SourceIndices[3] =
        {
            SourceIndex0,
            SourceIndex1,
            SourceIndex2
        };

        for (const int32 SourceIndex : SourceIndices)
        {
            if (!Source.VertexColors.IsValidIndex(SourceIndex))
            {
                continue;
            }

            const FLinearColor& Weights = Source.VertexColors[SourceIndex];
            WeightTotals[0] += Weights.R;
            WeightTotals[1] += Weights.G;
            WeightTotals[2] += Weights.B;
            WeightTotals[3] += Weights.A;
        }

        int32 DominantSlot = 0;
        for (int32 Slot = 1; Slot < 4; ++Slot)
        {
            if (WeightTotals[Slot] > WeightTotals[DominantSlot])
            {
                DominantSlot = Slot;
            }
        }

        return FMath::Max(1, MaterialIds[DominantSlot]);
    }

    float GetBlockFaceSelector(const FVector& FaceNormal)
    {
        if (FaceNormal.Z > 0.55f)
        {
            return 0.5f;
        }

        if (FaceNormal.Z < -0.55f)
        {
            return 1.0f;
        }

        return 0.0f;
    }

    void ConvertDensityMeshToBlockSections(
        const FCubusMaterialMeshMap& DensityMeshes,
        FCubusMaterialMeshMap& OutBlockMeshes
    )
    {
        OutBlockMeshes.Reset();

        const FCubusMeshData* Source =
            DensityMeshes.Find(
                FCubusDensityMesher::UnifiedDensityMaterialKey
            );

        if (Source == nullptr)
        {
            return;
        }

        for (
            int32 TriangleOffset = 0;
            TriangleOffset + 2 < Source->Triangles.Num();
            TriangleOffset += 3
        )
        {
            const int32 SourceIndices[3] =
            {
                Source->Triangles[TriangleOffset + 0],
                Source->Triangles[TriangleOffset + 1],
                Source->Triangles[TriangleOffset + 2]
            };

            if (
                !Source->Vertices.IsValidIndex(SourceIndices[0]) ||
                !Source->Vertices.IsValidIndex(SourceIndices[1]) ||
                !Source->Vertices.IsValidIndex(SourceIndices[2])
            )
            {
                continue;
            }

            const FVector Vertex0 = Source->Vertices[SourceIndices[0]];
            const FVector Vertex1 = Source->Vertices[SourceIndices[1]];
            const FVector Vertex2 = Source->Vertices[SourceIndices[2]];

            FVector FaceNormal = FVector::CrossProduct(
                Vertex1 - Vertex0,
                Vertex2 - Vertex0
            ).GetSafeNormal();

            if (FaceNormal.IsNearlyZero())
            {
                FaceNormal = FVector::UpVector;
            }

            const int32 MaterialId = ResolveTriangleMaterial(
                *Source,
                SourceIndices[0],
                SourceIndices[1],
                SourceIndices[2]
            );

            FCubusMeshData& Target = OutBlockMeshes.FindOrAdd(MaterialId);
            const int32 FirstTargetIndex = Target.Vertices.Num();

            Target.Vertices.Add(Vertex0);
            Target.Vertices.Add(Vertex1);
            Target.Vertices.Add(Vertex2);

            Target.Triangles.Append(
            {
                FirstTargetIndex + 0,
                FirstTargetIndex + 1,
                FirstTargetIndex + 2
            });

            Target.Normals.Add(FaceNormal);
            Target.Normals.Add(FaceNormal);
            Target.Normals.Add(FaceNormal);

            // Ordinary block materials expect conventional face UVs and the
            // top/side/bottom selector in vertex alpha, not density palettes.
            Target.UV0.Add(FVector2D(0.0f, 0.0f));
            Target.UV0.Add(FVector2D(1.0f, 0.0f));
            Target.UV0.Add(FVector2D(0.0f, 1.0f));

            const float Selector = GetBlockFaceSelector(FaceNormal);
            const FLinearColor FaceColor(
                1.0f,
                1.0f,
                1.0f,
                Selector
            );

            Target.VertexColors.Add(FaceColor);
            Target.VertexColors.Add(FaceColor);
            Target.VertexColors.Add(FaceColor);

            const FProcMeshTangent Tangent(
                (Vertex1 - Vertex0).GetSafeNormal(),
                false
            );

            Target.Tangents.Add(Tangent);
            Target.Tangents.Add(Tangent);
            Target.Tangents.Add(Tangent);
        }
    }
}

void FCubusBlockMesher::BuildChunk(
    const FCubusBlockChunkNeighborhood& Neighborhood,
    const UCubusMaterialRegistry* MaterialRegistry,
    const float VoxelSize,
    FCubusMaterialMeshMap& OutMaterialMeshes,
    int32& OutGeneratedFaceCount
)
{
    OutMaterialMeshes.Reset();
    OutGeneratedFaceCount = 0;

    if (
        Neighborhood.Centre == nullptr ||
        VoxelSize <= 0.0f
    )
    {
        return;
    }

    const FIntVector ChunkCoordinate =
        Neighborhood.Centre->GetChunkCoordinate();

    const FIntVector GlobalVoxelOrigin =
        ChunkCoordinate * Cubus::ChunkSize;

    const FCubusBlockDensityField BlockDensityField(
        [
            &Neighborhood,
            MaterialRegistry,
            GlobalVoxelOrigin
        ](const FIntVector& GlobalVoxelCoordinate)
        {
            return CubusBlockMesher::ResolveBlockSample(
                Neighborhood,
                MaterialRegistry,
                GlobalVoxelCoordinate - GlobalVoxelOrigin
            );
        },
        true,
        1.0f
    );

    FCubusDensitySamplingBuffer SamplingBuffer;
    SamplingBuffer.Build(
        ChunkCoordinate,
        BlockDensityField
    );

    FCubusMaterialMeshMap DensityMeshes;
    int32 GeneratedTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        SamplingBuffer,
        VoxelSize,
        0.0f,
        DensityMeshes,
        GeneratedTriangleCount
    );

    CubusBlockMesher::ConvertDensityMeshToBlockSections(
        DensityMeshes,
        OutMaterialMeshes
    );

    OutGeneratedFaceCount = GeneratedTriangleCount;
}
