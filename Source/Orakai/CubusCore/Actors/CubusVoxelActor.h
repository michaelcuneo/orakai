#pragma once
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusCore/Data/CubusVoxel.h"

#include "CubusVoxelActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * Editor-facing actor used to inspect and test one Cubus block voxel.
 *
 * This is a development and authoring actor. A complete terrain will later
 * use chunk actors rather than one actor per voxel.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Voxel")
)
class ORAKAI_API ACubusVoxelActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusVoxelActor();

    virtual void OnConstruction(
        const FTransform& Transform
    ) override;

    /**
     * Explicitly rebuilds the voxel mesh.
     *
     * Exposed as an editor button through CallInEditor.
     */
    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Rendering"
    )
    void RebuildVoxel();

    /**
     * Removes the generated voxel geometry.
     */
    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Rendering"
    )
    void ClearVoxel();

protected:
    /**
     * The generated voxel mesh.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Components"
    )
    TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

    /**
     * Data represented by this voxel.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel"
    )
    FCubusVoxel Voxel;

    /**
     * Size of the voxel in Unreal units.
     *
     * Unreal commonly treats 100 units as approximately one metre, making
     * the default voxel a convenient visible test object.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel",
        meta = (
            ClampMin = "1.0",
            UIMin = "1.0",
            UIMax = "1000.0",
            Units = "cm"
            )
    )
    float VoxelSize = 100.0f;

    /**
     * Material assigned to the generated mesh section.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Rendering"
    )
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    /**
     * Whether the generated mesh should create collision.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Collision"
    )
    bool bGenerateCollision = false;

    /**
     * Allows automatic reconstruction whenever an exposed property changes.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Editor"
    )
    bool bRebuildAutomatically = true;

    /**
     * Number of vertices in the current generated mesh.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Diagnostics"
    )
    int32 GeneratedVertexCount = 0;

    /**
     * Number of triangles in the current generated mesh.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Diagnostics"
    )
    int32 GeneratedTriangleCount = 0;
};