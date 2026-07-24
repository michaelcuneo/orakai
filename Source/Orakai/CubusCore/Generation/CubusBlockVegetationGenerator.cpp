#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

void FCubusBlockVegetationGenerator::Generate(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile
)
{
    TArray<FCubusVegetationInstance> Instances;

    const bool bUseConfiguredBiomes =
        IsValid(GeologyProfile) &&
        GeologyProfile->bGenerateBiomes;

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const int32 VegetationSeed = Chunk.GetGenerationSeeds().Vegetation;

    int32 CountsByType[6] = {0, 0, 0, 0, 0, 0};

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            int32 SurfaceLocalZ = INDEX_NONE;

            for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
            {
                const FCubusBlockVoxel* Voxel =
                    Chunk.GetVoxel(LocalX, LocalY, LocalZ);

                if (
                    Voxel != nullptr &&
                    Voxel->MaterialId > 0 &&
                    !Voxel->IsWater()
                )
                {
                    SurfaceLocalZ = LocalZ;
                    break;
                }
            }

            if (
                SurfaceLocalZ == INDEX_NONE ||
                SurfaceLocalZ >= Cubus::ChunkSize - 1
            )
            {
                continue;
            }

            const FCubusBlockVoxel* SurfaceVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ
            );
            const FCubusBlockVoxel* AboveVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ + 1
            );

            if (
                SurfaceVoxel == nullptr ||
                SurfaceVoxel->IsWater() ||
                AboveVoxel == nullptr ||
                AboveVoxel->MaterialId > 0 ||
                AboveVoxel->IsWater()
            )
            {
                continue;
            }

            const int32 WorldX = BaseX + LocalX;
            const int32 WorldY = BaseY + LocalY;
            const int32 WorldZ = BaseZ + SurfaceLocalZ + 1;

            const float PlacementRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 101)
            );

            int32 TypeId = 0;
            float Density = 0.0f;

            if (bUseConfiguredBiomes)
            {
                if (SurfaceVoxel->MaterialId == GeologyProfile->ForestSurfaceMaterialId)
                {
                    TypeId = PlacementRoll < 0.55f ? 3 : 2;
                    Density = 0.42f;
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->WetlandSurfaceMaterialId)
                {
                    TypeId = 4;
                    Density = 0.36f;
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->RockySurfaceMaterialId)
                {
                    TypeId = 5;
                    Density = 0.08f;
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->PlainsSurfaceMaterialId)
                {
                    TypeId = PlacementRoll < 0.82f ? 1 : 2;
                    Density = 0.22f;
                }
            }
            else
            {
                TypeId = 3;
                Density = 0.025f;
            }

            if (TypeId <= 0 || PlacementRoll > Density)
            {
                continue;
            }

            FCubusVegetationInstance Instance;
            Instance.WorldVoxel = FIntVector(WorldX, WorldY, WorldZ);
            Instance.RotationYaw = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 211)
            ) * 360.0f;
            Instance.Scale = FMath::Lerp(
                0.85f,
                1.15f,
                HashToUnitFloat(
                    HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)
                )
            );
            Instance.TypeId = TypeId;

            Instances.Add(Instance);
            ++CountsByType[TypeId];
        }
    }

    Chunk.SetVegetationInstances(MoveTemp(Instances));

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus vegetation chunk (%d, %d, %d), seed %d: grass %d, shrubs %d, trees %d, reeds %d, alpine %d%s"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        VegetationSeed,
        CountsByType[1],
        CountsByType[2],
        CountsByType[3],
        CountsByType[4],
        CountsByType[5],
        bUseConfiguredBiomes ? TEXT("") : TEXT(" (fallback)")
    );
}

uint32 FCubusBlockVegetationGenerator::HashWorldColumn(
    const int32 WorldX,
    const int32 WorldY,
    const int32 Salt
)
{
    uint32 Hash = static_cast<uint32>(WorldX) * 0x8da6b343u;
    Hash ^= static_cast<uint32>(WorldY) * 0xd8163841u;
    Hash ^= static_cast<uint32>(Salt) * 0xcb1ab31fu;
    Hash ^= Hash >> 13;
    Hash *= 0x85ebca6bu;
    Hash ^= Hash >> 16;
    return Hash;
}

float FCubusBlockVegetationGenerator::HashToUnitFloat(const uint32 Hash)
{
    return static_cast<float>(Hash & 0x00ffffffu) /
        static_cast<float>(0x01000000u);
}
