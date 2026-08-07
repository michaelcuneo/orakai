#include "CubusCore/Generation/CubusTerrainClutterGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Data/CubusTerrainSurfaceLayers.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"

namespace CubusTerrainClutterGenerator
{
    uint32 HashColumn(const int32 X, const int32 Y, const int32 Salt)
    {
        uint32 Hash = static_cast<uint32>(X) * 0x8da6b343u;
        Hash ^= static_cast<uint32>(Y) * 0xd8163841u;
        Hash ^= static_cast<uint32>(Salt) * 0xcb1ab31fu;
        Hash ^= Hash >> 13;
        Hash *= 0x85ebca6bu;
        Hash ^= Hash >> 16;
        return Hash;
    }

    int32 PositiveModulo(const int32 Value, const int32 Modulus)
    {
        const int32 Result = Value % Modulus;
        return Result < 0 ? Result + Modulus : Result;
    }

    bool IsCellCandidate(
        const int32 WorldX,
        const int32 WorldY,
        const int32 CellSizeVoxels,
        const int32 Salt
    )
    {
        const int32 SafeCellSize = FMath::Max(1, CellSizeVoxels);
        const int32 CellX = FMath::FloorToInt(
            static_cast<double>(WorldX) / static_cast<double>(SafeCellSize)
        );
        const int32 CellY = FMath::FloorToInt(
            static_cast<double>(WorldY) / static_cast<double>(SafeCellSize)
        );

        const int32 CandidateX = CellX * SafeCellSize + PositiveModulo(
            static_cast<int32>(HashColumn(CellX, CellY, Salt ^ 0x4b91)),
            SafeCellSize
        );
        const int32 CandidateY = CellY * SafeCellSize + PositiveModulo(
            static_cast<int32>(HashColumn(CellX, CellY, Salt ^ 0x96d3)),
            SafeCellSize
        );

        return WorldX == CandidateX && WorldY == CandidateY;
    }

    FCubusVegetationInstance MakeInstance(
        const FIntVector& WorldVoxel,
        const int32 TypeId,
        const int32 Seed,
        const float MinimumScale,
        const float MaximumScale
    )
    {
        const uint32 RotationHash = HashColumn(
            WorldVoxel.X,
            WorldVoxel.Y,
            Seed ^ TypeId ^ 0x211
        );
        const uint32 ScaleHash = HashColumn(
            WorldVoxel.X,
            WorldVoxel.Y,
            Seed ^ TypeId ^ 0x307
        );

        FCubusVegetationInstance Instance;
        Instance.WorldVoxel = WorldVoxel;
        Instance.RotationYaw =
            static_cast<float>(RotationHash & 0x00ffffffu) /
            static_cast<float>(0x01000000u) * 360.0f;
        Instance.Scale = FMath::Lerp(
            MinimumScale,
            MaximumScale,
            static_cast<float>(ScaleHash & 0x00ffffffu) /
                static_cast<float>(0x01000000u)
        );
        Instance.TypeId = TypeId;
        Instance.BiomeMask = CubusVegetationBiome::All;
        return Instance;
    }
}

