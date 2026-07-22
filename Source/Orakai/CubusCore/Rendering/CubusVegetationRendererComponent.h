#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Renders deterministic chunk vegetation records using one ISM component per
 * vegetation type. Assigned mesh assets may use Nanite; Nanite is configured
 * on the UStaticMesh asset rather than on the component at runtime.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (BlueprintSpawnableComponent, DisplayName = "Cubus Vegetation Renderer")
)
class ORAKAI_API UCubusVegetationRendererComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCubusVegetationRendererComponent();

    virtual void OnRegister() override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearVegetation();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Meshes")
    TObjectPtr<UStaticMesh> GrassMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Meshes")
    TObjectPtr<UStaticMesh> ShrubMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Meshes")
    TObjectPtr<UStaticMesh> TreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Meshes")
    TObjectPtr<UStaticMesh> ReedsMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Meshes")
    TObjectPtr<UStaticMesh> AlpineMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Collision")
    bool bEnableTreeCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastTreeShadows = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastSmallVegetationShadows = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 RenderedInstanceCount = 0;

private:
    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GrassInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ShrubInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> TreeInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ReedsInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> AlpineInstances = nullptr;

    void EnsureInstanceComponents();

    UInstancedStaticMeshComponent* CreateInstanceComponent(
        FName ComponentName,
        UStaticMesh* Mesh,
        bool bEnableCollision,
        bool bCastShadow
    );

    UInstancedStaticMeshComponent* ResolveComponentForType(int32 TypeId) const;
};