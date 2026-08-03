#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Float16Color.h"

const FCubusMaterialDefinition UCubusMaterialRegistry::InvalidDefinition;

namespace CubusMaterialRegistry
{
    constexpr int32 MaterialDataRowCount = 4;

    void ApplyTextureIfValid(
        UMaterialInstanceDynamic* RuntimeMaterial,
        const FName ParameterName,
        UTexture* Texture
    )
    {
        if (IsValid(RuntimeMaterial) && IsValid(Texture))
        {
            RuntimeMaterial->SetTextureParameterValue(ParameterName, Texture);
        }
    }

    void ApplyBlockSurface(
        UMaterialInstanceDynamic* RuntimeMaterial,
        const TCHAR* Prefix,
        const FCubusBlockSurfaceTextures& Surface,
        const FCubusBlockSurfaceTextures& Fallback
    )
    {
        const FCubusBlockSurfaceTextures& Resolved =
            Surface.HasAnyTexture() ? Surface : Fallback;
        const FString P(Prefix);

        ApplyTextureIfValid(RuntimeMaterial, FName(P + TEXT("BaseColor")), Resolved.BaseColor.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(P + TEXT("Normal")), Resolved.Normal.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(P + TEXT("ORM")), Resolved.ORM.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(P + TEXT("Height")), Resolved.Height.Get());
    }

    FFloat16Color MakeDataColor(
        const float R,
        const float G,
        const float B,
        const float A
    )
    {
        return FFloat16Color(FLinearColor(R, G, B, A));
    }
}

const FCubusMaterialDefinition* UCubusMaterialRegistry::FindMaterialDefinition(
    const int32 MaterialId
) const
{
    if (bLookupCacheDirty)
    {
        RebuildLookupCache();
    }

    const int32* MaterialIndex = MaterialIndexById.Find(MaterialId);
    if (MaterialIndex == nullptr || !Materials.IsValidIndex(*MaterialIndex))
    {
        return nullptr;
    }

    return &Materials[*MaterialIndex];
}

const FCubusMaterialDefinition& UCubusMaterialRegistry::GetMaterialDefinition(
    const int32 MaterialId
) const
{
    if (MaterialId < 0 || MaterialId > MAX_uint16)
    {
        return InvalidDefinition;
    }

    const FCubusMaterialDefinition* Definition = FindMaterialDefinition(MaterialId);
    return Definition != nullptr ? *Definition : InvalidDefinition;
}

UMaterialInterface* UCubusMaterialRegistry::ResolveMaterial(
    const int32 MaterialId
) const
{
    const FCubusMaterialDefinition* Definition = FindMaterialDefinition(MaterialId);
    if (Definition != nullptr && IsValid(Definition->Material.Get()))
    {
        return Definition->Material.Get();
    }

    return DefaultMaterial.Get();
}

