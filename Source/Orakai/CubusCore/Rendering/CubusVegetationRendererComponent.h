#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;

/**
 * Renders deterministic Cubus vegetation placements and streams the expensive
 * instanced skinned tree components independently