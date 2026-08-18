#pragma once

#include "CoreMinimal.h"

/**
 * Conversion helpers between Cubus canonical voxel coordinates and real-world
 * metric scale.
 *
 * Procedural generation is evaluated in canonical voxel coordinates because
 * edits, chunks and density sampling all share that lattice. Landform design,
 * however, is authored in metres/kilometres. Keeping this conversion in one
 * place prevents every noise field from quietly inventing its own scale.
 */
namespace CubusWorldScale
{
    static constexpr float DefaultVoxelSizeCm = 80.0f;

    FORCEINLINE float MetersPerVoxel(const float VoxelSizeCm)
    {
        return FMath::Max(1.0f, VoxelSizeCm) * 0.01f;
    }

    FORCEINLINE float MetersToVoxels(const float Meters, const float VoxelSizeCm)
    {
        return Meters / MetersPerVoxel(VoxelSizeCm);
    }

    FORCEINLINE float VoxelsToMeters(const float Voxels, const float VoxelSizeCm)
    {
        return Voxels * MetersPerVoxel(VoxelSizeCm);
    }

    /** Per-canonical-voxel noise frequency for a requested real wavelength. */
    FORCEINLINE float FrequencyForWavelengthMeters(const float WavelengthMeters, const float VoxelSizeCm)
    {
        return MetersPerVoxel(VoxelSizeCm) / FMath::Max(1.0f, WavelengthMeters);
    }

    FORCEINLINE float FrequencyForWavelengthKilometres(const float WavelengthKilometres, const float VoxelSizeCm)
    {
        return FrequencyForWavelengthMeters(FMath::Max(0.001f, WavelengthKilometres) * 1000.0f, VoxelSizeCm);
    }
}
