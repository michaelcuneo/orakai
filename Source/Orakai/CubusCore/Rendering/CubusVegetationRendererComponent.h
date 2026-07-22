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
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float VoxelSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Collision")
    bool bEnableTreeCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastTreeShadows = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastSmallVegetationShadows = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0.1", Units = "s"))
    float ChangeCheckInterval = 0.5f;

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

    uint32 LastPlacementHash = 0;
    float TimeUntilNextCheck = 0.0f;

    void EnsureInstanceComponents();
    uint32 CalculatePlacementHash() const;

    UInstancedStaticMeshComponent* CreateInstanceComponent(
        FName ComponentName,
        UStaticMesh* Mesh,
        bool bEnableCollision,
        bool bCastShadow
    );

    UInstancedStaticMeshComponent* ResolveComponentForType(int32 TypeId) const;
};