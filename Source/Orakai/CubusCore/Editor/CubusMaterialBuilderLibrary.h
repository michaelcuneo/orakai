#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubusMaterialBuilderLibrary.generated.h"

class UMaterial;

/**
 * Editor-only utilities for generating Cubus material assets.
 *
 * The graph is assembled in C++ and every FExpressionInput is assigned
 * directly, avoiding the unreliable named-pin Python material API.
 */
UCLASS()
class ORAKAI_API UCubusMaterialBuilderLibrary : public UBlueprint