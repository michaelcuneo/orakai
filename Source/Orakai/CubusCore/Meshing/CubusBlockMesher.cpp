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

    const FCubusBlockVoxel* ResolveAxisNeighbourVoxel(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const FIntVector& LocalCoordinate,
        const int32 Axis
    )
    {
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

        if (Axis == 0)
        {
            if (LocalCoordinate.X < 0 && Neighborhood.NegativeX != nullptr)
            {
                return Neighborhood.NegativeX->GetVoxel(
                    Cubus::ChunkSize + LocalCoordinate.X,
                    ClampedY,
                    ClampedZ
                );
            }

            if (
                LocalCoordinate.X >= Cubus::ChunkSize &&
                Neighborhood.PositiveX != nullptr
            )
            {
                return Neighborhood.PositiveX->GetVoxel(
                    LocalCoordinate.X - Cubus::ChunkSize,
                    ClampedY,
                    ClampedZ
                );
            }
        }
        else if (Axis == 1)
        {
            if (LocalCoordinate.Y < 0 && Neighborhood.NegativeY != nullptr)
            {
                return Neighborhood.NegativeY->GetVoxel(
                    ClampedX,
                    Cubus::ChunkSize + LocalCoordinate.Y,
                    ClampedZ
                );
            }

            if (
                LocalCoordinate.Y >= Cubus::ChunkSize &&
                Neighborhood.PositiveY != nullptr
            )
            {
                return Neighborhood.PositiveY->GetVoxel(
                    ClampedX,
                    LocalCoordinate.Y - Cubus::ChunkSize,
                    ClampedZ
                );
            }
        }
        else
        {
            if (LocalCoordinate.Z < 0 && Neighborhood.NegativeZ != nullptr)
            {
                return Neighborhood.NegativeZ->GetVoxel(
                    ClampedX,
                    ClampedY,
                    Cubus::ChunkSize + LocalCoordinate.Z
                );
            }

            if (
                LocalCoordinate.Z >= Cubus::ChunkSize &&
                Neighborhood.PositiveZ != nullptr
            )
            {
                return Neighborhood.PositiveZ->GetVoxel(
                    ClampedX,
                    ClampedY,
                    LocalCoordinate.Z - Cubus::ChunkSize
                );
            }
        }

        return nullptr;
    }

    FCubusBlockVoxel ResolveBlockSample(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const FIntVector& LocalCoordinate
    )
    {
        if (
            LocalCoordinate.X >= 0 &&
            LocalCoordinate.X < Cubus::ChunkSize &&
            LocalCoordinate.Y >= 0 &&
            LocalCoordinate.Y < Cubus::ChunkSize &&
            LocalCoordinate.Z >= 0 &&
            LocalCoordinate.Z < Cubus::ChunkSize
        )
        {
            const FCubusBlockVoxel* DirectVoxel =
                Neighborhood.Centre->GetVoxel(LocalCoordinate);

            return IsRenderableSolid(DirectVoxel, MaterialRegistry)
                ? *DirectVoxel
                : MakeEmptyVoxel();
        }

        TArray<const FCubusBlockVoxel*, TInlineAllocator<4>> Candidates;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            if (
                const FCubusBlockVoxel* AxisVoxel =
                    ResolveAxisNeighbourVoxel(
                        Neighborhood,
                        LocalCoordinate,
                        Axis
                    )
            )
            {
                Candidates.Add(AxisVoxel);
            }
        }

        const FIntVector ClampedCoordinate(
            FMath::Clamp(LocalCoordinate.X, 0, Cubus::ChunkSize - 1),
            FMath::Clamp(LocalCoordinate.Y, 0, Cubus::ChunkSize - 1),
            FMath::Clamp(LocalCoordinate.Z, 0, Cubus::ChunkSize - 1)
        );

        Candidates.Add(
            Neighborhood.Centre->GetVoxel(ClampedCoordinate)
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
            else
            {
                ++EmptyVoteCount;
            }
        }

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

    int32 FindDominantPaletteSlot(
        const FCubusMeshData& Mesh,
        const int32 Index0,
        const int32 Index1,
        const int32 Index2
    )
    {
        const int32 Indices[3] = { Index0, Index1, Index2 };
        float Totals[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        for (const int32 Index : Indices)
        {
            if (!Mesh.VertexColors.IsValidIndex(Index))
            {
                continue;
            }

            const FLinearColor& Weight = Mesh.VertexColors[Index];
            Totals[0] += Weight.R;
            Totals[1] += Weight.G;
            Totals[2] += Weight.B;
            Totals[3] += Weight.A;
        }

        int32 DominantSlot = 0;
        for (int32 Slot = 1; Slot < 4; ++Slot)
        {
            if (Totals[Slot] > Totals[DominantSlot])
            {
                DominantSlot = Slot;
            }
        }

        return DominantSlot;
    }

    FLinearColor MakeHardPaletteWeight(const int32 Slot)
    {
        switch (Slot)
        {
            case 1: return FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
            case 2: return FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
            case 3: return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
            default: return FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    void HardenGeologicalBlockMesh(FCubusMeshData& Mesh)
    {
        for (
            int32 TriangleOffset = 0;
            TriangleOffset + 2 < Mesh.Triangles.Num();
            TriangleOffset += 3
        )
        {
            const int32 Index0 = Mesh.Triangles[TriangleOffset + 0];
            const int32 Index1 = Mesh.Triangles[TriangleOffset + 1];
            const int32 Index2 = Mesh.Triangles[TriangleOffset + 2];

            if (
                !Mesh.Vertices.IsValidIndex(Index0) ||
                !Mesh.Vertices.IsValidIndex(Index1) ||
                !Mesh.Vertices.IsValidIndex(Index2)
            )
            {
                continue;
            }

            const FVector& Vertex0 = Mesh.Vertices[Index0];
            const FVector& Vertex1 = Mesh.Vertices[Index1];
            const FVector& Vertex2 = Mesh.Vertices[Index2];

            FVector FaceNormal = FVector::CrossProduct(
                Vertex1 - Vertex0,
                Vertex2 - Vertex0
            ).GetSafeNormal();

            if (FaceNormal.IsNearlyZero())
            {
                FaceNormal = FVector::UpVector;
            }

            const int32 DominantSlot = FindDominantPaletteSlot(
                Mesh,
                Index0,
                Index1,
                Index2
            );

            const FLinearColor HardWeight =
                MakeHardPaletteWeight(DominantSlot);

            const FVector TangentDirection =
                (Vertex1 - Vertex0).GetSafeNormal();
            const FProcMeshTangent Tangent(
                TangentDirection.IsNearlyZero()
                    ? FVector::ForwardVector
                    : TangentDirection,
                false
            );

            const int32 Indices[3] = { Index0, Index1, Index2 };
            for (const int32 Index : Indices)
            {
                if (Mesh.Normals.IsValidIndex(Index))
                {
                    Mesh.Normals[Index] = FaceNormal;
                }

                if (Mesh.VertexColors.IsValidIndex(Index))
                {
                    Mesh.VertexColors[Index] = HardWeight;
                }

                if (Mesh.Tangents.IsValidIndex(Index))
                {
                    Mesh.Tangents[Index] = Tangent;
                }
            }
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

    int32 GeneratedTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        SamplingBuffer,
        VoxelSize,
        0.0f,
        OutMaterialMeshes,
        GeneratedTriangleCount
    );

    if (
        FCubusMeshData* GeologicalMesh =
            OutMaterialMeshes.Find(
                FCubusDensityMesher::UnifiedDensityMaterialKey
            )
    )
    {
        CubusBlockMesher::HardenGeologicalBlockMesh(*GeologicalMesh);
    }

    OutGeneratedFaceCount = GeneratedTriangleCount;
}
