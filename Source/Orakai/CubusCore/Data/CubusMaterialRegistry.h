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

/**
 * Terrain material library for Cubus. Construction/building materials belong
 * to a separate future asset and are intentionally not represented here.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Cubus Terrain Material Library"))
class ORAKAI_API UCubusMaterialRegistry : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Fallback used only when the generated terrain material is unavailable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Material")
    TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

    /** Shared master material used by all density terrain chunks. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Material")
    TObjectPtr<UMaterialInterface> DensityMaterial = nullptr;

    /** Terrain-wide distribution, clutter and natural-feature rules. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Terrain|Distribution",
        meta = (DisplayName = "Terrain Distribution", ShowOnlyInnerProperties)
    )
    FCubusTerrainSurfaceLayerSettings TerrainSurfaceLayers;

    /** Terrain definitions indexed by stable MaterialId values. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Materials")
    TArray<FCubusMaterialDefinition> Materials;

    /** Generated GPU resources. These are outputs, not authoring inputs. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityBaseColorArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityNormalArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityOrmArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityHeightArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityMacroColorArray = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Generated")
    TObjectPtr<UTexture2DArray> DensityDetailNormalArray = nullptr;

    UFUNCTION(BlueprintPure, Category = "Cubus|Terrain Materials")
    const FCubusMaterialDefinition& GetMaterialDefinition(int32 MaterialId) const;

    const FCubusMaterialDefinition* FindMaterialDefinition(int32 MaterialId) const;

    UMaterialInterface* ResolveMaterial(int32 MaterialId) const;
    UMaterialInterface* ResolveRuntimeMaterial(int32 MaterialIdOrDensityKey) const;
    UMaterialInterface* ResolveUnifiedDensityRuntimeMaterial() const;

    /** Legacy resolver retained for existing serialized callers. */
    UMaterialInterface* ResolveDensityRuntimeMaterial(
        int32 PrimaryMaterialId,
        int32 SecondaryMaterialId
    ) const;

    void SetWeatherMaterialState(
        float Wetness,
        float WetDarkening,
        float WetRoughness
    ) const;

    bool IsRenderableSolid(int32 MaterialId) const;
    bool OccludesBlockFaces(int32 MaterialId) const;

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Terrain Materials",
        meta = (DisplayName = "Build Terrain Material")
    )
    void BuildDensityMaterial();

    /** Transitional editor entrypoint retained until old callers are removed. */
    void BuildWeatherResponsiveMaterials();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Terrain Materials")
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

    mutable TMap<int32, int32> MaterialIndexById;

    /** Hidden compatibility caches used only by the legacy editor rebuild path. */
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
