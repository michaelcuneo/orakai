#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "Materials/MaterialInterface.h"

const FCubusMaterialDefinition
    UCubusMaterialRegistry::InvalidDefinition;

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
        FindMaterialDefinition(
            static_cast<uint16>(MaterialId)
        );

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

    /*
     * An occupied voxel must still hide internal terrain faces when its
     * material has not yet been added to the editor registry. Previously an
     * unknown biome or geology material returned false here, causing the top
     * face of the chunk below to be rendered at every vertical chunk boundary.
     *
     * Registered materials retain their explicit occlusion behaviour, so
     * intentionally non-occluding materials such as glass remain supported.
     */
    return Definition != nullptr
        ? Definition->bOccludesBlockFaces
        : true;
}

void UCubusMaterialRegistry::ValidateRegistry()
{

    if (!IsValid(DefaultMaterial.Get()))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Cubus material registry has no valid DefaultMaterial."
            )
        );
    }

    bLookupCacheDirty = true;
    RebuildLookupCache();

    TSet<int32> UsedIds;

    for (
        const FCubusMaterialDefinition& Definition :
        Materials
    )
    {
        if (
            Definition.bRenderable &&
            !IsValid(Definition.Material.Get())
        )
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Renderable Cubus material '%s' using ID %d has no material asset. M_DEFAULT will be used."
                ),
                *Definition.Name.ToString(),
                Definition.MaterialId
            );
        }
        
        if (UsedIds.Contains(Definition.MaterialId))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Cubus material registry contains duplicate ID %d."
                ),
                Definition.MaterialId
            );
        }

        UsedIds.Add(Definition.MaterialId);

        if (
            Definition.MaterialId == 0 &&
            Definition.State != ECubusMatterState::Empty
        )
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Cubus material ID 0 must use the Empty state."
                )
            );
        }

        if (
            Definition.State == ECubusMatterState::Empty &&
            Definition.bRenderable
        )
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Empty material '%s' is marked renderable."
                ),
                *Definition.Name.ToString()
            );
        }
    }

    if (!UsedIds.Contains(0))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Cubus material registry has no definition for Air using ID 0."
            )
        );
    }
}

void UCubusMaterialRegistry::PostLoad()
{
    Super::PostLoad();

    bLookupCacheDirty = true;
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

        /*
         * Preserve the first definition when duplicate IDs exist.
         * ValidateRegistry() will report the duplicate separately.
         */
        if (!MaterialIndexById.Contains(Definition.MaterialId))
        {
            MaterialIndexById.Add(
                Definition.MaterialId,
                MaterialIndex
            );
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
    RebuildLookupCache();
}
#endif