UMaterialInterface* UCubusMaterialRegistry::ResolveRuntimeMaterial(
    const int32 MaterialIdOrDensityKey
) const
{
    if (MaterialIdOrDensityKey == FCubusDensityMesher::UnifiedDensityMaterialKey)
    {
        return ResolveUnifiedDensityRuntimeMaterial();
    }

    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialIdOrDensityKey);

    if (Definition == nullptr || !Definition->UsesPbrTextures())
    {
        return ResolveMaterial(MaterialIdOrDensityKey);
    }

    if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Existing =
        RuntimeMaterialById.Find(MaterialIdOrDensityKey))
    {
        if (Existing->IsValid())
        {
            return Existing->Get();
        }
    }

    UMaterialInterface* ParentMaterial = ResolveMaterial(MaterialIdOrDensityKey);
    if (!IsValid(ParentMaterial))
    {
        return nullptr;
    }

    UMaterialInstanceDynamic* RuntimeMaterial =
        UMaterialInstanceDynamic::Create(
            ParentMaterial,
            const_cast<UCubusMaterialRegistry*>(this)
        );

    if (!IsValid(RuntimeMaterial))
    {
        return ParentMaterial;
    }

    CubusMaterialRegistry::ApplyBlockSurface(
        RuntimeMaterial,
        TEXT("Side"),
        Definition->SideSurface,
        Definition->SideSurface
    );
    CubusMaterialRegistry::ApplyBlockSurface(
        RuntimeMaterial,
        TEXT("Top"),
        Definition->TopSurface,
        Definition->SideSurface
    );
    CubusMaterialRegistry::ApplyBlockSurface(
        RuntimeMaterial,
        TEXT("Bottom"),
        Definition->BottomSurface,
        Definition->SideSurface
    );

    RuntimeMaterial->SetScalarParameterValue(TEXT("TextureScale"), FMath::Max(0.01f, Definition->TextureScale));
    RuntimeMaterial->SetScalarParameterValue(TEXT("HeightStrength"), FMath::Max(0.0f, Definition->HeightStrength));
    RuntimeMaterial->SetScalarParameterValue(TEXT("SideTopBlendStart"), FMath::Clamp(Definition->SideTopBlendStart, 0.0f, 1.0f));
    RuntimeMaterial->SetScalarParameterValue(TEXT("SideTopBlendSharpness"), FMath::Max(0.01f, Definition->SideTopBlendSharpness));
    RuntimeMaterial->SetVectorParameterValue(TEXT("Tint"), Definition->Tint);
    RuntimeMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Definition->EmissiveColor);
    RuntimeMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Max(0.0f, Definition->EmissiveStrength));

    RuntimeMaterialById.Add(MaterialIdOrDensityKey, RuntimeMaterial);
    return RuntimeMaterial;
}

void UCubusMaterialRegistry::RebuildDensityMaterialDataTexture() const
{
    int32 MaximumMaterialId = 1;
    for (const FCubusMaterialDefinition& Definition : Materials)
    {
        MaximumMaterialId = FMath::Max(
            MaximumMaterialId,
            FMath::Clamp(
                Definition.MaterialId,
                0,
                FCubusDensityMesher::MaximumDensityMaterialId
            )
        );
    }

    const int32 Width = MaximumMaterialId + 1;
    UTexture2D* DataTexture = UTexture2D::CreateTransient(
        Width,
        CubusMaterialRegistry::MaterialDataRowCount,
        PF_FloatRGBA,
        TEXT("CubusDensityMaterialData")
    );

    if (!IsValid(DataTexture) || DataTexture->GetPlatformData() == nullptr ||
        DataTexture->GetPlatformData()->Mips.IsEmpty())
    {
        DensityMaterialDataTexture = nullptr;
        return;
    }

    DataTexture->SRGB = false;
    DataTexture->Filter = TF_Nearest;
    DataTexture->AddressX = TA_Clamp;
    DataTexture->AddressY = TA_Clamp;
    DataTexture->NeverStream = true;

    TArray<FFloat16Color> Pixels;
    Pixels.SetNum(Width * CubusMaterialRegistry::MaterialDataRowCount);

    for (int32 MaterialId = 0; MaterialId < Width; ++MaterialId)
    {
        Pixels[MaterialId] = CubusMaterialRegistry::MakeDataColor(1.0f, 1.0f, 1.0f, 1.0f);
        Pixels[Width + MaterialId] = CubusMaterialRegistry::MakeDataColor(0.01f, 6.0f, 0.35f, 4.0f);
        Pixels[Width * 2 + MaterialId] = CubusMaterialRegistry::MakeDataColor(0.0005f, 0.0f, 0.08f, 0.0f);
        Pixels[Width * 3 + MaterialId] = CubusMaterialRegistry::MakeDataColor(0.0f, 0.0f, 0.0f, 0.0f);
    }

    for (const FCubusMaterialDefinition& Definition : Materials)
    {
        if (Definition.MaterialId < 0 || Definition.MaterialId >= Width)
        {
            continue;
        }

        const int32 Id = Definition.MaterialId;
        const FCubusDensitySurfaceTextures& Surface = Definition.DensitySurface;

        Pixels[Id] = FFloat16Color(Surface.Tint);
        Pixels[Width + Id] = CubusMaterialRegistry::MakeDataColor(
            FMath::Max(0.0001f, Surface.WorldScale),
            FMath::Max(0.1f, Surface.TriplanarSharpness),
            FMath::Max(0.0f, Surface.HeightStrength),
            FMath::Max(0.01f, Surface.BlendContrast)
        );
        Pixels[Width * 2 + Id] = CubusMaterialRegistry::MakeDataColor(
            FMath::Max(0.000001f, Surface.MacroScale),
            FMath::Clamp(Surface.MacroStrength, 0.0f, 1.0f),
            FMath::Max(0.0001f, Surface.DetailScale),
            FMath::Max(0.0f, Surface.DetailNormalStrength)
        );
        Pixels[Width * 3 + Id] = CubusMaterialRegistry::MakeDataColor(
            Definition.EmissiveColor.R,
            Definition.EmissiveColor.G,
            Definition.EmissiveColor.B,
            FMath::Max(0.0f, Definition.EmissiveStrength)
        );
    }

    FTexture2DMipMap& Mip = DataTexture->GetPlatformData()->Mips[0];
    void* Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
    Destination = Mip.BulkData.Realloc(Pixels.Num() * sizeof(FFloat16Color));
    FMemory::Memcpy(
        Destination,
        Pixels.GetData(),
        Pixels.Num() * sizeof(FFloat16Color)
    );
    Mip.BulkData.Unlock();
    DataTexture->UpdateResource();

    DensityMaterialDataTexture = DataTexture;
}

