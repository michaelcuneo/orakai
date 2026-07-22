#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Publishes deterministic Cubus vegetation placements as tagged ISM point
 * sources and can directly spawn transient PVE tree actors for tree points.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (
        BlueprintSpawnableComponent,
       