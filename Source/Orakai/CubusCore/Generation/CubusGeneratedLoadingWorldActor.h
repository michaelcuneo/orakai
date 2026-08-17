#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusGeneratedLoadingWorldActor.generated.h"

/**
 * Runtime BlockWorld used by the existing world-generation screen after the
 * authoritative DEM finishes. It carries the same project asset defaults that
 * the gameplay world normally receives from its placed/Blueprint configuration,
 * so the loader can perform the support and full gameplay chunk passes without
 * leaving the generation world.
 */
UCLASS(NotBlueprintable, Transient)
class ORAKAI_API ACubusGeneratedLoadingWorldActor : public ACubusBlockWorldActor
{
    GENERATED_BODY()

public:
    ACubusGeneratedLoadingWorldActor();
};
