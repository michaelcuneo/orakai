#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusDemIslandGenerator.h"

namespace CubusDemPyramid
{
struct FResult
{
	TArray<float> ReliefM;
	TArray<uint8> ProvinceId;
	int32 Macro128SourceCount = 0;
	int32 Macro64SourceCount = 0;
	int32 Macro32SourceCount = 0;

	bool IsValid(const int32 CellCount) const
	{
		return ReliefM.Num() == CellCount && ProvinceId.Num() == CellCount;
	}
};

/**
 * Compose the production global relief field from physical-scale real DEM tiers.
 *
 * Macro128km provides the dominant long-wavelength mountain/basin structure.
 * Macro64km and Macro32km add progressively smaller real landforms at reduced
 * amplitudes. Each source patch is sampled near its real physical extent; no
 * 1 km LiDAR patch is enlarged into a mountain range.
 */
ORAKAI_API bool Compose(
	const CubusDemIsland::FSettings& Settings,
	const TArray<CubusDemIsland::FIndexedPatch>& IndexEntries,
	FResult& OutResult,
	FString* OutError = nullptr);
} // namespace CubusDemPyramid
