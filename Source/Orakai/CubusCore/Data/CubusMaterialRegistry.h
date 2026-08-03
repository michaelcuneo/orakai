#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusCore/Data/CubusMaterialDefinition.h"

#include "CubusMaterialRegistry.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UTexture2DArray;

/** Editor-authored collection of every voxel material available to Cubus. */
UCLASS(BlueprintType, meta = (DisplayName = "Cubus Material Registry"))
class ORAKAI_API UCubusMaterialRegistry : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Materials")
    TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density")
    TObjectPtr<UMaterialInterface> DensityMaterial = nullptr;

    /** Slice index is the voxel MaterialId. Slice zero is reserved for air. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityBaseColorArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityNormalArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityOrmArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityHeightArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityMacroColorArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|GPU")
    TObjectPtr<UTexture2DArray> DensityDetailNormalArray = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Materials")
    TArray<FCubusMaterialDefinition> Materials;

    UFUNCTION(BlueprintPure, Category = "Cubus|Materials")
    const FCubusMaterialDefinition& GetMaterialDefinition(int32 MaterialId) const;

    const FCubusMaterialDefinition* FindMaterialDefinition(int32 MaterialId) const;

    UMaterialInterface* ResolveMaterial(int32 MaterialId) const;
    UMaterialInterface* ResolveRuntimeMaterial(int32 MaterialIdOrDensityKey) const;

    /** Returns the one shared texture-array density material instance. */
    UMaterialInterface* ResolveUnifiedDensityRuntimeMaterial() const;

    /** Legacy pair resolver retained for serialized callers during migration. */
    UMaterialInterface* ResolveDensityRuntimeMaterial(
        int32 PrimaryMaterialId,
        int32 SecondaryMaterialId
    ) const;

    bool IsRenderableSolid(int32 MaterialId) const;
    bool OccludesBlockFaces(int32 MaterialId) const;

    /** Builds texture arrays, material data texture and M_CubusDensityPBR. */
    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Materials|Density",
        meta = (DisplayName = "Build Density Material")
    )
    void BuildDensityMaterial();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Materials")
    void ValidateRegistry();

    virtual void PostLoad() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(
        FPropertyChangedEvent& PropertyChangedEvent
    ) override;
#endif

private:
    void RebuildLookupCache() const;
    void RebuildDensityMaterialDataTexture() const;
    void BindDensityGpuResources(UMaterialInstanceDynamic* RuntimeMaterial) const;

    mutable TMap<int32, int32> MaterialIndexById;
    mutable TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterialById;
    mutable TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>> DensityRuntimeMaterialByKey;
    mutable TWeakObjectPtr<UMaterialInstanceDynamic> UnifiedDensityRuntimeMaterial;

    UPROPERTY(Transient)
    mutable TObjectPtr<UTexture2D> DensityMaterialDataTexture = nullptr;

    mutable bool bLookupCacheDirty = true;

    static const FCubusMaterialDefinition InvalidDefinition;
};