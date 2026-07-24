#pragma once

#include "CoreMinimal.h"

/**
 * Stable per-system seeds derived from one authoritative world seed.
 *
 * The internal salts are deliberately fixed. Changing one of them changes
 * that generator's output and must therefore be accompanied by a generation
 * version bump once persisted worlds are introduced.
 */
struct ORAKAI_API FCubusGenerationSeeds
{
    static constexpr uint32 CurrentGenerationVersion = 1;

    int64 World = 1;
    int32 Terrain = 0;
    int32 Rivers = 0;
    int32 Biomes = 0;
    int32 Caves = 0;
    int32 Ores = 0;
    int32 Vegetation = 0;

    static FCubusGenerationSeeds FromWorldSeed(const int64 InWorldSeed)
    {
        FCubusGenerationSeeds Seeds;
        Seeds.World = InWorldSeed;
        Seeds.Terrain = Derive(InWorldSeed, 0x13579BDFu);
        Seeds.Rivers = Derive(InWorldSeed, 0x2468ACE1u);
        Seeds.Biomes = Derive(InWorldSeed, 0x9E3779B9u);
        Seeds.Caves = Derive(InWorldSeed, 0x85EBCA6Bu);
        Seeds.Ores = Derive(InWorldSeed, 0xC2B2AE35u);
        Seeds.Vegetation = Derive(InWorldSeed, 0x27D4EB2Fu);
        return Seeds;
    }

    static int32 DomainOffsetX(const int32 Seed)
    {
        const uint32 Bits = static_cast<uint32>(Seed);
        return static_cast<int32>(Bits & 0x000fffffu) - 524288;
    }

    static int32 DomainOffsetY(const int32 Seed)
    {
        const uint32 Bits = static_cast<uint32>(Seed);
        return static_cast<int32>((Bits >> 10) & 0x000fffffu) - 524288;
    }

    static int32 DomainOffsetZ(const int32 Seed)
    {
        uint32 Bits = static_cast<uint32>(Seed);
        Bits ^= Bits << 13;
        Bits ^= Bits >> 17;
        Bits ^= Bits << 5;
        return static_cast<int32>(Bits & 0x000fffffu) - 524288;
    }

private:
    static int32 Derive(const int64 WorldSeed, const uint32 InternalSalt)
    {
        uint64 Value = static_cast<uint64>(WorldSeed);
        Value ^= static_cast<uint64>(InternalSalt) * 0x9E3779B97F4A7C15ull;
        Value ^= Value >> 30;
        Value *= 0xBF58476D1CE4E5B9ull;
        Value ^= Value >> 27;
        Value *= 0x94D049BB133111EBull;
        Value ^= Value >> 31;

        return static_cast<int32>(
            static_cast<uint32>(Value) ^
            static_cast<uint32>(Value >> 32)
        );
    }
};
