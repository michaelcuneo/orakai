#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

const FCubusMaterialDefinition
    UCubusMaterialRegistry::InvalidDefinition;

namespace CubusMaterialRegistry
{
    void ApplyTextureIfValid(
        UMaterialInstanceDynamic* RuntimeMaterial,
        const FName ParameterName,
        UTexture2D* Texture
    )
    {
        if (IsValid(RuntimeMaterial) && IsValid(Texture))
        {
            RuntimeMaterial->SetTextureParameterValue(
                ParameterName,
                Texture
            );
        }
    }

    void ApplySurface(
        UMaterialInstanceDynamic* RuntimeMaterial,
        const TCHAR* Prefix,
        const FCubusBlockSurfaceTextures& Surface,
        const FCubusBlockSurfaceTextures& Fallback
    )
    {
        const FCubusBlockSurfaceTextures& Resolved =
            Surface.HasAnyTexture()
                ? Surface
                : Fallback;

        const FString PrefixString(Prefix);

        ApplyTextureIfValid(RuntimeMaterial, FName(PrefixString + TEXT("BaseColor")), Resolved.BaseColor.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(PrefixString + TEXT("Normal")), Resolved.Normal.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(PrefixString + TEXT("ORM")), Resolved.ORM.Get());
        ApplyTextureIfValid(RuntimeMaterial, FName(PrefixString + TEXT("Height")), Resolved.Height.Get());
    }
}

const FCubusMaterialDefinition*
UCubusMaterialRegistry::FindMaterialDefinition(
    const int32 MaterialId
) const
{
    if (bLookupCacheDirty)
    {
        RebuildLookupCache();
    }

    const int32* MaterialIndex =
        MaterialIndexById.Find(MaterialId);

    if (
        MaterialIndex == nullptr ||
        !Materials.IsValidIndex(*MaterialIndex)
    )
    {
        return nullptr;
    }

    return &Materials[*MaterialIndex];
}

const FCubusMaterialDefinition&
UCubusMaterialRegistry::GetMaterialDefinition(
    const int32 MaterialId
) const
{
    if (MaterialId < 0 || MaterialId > MAX_uint16)
    {
        return InvalidDefinition;
    }

    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    return Definition != nullptr
        ? *Definition
        : InvalidDefinition;
}

UMaterialInterface* UCubusMaterialRegistry::ResolveMaterial(
    const int32 MaterialId
) const
{
    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    if (
        Definition != nullptr &&
        IsValid(Definition->Material.Get())
    )
    {
        return Definition->Material.Get();
    }

    return DefaultMaterial.Get();
}

UMaterialInterface* UCubusMaterialRegistry::ResolveRuntimeMaterial(
    const int32 MaterialId
) const
{
    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    if (Definition == nullptr || !Definition->UsesPbrTextures())
    {
        return ResolveMaterial(MaterialId);
    }

    if (
        const TWeakObjectPtr<UMaterialInstanceDynamic>* Existing =
            RuntimeMaterialById.Find(MaterialId)
    )
    {
        if (Existing->IsValid())
        {
            return Existing->Get();
        }
    }

    UMaterialInterface* ParentMaterial =
        ResolveMaterial(MaterialId);

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

    CubusMaterialRegistry::ApplySurface(RuntimeMaterial, TEXT("Side"), Definition->SideSurface, Definition->SideSurface);
    CubusMaterialRegistry::ApplySurface(RuntimeMaterial, TEXT("Top"), Definition->TopSurface, Definition->SideSurface);
    CubusMaterialRegistry::ApplySurface(RuntimeMaterial, TEXT("Bottom"), Definition->BottomSurface, Definition->SideSurface);

    RuntimeMaterial->SetScalarParameterValue(TEXT("TextureScale"), FMath::Max(0.01f, Definition->TextureScale));
    RuntimeMaterial->SetScalarParameterValue(TEXT("HeightStrength"), FMath::Max(0.0f, Definition->HeightStrength));
    RuntimeMaterial->SetScalarParameterValue(TEXT("SideTopBlendStart"), FMath::Clamp(Definition->SideTopBlendStart, 0.0f, 1.0f));
    RuntimeMaterial->SetScalarParameterValue(TEXT("SideTopBlendSharpness"), FMath::Max(0.01f, Definition->SideTopBlendSharpness));
    RuntimeMaterial->SetVectorParameterValue(TEXT("Tint"), Definition->Tint);
    RuntimeMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Definition->EmissiveColor);
    RuntimeMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Max(0.0f, Definition->EmissiveStrength));

    RuntimeMaterialById.Add(MaterialId, RuntimeMaterial);
    return RuntimeMaterial;
}

bool UCubusMaterialRegistry::IsRenderableSolid(
    const int32 MaterialId
) const
{
    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    return
        Definition != nullptr &&
        Definition->IsSolid() &&
        Definition->bRenderable;
}

bool UCubusMaterialRegistry::OccludesBlockFaces(
    const int32 MaterialId
) const
{
    if (MaterialId <= 0)
    {
        return false;
    }

    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    return Definition != nullptr
        ? Definition->bOccludesBlockFaces
        : true;
}

void UCubusMaterialRegistry::ValidateRegistry()
{
    if (!IsValid(DefaultMaterial.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus material registry has no valid DefaultMaterial."));
    }

    bLookupCacheDirty = true;
    RuntimeMaterialById.Reset();
    RebuildLookupCache();

    TSet<int32> UsedIds;

    for (const FCubusMaterialDefinition& Definition : Materials)
    {
        if (
            Definition.bRenderable &&
            !IsValid(Definition.Material.Get())
        )
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Renderable Cubus material '%s' using ID %d has no material asset. DefaultMaterial will be used."),
                *Definition.Name.ToString(),
                Definition.MaterialId
            );
        }

        if (UsedIds.Contains(Definition.MaterialId))
        {
            UE_LOG(LogTemp, Error, TEXT("Cubus material registry contains duplicate ID %d."), Definition.MaterialId);
        }

        UsedIds.Add(Definition.MaterialId);

        if (
            Definition.MaterialId == 0 &&
            Definition.State != ECubusMatterState::Empty
        )
        {
            UE_LOG(LogTemp, Error, TEXT("Cubus material ID 0 must use the Empty state."));
        }

        if (
            Definition.State == ECubusMatterState::Empty &&
            Definition.bRenderable
        )
        {
            UE_LOG(LogTemp, Warning, TEXT("Empty material '%s' is marked renderable."), *Definition.Name.ToString());
        }
    }

    if (!UsedIds.Contains(0))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus material registry has no definition for Air using ID 0."));
    }
}

void UCubusMaterialRegistry::PostLoad()
{
    Super::PostLoad();

    bLookupCacheDirty = true;
    RuntimeMaterialById.Reset();
    RebuildLookupCache();
}

void UCubusMaterialRegistry::RebuildLookupCache() const
{
    MaterialIndexById.Reset();
    MaterialIndexById.Reserve(Materials.Num());

    for (
        int32 MaterialIndex = 0;
        MaterialIndex < Materials.Num();
        ++MaterialIndex
    )
    {
        const FCubusMaterialDefinition& Definition =
            Materials[MaterialIndex];

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
    RebuildLookupCache();
}
#endif
