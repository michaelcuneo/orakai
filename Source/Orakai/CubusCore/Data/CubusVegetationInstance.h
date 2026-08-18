#pragma once

#include "CoreMinimal.h"

namespace CubusVegetationBiome
{
    constexpr int32 Plains = 1 << 0;
    constexpr int32 Forest = 1 << 1;
    constexpr int32 Rocky = 1 << 2;
    constexpr int32 Wetland = 1 << 3;
    constexpr int32 All = Plains | Forest | Rocky | Wetland;
}

/**
 * Compact ecological context captured when a vegetation placement is created.
 *
 * Species selection happens later in the world vegetation catalog, so the
 * placement carries the authoritative density-world habitat with it rather
 * than re-sampling terrain during rendering. Unit fields are quantized to one
 * byte; this keeps large near/far vegetation sets cheap while retaining much
 * more precision than species suitability needs.
 */
struct FCubusVegetationHabitatSample
{
    uint8 Moisture = 128;
    uint8 Temperature = 128;
    uint8 SoilDepth = 128;
    uint8 Drainage = 0;
    uint8 RiverInfluence = 0;
    uint8 RockExposure = 0;
    uint8 Exposure = 0;
    uint8 Fertility = 128;
    uint8 ElevationNormalized = 0;
    uint8 TreeLineWeight = 255;
    uint8 SoilSaturation = 0;
    uint8 GroundwaterPotential = 0;
    uint8 SolarExposure = 128;
    uint8 Erosion = 0;
    uint8 ColdAirPooling = 0;
    uint8 Disturbance = 0;
    uint8 CanopyPotential = 0;
    uint8 SoilCoarseness = 128;
    uint8 WaterHoldingCapacity = 128;
    uint8 SlopeDegrees = 0;

    static uint8 QuantizeUnit(const float Value)
    {
        return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Value, 0.0f, 1.0f) * 255.0f), 0, 255));
    }

    static float DecodeUnit(const uint8 Value)
    {
        return static_cast<float>(Value) / 255.0f;
    }

    static uint8 QuantizeSlopeDegrees(const float Value)
    {
        return QuantizeUnit(FMath::Clamp(Value, 0.0f, 90.0f) / 90.0f);
    }

    float GetMoisture() const { return DecodeUnit(Moisture); }
    float GetTemperature() const { return DecodeUnit(Temperature); }
    float GetSoilDepth() const { return DecodeUnit(SoilDepth); }
    float GetDrainage() const { return DecodeUnit(Drainage); }
    float GetRiverInfluence() const { return DecodeUnit(RiverInfluence); }
    float GetRockExposure() const { return DecodeUnit(RockExposure); }
    float GetExposure() const { return DecodeUnit(Exposure); }
    float GetFertility() const { return DecodeUnit(Fertility); }
    float GetElevationNormalized() const { return DecodeUnit(ElevationNormalized) * 1.5f; }
    float GetTreeLineWeight() const { return DecodeUnit(TreeLineWeight); }
    float GetSoilSaturation() const { return DecodeUnit(SoilSaturation); }
    float GetGroundwaterPotential() const { return DecodeUnit(GroundwaterPotential); }
    float GetSolarExposure() const { return DecodeUnit(SolarExposure); }
    float GetErosion() const { return DecodeUnit(Erosion); }
    float GetColdAirPooling() const { return DecodeUnit(ColdAirPooling); }
    float GetDisturbance() const { return DecodeUnit(Disturbance); }
    float GetCanopyPotential() const { return DecodeUnit(CanopyPotential); }
    float GetSoilCoarseness() const { return DecodeUnit(SoilCoarseness); }
    float GetWaterHoldingCapacity() const { return DecodeUnit(WaterHoldingCapacity); }
    float GetSlopeDegrees() const { return DecodeUnit(SlopeDegrees) * 90.0f; }
};

/**
 * Deterministic vegetation placement generated for one terrain column.
 * Rendering is intentionally handled separately from generation.
 */
struct FCubusVegetationInstance
{
    FIntVector WorldVoxel = FIntVector::ZeroValue;
    float RotationYaw = 0.0f;
    float Scale = 1.0f;
    int32 TypeId = 0;
    int32 BiomeMask = CubusVegetationBiome::All;
    FCubusVegetationHabitatSample Habitat;
};

/**
 * Immutable inputs required to generate vegetation directly from world
 * coordinates without constructing a terrain chunk.
 */
struct FCubusVegetationRegion
{
    FIntPoint Minimum = FIntPoint::ZeroValue;
    FIntPoint Maximum = FIntPoint::ZeroValue;
};