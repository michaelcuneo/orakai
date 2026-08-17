#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainStructure.h"

/** Diagnostics for the nested pre-erosion morphology layered over the macro skeleton. */
struct ORAKAI_API FCubusTerrainMorphologySample
{
    float HeightOffsetMeters = 0.0f;
    float HillMeters = 0.0f;
    float SpurMeters = 0.0f;
    float SwaleMeters = 0.0f;
    float FineMeters = 0.0f;
};

/**
 * Multi-scale deterministic terrain morphology.
 *
 * This does not decide where mountain systems live. FCubusTerrainStructure owns
 * that topology. Morphology supplies the nested relief visible inside a local
 * DEM crop: broad hills, branching/ridged spurs, shallow drainage-ready swales
 * and fine tens-of-metres breakup. All coordinates are physical metres.
 */
class ORAKAI_API FCubusTerrainMorphology
{
public:
    static FCubusTerrainMorphologySample Sample(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainStructureSample& Structure,
        const FCubusTerrainStructureSettings& Settings
    );
};