void UCubusMaterialRegistry::BindDensityGpuResources(
    UMaterialInstanceDynamic* RuntimeMaterial
) const
{
    if (!IsValid(RuntimeMaterial))
    {
        return;
    }

    if (!IsValid(DensityMaterialDataTexture.Get()))
    {
        RebuildDensityMaterialDataTexture();
    }

    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityBaseColorArray"), DensityBaseColorArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityNormalArray"), DensityNormalArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityORMArray"), DensityOrmArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityHeightArray"), DensityHeightArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityMacroColorArray"), DensityMacroColorArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityDetailNormalArray"), DensityDetailNormalArray.Get());
    CubusMaterialRegistry::ApplyTextureIfValid(RuntimeMaterial, TEXT("DensityMaterialData"), DensityMaterialDataTexture.Get());

    const float TableWidth = IsValid(DensityMaterialDataTexture.Get())
        ? static_cast<float>(DensityMaterialDataTexture->GetSizeX())
        : 1.0f;
    RuntimeMaterial->SetScalarParameterValue(TEXT("DensityMaterialTableWidth"), TableWidth);
    RuntimeMaterial->SetScalarParameterValue(
        TEXT("DensityMaterialIdPackingBase"),
        static_cast<float>(FCubusDensityMesher::MaterialIdPackingBase)
    );
}

UMaterialInterface* UCubusMaterialRegistry::ResolveUnifiedDensityRuntimeMaterial() const
{
    if (UnifiedDensityRuntimeMaterial.IsValid())
    {
        return UnifiedDensityRuntimeMaterial.Get();
    }

    UMaterialInterface* ParentMaterial = DensityMaterial.Get();
    if (!IsValid(ParentMaterial))
    {
        return DefaultMaterial.Get();
    }

    UMaterialInstanceDynamic* RuntimeMaterial =
        UMaterialInstanceDynamic::Create(
            ParentMaterial,
            const_cast<UCubusMaterialRegistry*>(this)
        );

    if (!IsValid(RuntimeMaterial))
    {
        return ParentMaterial;
    }

    BindDensityGpuResources(RuntimeMaterial);
    UnifiedDensityRuntimeMaterial = RuntimeMaterial;
    return RuntimeMaterial;
}

UMaterialInterface* UCubusMaterialRegistry::ResolveDensityRuntimeMaterial(
    const int32 PrimaryMaterialId,
    const int32 SecondaryMaterialId
) const
{
    return ResolveUnifiedDensityRuntimeMaterial();
}

