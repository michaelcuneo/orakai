#include "CubusCore/Data/CubusMaterialRegistry.h"

const FCubusMaterialDefinition
    UCubusMaterialRegistry::InvalidDefinition;

const FCubusMaterialDefinition*
UCubusMaterialRegistry::FindMaterialDefinition(
    const int32 MaterialId
) const
{
    for (
        const FCubusMaterialDefinition& Definition :
        Materials
    )
    {
        if (Definition.MaterialId == MaterialId)
        {
            return &Definition;
        }
    }

    return nullptr;
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
    const FCubusMaterialDefinition* Definition =
        FindMaterialDefinition(MaterialId);

    return
        Definition != nullptr &&
        Definition->bOccludesBlockFaces;
}

void UCubusMaterialRegistry::ValidateRegistry()
{
    TSet<int32> UsedIds;

    for (
        const FCubusMaterialDefinition& Definition :
        Materials
    )
    {
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