#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusCore/Data/CubusMaterialDefinition.h"
#include "CubusCore/Data/CubusTerrainSurfaceLayers.h"

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

    /**
     * Global layered-surface controls for the unified density material.
     * Edit these directly on OrakaiMaterialLibrary, then press Build Density
     * Material to rebuild the generated master material if its graph changed.
     * Runtime scalar changes are applied whenever the density MID is recreated.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Materials|Density|Surface Layers", meta = (ShowOnlyInnerProperties))
    FCubusTerrainSurfaceLayerSettings TerrainSurfaceLayers;

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

    /** Updates shared terrain material instances without rebuilding chunks. */
    void SetWeatherMaterialState(
        float Wetness,
        float WetDarkening,
        float WetRoughness
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

    /** Rebuilds both Cubus master materials with live weather response. */
    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Materials|Weather",
        meta = (DisplayName = "Build Weather Responsive Materials")
    )
    void BuildWeatherResponsiveMaterials();

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
    void ApplyWeatherParameters(UMaterialInstanceDynamic* RuntimeMaterial) const;
    void ApplyTerrainSurfaceLayerParameters(UMaterialInstanceDynamic* RuntimeMaterial) const;

    mutable TMap<int32, int32> MaterialIndexById;
    mutable TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterialById;
    mutable TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>> DensityRuntimeMaterialByKey;
    mutable TWeakObjectPtr<UMaterialInstanceDynamic> UnifiedDensityRuntimeMaterial;

    mutable float WeatherWetness = 0.0f;
    mutable float WeatherWetDarkening = 0.65f;
    mutable float WeatherWetRoughness = 0.12f;

    UPROPERTY(Transient)
    mutable TObjectPtr<UTexture2D> DensityMaterialDataTexture = nullptr;

    mutable bool bLookupCacheDirty = true;

    static const FCubusMaterialDefinition InvalidDefinition;
};