void FCubusTerrainClutterGenerator::Append(
    FCubusBlockChunkData& Chunk,
    const UCubusMaterialRegistry* MaterialRegistry,
    const float VoxelSize
)
{
    if (!IsValid(MaterialRegistry) || VoxelSize <= 0.0f)
    {
        return;
    }

    const FCubusTerrainSurfaceLayerSettings& Settings =
        MaterialRegistry->TerrainSurfaceLayers;
    const int32 Seed = Chunk.GetGenerationSeeds().Vegetation;
    const int32 CellSizeVoxels = FMath::Max(
        1,
        FMath::CeilToInt(Settings.ClutterMinimumSpacing / VoxelSize)
    );

    TSet<FIntVector> OccupiedColumns;
    for (const FCubusVegetationInstance& Existing : Chunk.GetVegetationInstances())
    {
        OccupiedColumns.Add(
            FIntVector(Existing.WorldVoxel.X, Existing.WorldVoxel.Y, 0)
        );
    }

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;

    int32 GrassCount = 0;
    int32 StoneCount = 0;
    int32 OrganicCount = 0;

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            const int32 SurfaceLocalZ = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                LocalY
            );
            if (
                SurfaceLocalZ == INDEX_NONE ||
                SurfaceLocalZ >= Cubus::ChunkSize - 1
            )
            {
                continue;
            }

            const FCubusBlockVoxel* Above = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ + 1
            );
            if (Above == nullptr || Above->MaterialId > 0 || Above->IsWater())
            {
                continue;
            }

            const int32 WorldX = BaseX + LocalX;
            const int32 WorldY = BaseY + LocalY;
            const int32 WorldZ = BaseZ + SurfaceLocalZ + 1;
            const FIntVector OccupiedKey(WorldX, WorldY, 0);
            if (OccupiedColumns.Contains(OccupiedKey))
            {
                continue;
            }

            const int32 West = FindSurfaceLocalZ(
                Chunk,
                FMath::Max(0, LocalX - 1),
                LocalY
            );
            const int32 East = FindSurfaceLocalZ(
                Chunk,
                FMath::Min(Cubus::ChunkSize - 1, LocalX + 1),
                LocalY
            );
            const int32 South = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                FMath::Max(0, LocalY - 1)
            );
            const int32 North = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                FMath::Min(Cubus::ChunkSize - 1, LocalY + 1)
            );

            const float GradientX =
                West != INDEX_NONE && East != INDEX_NONE
                    ? static_cast<float>(East - West) * 0.5f
                    : 0.0f;
            const float GradientY =
                South != INDEX_NONE && North != INDEX_NONE
                    ? static_cast<float>(North - South) * 0.5f
                    : 0.0f;
            const FVector SurfaceNormal = FVector(
                -GradientX,
                -GradientY,
                1.0f
            ).GetSafeNormal();
            const FVector WorldPosition(
                (static_cast<double>(WorldX) + 0.5) * VoxelSize,
                (static_cast<double>(WorldY) + 0.5) * VoxelSize,
                static_cast<double>(WorldZ) * VoxelSize
            );
            const FCubusTerrainSurfaceLayerMasks Masks =
                EvaluateCubusTerrainSurfaceLayers(
                    Settings,
                    WorldPosition,
                    SurfaceNormal,
                    Seed
                );

            int32 TypeId = 0;
            float MinimumScale = 0.8f;
            float MaximumScale = 1.2f;

            if (
                Masks.StoneClutter > 0.0f &&
                CubusTerrainClutterGenerator::IsCellCandidate(
                    WorldX,
                    WorldY,
                    CellSizeVoxels,
                    Seed ^ 0x32b7
                )
            )
            {
                TypeId = CubusVegetationType::StoneClutter;
                MinimumScale = 0.65f;
                MaximumScale = 1.35f;
                ++StoneCount;
            }
            else if (
                Masks.OrganicClutter > 0.0f &&
                CubusTerrainClutterGenerator::IsCellCandidate(
                    WorldX,
                    WorldY,
                    CellSizeVoxels,
                    Seed ^ 0x8d31
                )
            )
            {
                TypeId = CubusVegetationType::OrganicClutter;
                MinimumScale = 0.75f;
                MaximumScale = 1.25f;
                ++OrganicCount;
            }
            else if (
                Masks.GrassClutter > 0.0f &&
                CubusTerrainClutterGenerator::IsCellCandidate(
                    WorldX,
                    WorldY,
                    CellSizeVoxels,
                    Seed ^ 0x1241
                )
            )
            {
                TypeId = CubusVegetationType::Grass;
                MinimumScale = 0.8f;
                MaximumScale = 1.2f;
                ++GrassCount;
            }

            if (TypeId <= 0)
            {
                continue;
            }

            const FIntVector WorldVoxel(WorldX, WorldY, WorldZ);
            Chunk.AddOrReplaceVegetationInstance(
                CubusTerrainClutterGenerator::MakeInstance(
                    WorldVoxel,
                    TypeId,
                    Seed,
                    MinimumScale,
                    MaximumScale
                )
            );
            OccupiedColumns.Add(OccupiedKey);
        }
    }

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus terrain clutter chunk (%d, %d, %d): grass %d, stones %d, organic %d"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        GrassCount,
        StoneCount,
        OrganicCount
    );
}

int32 FCubusTerrainClutterGenerator::FindSurfaceLocalZ(
    const FCubusBlockChunkData& Chunk,
    const int32 LocalX,
    const int32 LocalY
)
{
    for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
    {
        const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
        if (Voxel != nullptr && Voxel->MaterialId > 0 && !Voxel->IsWater())
        {
            return LocalZ;
        }
    }

    return INDEX_NONE;
}
