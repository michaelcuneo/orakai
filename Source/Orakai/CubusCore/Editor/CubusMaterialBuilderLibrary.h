#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubusMaterialBuilderLibrary.generated.h"

class UMaterial;

/** Editor-only utilities for generating Cubus material assets. */
UCLASS()
class ORAKAI_API UCubusMaterialBuilderLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Creates or completely rebuilds M_CubusBlockPBR. */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Editor|Materials",
        meta = (DevelopmentOnly)
    )
    static UMaterial* BuildCubusBlockPbrMaterial();

    /**
     * Creates or completely rebuilds M_CubusDensityPBR.
     *
     * The generated material samples MaterialId-indexed texture arrays and
     * blends four locally dominant density materials using packed UV0 IDs and
     * vertex-colour RGBA weights. All density sections share this one master
     * material and one registry-owned runtime instance.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Editor|Materials",
        meta = (DevelopmentOnly)
    )
    static UMaterial* BuildCubusDensityPbrMaterial();
};