bool UCubusMaterialRegistry::IsRenderableSolid(const int32 MaterialId) const
{
    const FCubusMaterialDefinition* Definition = FindMaterialDefinition(MaterialId);
    return Definition != nullptr && Definition->IsSolid() && Definition->bRenderable;
}

bool UCubusMaterialRegistry::OccludesBlockFaces(const int32 MaterialId) const
{
    if (MaterialId <= 0)
    {
        return false;
    }

    const FCubusMaterialDefinition* Definition = FindMaterialDefinition(MaterialId);
    return Definition != nullptr ? Definition->bOccludesBlockFaces : true;
}

void UCubusMaterialRegistry::ValidateRegistry()
{
    if (!IsValid(DefaultMaterial.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus material registry has no valid DefaultMaterial."));
    }

    if (!IsValid(DensityMaterial.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus material registry has no DensityMaterial."));
    }

    bLookupCacheDirty = true;
    RuntimeMaterialById.Reset();
    DensityRuntimeMaterialByKey.Reset();
    UnifiedDensityRuntimeMaterial.Reset();
    DensityMaterialDataTexture = nullptr;
    RebuildLookupCache();

    TSet<int32> UsedIds;
    for (const FCubusMaterialDefinition& Definition : Materials)
    {
        if (Definition.MaterialId > FCubusDensityMesher::MaximumDensityMaterialId &&
            Definition.bRenderable && Definition.IsSolid())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Density material '%s' uses ID %d. Unified density rendering supports IDs 1-%d."),
                *Definition.Name.ToString(),
                Definition.MaterialId,
                FCubusDensityMesher::MaximumDensityMaterialId
            );
        }

        if (Definition.bRenderable && !IsValid(Definition.Material.Get()))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Renderable Cubus material '%s' using ID %d has no block material asset."),
                *Definition.Name.ToString(),
                Definition.MaterialId
            );
        }

        if (Definition.bRenderable && Definition.IsSolid() && !Definition.UsesDensityTextures())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Solid Cubus material '%s' using ID %d has no density surface textures."),
                *Definition.Name.ToString(),
                Definition.MaterialId
            );
        }

        if (UsedIds.Contains(Definition.MaterialId))
        {
            UE_LOG(LogTemp, Error, TEXT("Cubus material registry contains duplicate ID %d."), Definition.MaterialId);
        }
        UsedIds.Add(Definition.MaterialId);

        if (Definition.MaterialId == 0 && Definition.State != ECubusMatterState::Empty)
        {
            UE_LOG(LogTemp, Error, TEXT("Cubus material ID 0 must use the Empty state."));
        }

        if (Definition.State == ECubusMatterState::Empty && Definition.bRenderable)
        {
            UE_LOG(LogTemp, Warning, TEXT("Empty material '%s' is marked renderable."), *Definition.Name.ToString());
        }
    }

    if (!UsedIds.Contains(0))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus material registry has no definition for Air using ID 0."));
    }

    RebuildDensityMaterialDataTexture();
}

void UCubusMaterialRegistry::PostLoad()
{
    Super::PostLoad();
    bLookupCacheDirty = true;
    RuntimeMaterialById.Reset();
    DensityRuntimeMaterialByKey.Reset();
    UnifiedDensityRuntimeMaterial.Reset();
    DensityMaterialDataTexture = nullptr;
    RebuildLookupCache();
}

void UCubusMaterialRegistry::RebuildLookupCache() const
{
    MaterialIndexById.Reset();
    MaterialIndexById.Reserve(Materials.Num());

    for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
    {
        const FCubusMaterialDefinition& Definition = Materials[MaterialIndex];
        if (!MaterialIndexById.Contains(Definition.MaterialId))
        {
            MaterialIndexById.Add(Definition.MaterialId, MaterialIndex);
        }
    }

    bLookupCacheDirty = false;
}

#if WITH_EDITOR
void UCubusMaterialRegistry::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent
)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    bLookupCacheDirty = true;
    RuntimeMaterialById.Reset();
    DensityRuntimeMaterialByKey.Reset();
    UnifiedDensityRuntimeMaterial.Reset();
    DensityMaterialDataTexture = nullptr;
    RebuildLookupCache();
}
#endif
